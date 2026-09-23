"""Which items a concept batch covers ("subjects"); standard library only.

A subject is what one concept image shows: one catalog item, or a whole armour set (any part of
an armour set selects the set, rendered and drawn together), or one of the art study's grouped
rework targets (the spell books that share one texture: one concept for the whole group).

Selections (combined as a union, in this order, duplicates dropped):
  --study-top N   the first N rows of the art study's ranked rework list (baseline.json,
                  rework_candidates), whose model files are mapped to catalog keys
  --keys A,B      catalog keys, e.g. 0-19,6-1
  --family F      every item of a catalog family, basic to rare (family_rank)
and then filtered by --tier (T1-T3, T2, 2-4, T1,T5).
"""

from pathlib import Path
import json
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CATALOG_FILE = ROOT / 'assets-work' / 'Items' / 'catalog.json'
BASELINE_FILE = ROOT / 'assets-work' / 'Items' / 'study' / 'baseline.json'
DATA_PREFIX = 'src/bin/Data/'
ROLE_ITEM = 'item'
ARMOUR_SET_GROUP = 8
TIER_MIN, TIER_MAX = 1, 7

KIND_ITEM = 'item'
KIND_ARMOUR_SET = 'armour-set'
KIND_SHARED_GROUP = 'shared-group'

# Catalog family -> prompt family (concept_prompt.md "## family <name>").
PROMPT_FAMILIES = {
    'wings-1': 'wings', 'wings-2': 'wings', 'wings-3': 'wings', 'wings-mini': 'wings',
}
FAMILY_LABELS = {
    'swords': 'sword', 'axes': 'axe', 'maces': 'mace', 'spears': 'spear', 'bows': 'bow',
    'staffs': 'staff', 'shields': 'shield', 'wings': 'pair of wings', 'armour-set': 'armour set',
    'skill-books': 'spell book', 'ammunition': 'quiver of ammunition', 'jewels': 'jewel',
    'jewelry': 'ring or pendant', 'pets': 'pet', 'potions': 'potion', 'socket-seeds': 'seed',
    'helms': 'helm', 'armours': 'body armour', 'pants': 'pair of armour pants',
    'gloves': 'pair of gloves', 'boots': 'pair of boots',
}
KEY_PATTERN = re.compile(r'^\d+-\d+$')
TIER_PATTERN = re.compile(r'^[Tt]?(\d)(?:-[Tt]?(\d))?$')


class SelectionError(ValueError):
    pass


def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def load_catalog(path=CATALOG_FILE):
    return load_json(path)['items']


def parse_keys(text):
    keys = [part.strip() for part in text.split(',') if part.strip()]
    bad = [key for key in keys if not KEY_PATTERN.match(key)]
    if bad:
        raise SelectionError(f'not an item key <group>-<index>: {", ".join(bad)}')
    return keys


def parse_tiers(text):
    """'T1-T3' / '2' / 'T1,T5' / '2-4' -> set of tier numbers."""
    tiers = set()
    for part in (p.strip() for p in text.split(',') if p.strip()):
        match = TIER_PATTERN.match(part)
        if not match:
            raise SelectionError(f'not a tier or tier range: {part!r} (use T1-T3, T2 or 1-3)')
        low = int(match.group(1))
        high = int(match.group(2) or low)
        if not TIER_MIN <= low <= high <= TIER_MAX:
            raise SelectionError(f'tiers run from T{TIER_MIN} to T{TIER_MAX}: {part!r}')
        tiers.update(range(low, high + 1))
    return tiers


def model_index(items):
    """'Item/Sword01.bmd' (lower case) -> item keys whose own model it is."""
    index = {}
    for key, entry in items.items():
        for model in entry['models']:
            if model['role'] == ROLE_ITEM:
                name = model['bmd'].removeprefix(DATA_PREFIX).lower()
                index.setdefault(name, []).append(key)
    return index


def item_model(entry):
    return next(model['bmd'] for model in entry['models'] if model['role'] == ROLE_ITEM)


def sort_key(key):
    group, index = key.split('-')
    return int(group), int(index)


def armour_set_subject(items, entry):
    armour_set = entry['armour_set']
    parts = [key for key in armour_set['parts'] if key in items]
    lead = next((key for key in parts if sort_key(key)[0] == ARMOUR_SET_GROUP), parts[0])
    return {
        'key': lead, 'kind': KIND_ARMOUR_SET, 'name': f'{armour_set["name"]} armour set',
        'family': 'armour-set', 'tier': items[lead]['tier']['value'], 'keys': parts,
        'models': [item_model(items[key]) for key in parts],
    }


def item_subject(items, key):
    entry = items.get(key)
    if entry is None:
        raise SelectionError(f'{key} is not in the catalog (no model)')
    if entry.get('armour_set'):
        return armour_set_subject(items, entry)
    family = PROMPT_FAMILIES.get(entry['family'], entry['family'])
    return {
        'key': key, 'kind': KIND_ITEM, 'name': entry['name'] or key, 'family': family,
        'tier': entry['tier']['value'], 'keys': [key], 'models': [item_model(entry)],
    }


def study_subjects(items, baseline, count):
    """The first `count` ranked rework targets of the art study, as subjects."""
    index = model_index(items)
    subjects = []
    for candidate in sorted(baseline['rework_candidates'], key=lambda c: c['rank'])[:count]:
        keys = [key for model in candidate['models'] for key in index.get(model.lower(), [])]
        if not keys:
            raise SelectionError(f'study target {candidate["target"]} maps to no catalog item')
        subject = item_subject(items, keys[0])
        if subject['kind'] == KIND_ITEM and len(keys) > 1:
            subject = dict(subject, kind=KIND_SHARED_GROUP, keys=sorted(set(keys), key=sort_key),
                           name=f'{subject["name"]} (and {len(set(keys)) - 1} more sharing its texture)')
        subjects.append(dict(subject, study_rank=candidate['rank']))
    return subjects


def family_keys(items, family):
    keys = [key for key, entry in items.items() if entry['family'] == family]
    if not keys:
        known = sorted({entry['family'] for entry in items.values()})
        raise SelectionError(f'unknown family {family!r}; families: {", ".join(known)}')
    return sorted(keys, key=lambda key: (items[key]['tier']['family_rank'], sort_key(key)))


def select(items, keys=(), families=(), tiers=None, study_top=0, baseline_path=BASELINE_FILE):
    """Subjects of a selection, in selection order, one per subject key."""
    if not (keys or families or study_top):
        raise SelectionError('select items with --keys, --family or --study-top')
    subjects = study_subjects(items, load_json(baseline_path), study_top) if study_top else []
    wanted = list(keys) + [key for family in families for key in family_keys(items, family)]
    subjects += [item_subject(items, key) for key in wanted]
    unique = {}
    for subject in subjects:
        unique.setdefault(subject['key'], subject)
    chosen = [s for s in unique.values() if tiers is None or s['tier'] in tiers]
    if not chosen:
        raise SelectionError('the selection is empty (check --tier)')
    return chosen


def family_label(family):
    return FAMILY_LABELS.get(family, family.replace('-', ' '))
