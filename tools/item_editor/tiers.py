"""Item families, badges and tiers ("basic to rare"); standard library only.

This is the one place the rule lives. build_item_catalog.py writes its result into
assets-work/Items/catalog.json, so the in-client editor and Codex read the same order; the rule is
also written out in assets-work/Items/README.md ("Tiers") for anyone who has to mirror it.

The rule, per item of the catalog:

1. family: FAMILY_KEYS (explicit item keys, checked first) else GROUP_FAMILIES by item group.
2. tier_score: OpenMU DropLevel when the OpenMU export has the item, else the client table's
   Level (ITEM_ATTRIBUTE::Level, the base drop level).
3. drops: OpenMU DropsFromMonsters when known, else true (the client table cannot tell).
4. badges (tie-breakers, in this order): socket item, set-capable (ancient), 380 option,
   excellent-capable, maximum item level.
5. family order: dropping items first, then items that never drop; inside each part by
   tier_score, then the badges (an item with a badge sorts after one without; a higher maximum
   level sorts later), then group and index. family_rank is the 1-based position in that order.
6. tier T1..T7: the percentile rank of tier_score inside the family, cut into seven equal bands.
   The reference scores are those of the family's dropping items (all of the family's items when
   none of them drops); with n reference scores, `below` of them lower than the item's score and
   `equal` of them equal to it: tier = 1 + floor(7 * (below + equal / 2) / n), at most 7. Equal
   scores share a tier, and a family whose items all have the same score sits in the middle (T4)
   instead of being called basic. Items that never drop get the tier their score would have among
   the dropping items, but still sort after them (rule 5).
7. owner override: assets-work/Items/tiers.json {"<group>-<index>": {"tier": 1..7, "note": "..."}}
   replaces the computed tier (the order of rule 5 is unchanged) and is marked source "owner".
"""

import json
from pathlib import Path

TIER_COUNT = 7
MIN_TIER = 1
OVERRIDE_FIELDS = {'tier', 'note'}
SOURCE_COMPUTED = 'computed'
SOURCE_OWNER = 'owner'
SCORE_OPENMU = 'openmu-drop-level'
SCORE_CLIENT = 'client-level'


def _keys(group, *indexes):
    """Item keys of one group; an index may be a range."""
    keys = []
    for index in indexes:
        keys += [f'{group}-{i}' for i in (index if isinstance(index, range) else (index,))]
    return keys


GROUP_FAMILIES = {
    0: 'swords', 1: 'axes', 2: 'maces', 3: 'spears', 4: 'bows', 5: 'staffs', 6: 'shields',
    7: 'helms', 8: 'armours', 9: 'pants', 10: 'gloves', 11: 'boots',
    12: 'misc', 13: 'helpers', 14: 'potions', 15: 'skill-books',
}

# Families that cut across groups or split one. Each key belongs to one family at most.
FAMILY_KEYS = {
    'ammunition': _keys(4, 7, 15),
    'wings-1': _keys(12, 0, 1, 2, 41),
    'wings-2': _keys(12, 3, 4, 5, 6, 42, 49) + _keys(13, 30),
    'wings-3': _keys(12, range(36, 41), 43, 50),
    'wings-mini': _keys(12, range(130, 136)),
    'jewels': (_keys(12, 15, 30, 31, range(136, 144))
               + _keys(14, 13, 14, 16, 22, 31, 41, 42, 43, 44, 160, 161)),
    'skill-books': _keys(12, range(7, 15), range(16, 20), range(21, 25), 35, range(44, 49)),
    'socket-seeds': _keys(12, range(60, 130)),
    'pets': _keys(13, range(0, 6), 31, 37, 64, 65, 67, 80, 106, 123),
    'jewelry': _keys(13, 8, 9, 10, 12, 13, 20, range(21, 29), 38, 39, 40, 41, 42, 68, 76, 107,
                     range(109, 116), 122),
}
KEY_FAMILY = {key: family for family, keys in FAMILY_KEYS.items() for key in keys}

# Families whose items can carry excellent options when no OpenMU export says otherwise.
EXCELLENT_FAMILIES_WITHOUT_OPENMU = {
    'swords', 'axes', 'maces', 'spears', 'bows', 'staffs', 'shields',
    'helms', 'armours', 'pants', 'gloves', 'boots',
}


class TierError(ValueError):
    """tiers.json is malformed."""


def family_of(group, index):
    key = f'{group}-{index}'
    return KEY_FAMILY.get(key) or GROUP_FAMILIES[group]


def load_overrides(path):
    """tiers.json -> {key: {'tier': int, 'note': str}}; a missing file means no overrides."""
    path = Path(path)
    if not path.exists():
        return {}
    data = json.loads(path.read_text(encoding='utf-8'))
    return validate_overrides(data)


def validate_overrides(data, known_keys=None):
    if not isinstance(data, dict):
        raise TierError('tiers.json must be an object keyed by "<group>-<index>"')
    overrides = {}
    for key, entry in data.items():
        parts = key.split('-')
        if len(parts) != 2 or not all(part.isdigit() for part in parts):
            raise TierError(f'tiers.json: {key!r} is not "<group>-<index>"')
        if known_keys is not None and key not in known_keys:
            raise TierError(f'tiers.json: {key} is not an item of the catalog')
        if not isinstance(entry, dict) or 'tier' not in entry or set(entry) - OVERRIDE_FIELDS:
            raise TierError(f'tiers.json: {key} must be {{"tier": 1..7, "note": "..."}}')
        tier = entry['tier']
        if not isinstance(tier, int) or isinstance(tier, bool) or not MIN_TIER <= tier <= TIER_COUNT:
            raise TierError(f'tiers.json: {key} tier must be an integer 1..{TIER_COUNT}')
        note = entry.get('note', '')
        if not isinstance(note, str):
            raise TierError(f'tiers.json: {key} note must be text')
        overrides[key] = {'tier': tier, 'note': note}
    return overrides


def tier_score(item):
    """(score, source) of an item fact dict with 'openmu_drop_level' and 'client_level'."""
    if item.get('openmu_drop_level') is not None:
        return item['openmu_drop_level'], SCORE_OPENMU
    return item['client_level'], SCORE_CLIENT


def drops(item):
    known = item.get('drops_from_monsters')
    return True if known is None else bool(known)


def badge_sort_key(badges):
    return (bool(badges['socket']), bool(badges['set']), bool(badges['option380']),
            bool(badges['excellent']), badges['max_item_level'] or 0)


def order_key(item):
    score, _ = tier_score(item)
    return (not drops(item), score, badge_sort_key(item['badges']), item['group'], item['index'])


def quantile_tier(score, reference):
    """1 + floor(TIER_COUNT * (below + equal / 2) / len(reference)), clamped to 1..TIER_COUNT."""
    if not reference:
        return MIN_TIER
    below = sum(1 for value in reference if value < score)
    equal = sum(1 for value in reference if value == score)
    band = TIER_COUNT * (2 * below + equal) // (2 * len(reference))
    return max(MIN_TIER, min(TIER_COUNT, MIN_TIER + band))


def compute_tiers(items, overrides=None):
    """items: {key: facts}; facts need group, index, family, client_level, badges and optionally
    openmu_drop_level and drops_from_monsters. Returns {key: tier dict}."""
    overrides = overrides or {}
    by_family = {}
    for key, item in items.items():
        by_family.setdefault(item['family'], []).append(key)
    result = {}
    for family, keys in by_family.items():
        ordered = sorted(keys, key=lambda k: order_key(items[k]))
        dropping = [tier_score(items[k])[0] for k in keys if drops(items[k])]
        reference = dropping or [tier_score(items[k])[0] for k in keys]
        for rank, key in enumerate(ordered, start=1):
            item = items[key]
            score, source = tier_score(item)
            entry = {'value': quantile_tier(score, reference), 'score': score, 'score_source': source,
                     'drops_from_monsters': item.get('drops_from_monsters'), 'family_rank': rank,
                     'family_size': len(keys), 'source': SOURCE_COMPUTED, 'note': None}
            if key in overrides:
                entry.update(value=overrides[key]['tier'], source=SOURCE_OWNER, note=overrides[key]['note'],
                             computed_value=quantile_tier(score, reference))
            result[key] = entry
    return result
