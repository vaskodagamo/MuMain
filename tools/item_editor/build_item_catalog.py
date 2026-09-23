#!/usr/bin/env python3
"""Build assets-work/Items/catalog.json (schema "mu-item-catalog/1"), the per-item file the item
editor and Codex read. Standard library only; reads game data, never writes it.

Inputs:
    tools/item_editor/item_models.json        item -> model files (gen_item_models.py, from the C++ load code)
    src/bin/Data/Local/Eng/item_eng.bmd        the client item table (--item-table for another language)
    src/bin/Data/Local/ItemSetType.bmd         client "can be ancient" records
    src/bin/Data/Local/ItemAddOption.bmd       client 380 options
    assets-work/Items/openmu-items.json        optional OpenMU export (export_openmu_items.py)
    assets-work/Items/tiers.json               owner tier overrides
    assets-work/Items/client-review.json       optional owner verdicts from the editor
    assets-work/Items/requests/*/request.json  filed item requests
    every model BMD, through `bmdconv info`    (--bmdconv, or $MU_BMDCONV, or out/build/*/tools/bmdconv/*/bmdconv)
    git history of HEAD                        original.revision of every model file

Usage (from anywhere):
    python3 tools/item_editor/build_item_catalog.py --bmdconv <path/to/bmdconv>          # write
    python3 tools/item_editor/build_item_catalog.py --bmdconv <path/to/bmdconv> --check  # exit 1 if stale

The output is deterministic for a given commit: sorted keys, sorted lists, no timestamps. Every
path is relative to the repository root. Field reference: assets-work/Items/README.md.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import bmd_facts  # noqa: E402
import item_table  # noqa: E402
import tiers  # noqa: E402

ROOT = HERE.parents[1]
ITEMS_DIR = ROOT / 'assets-work' / 'Items'
OUTPUT = ITEMS_DIR / 'catalog.json'
MODELS_FILE = HERE / 'item_models.json'
ITEM_TABLE = ROOT / 'src/bin/Data/Local/Eng/item_eng.bmd'
SET_TYPE_FILE = ROOT / 'src/bin/Data/Local/itemsettype.bmd'
ADD_OPTION_FILE = ROOT / 'src/bin/Data/Local/ItemAddOption.bmd'
OPENMU_FILE = ITEMS_DIR / 'openmu-items.json'
TIERS_FILE = ITEMS_DIR / 'tiers.json'
CLIENT_REVIEW_FILE = ITEMS_DIR / 'client-review.json'
REQUESTS_DIR = ITEMS_DIR / 'requests'
ASSIGNMENTS_FILE = ITEMS_DIR / 'assignments.json'
BMDCONV_ENV = 'MU_BMDCONV'
BMDCONV_GLOB = 'out/build/*/tools/bmdconv/*/bmdconv'
GIT_DATA_DIRS = ('src/bin/Data/Item', 'src/bin/Data/Player')

SCHEMA = 'mu-item-catalog/1'
MODELS_SCHEMA = 'mu-item-models/1'
OPENMU_SCHEMA = 'mu-openmu-items/1'
READ_CHUNK = 1 << 20
ROLE_ITEM = 'item'
ROLE_CLASS_VARIANT = 'class-variant'
ARMOUR_GROUPS = range(7, 12)
ARMOUR_SET_GROUP = 8
VERDICTS = ('looks-good', 'needs-work')
CLIENT_REVIEW_KEYS = {'verdict', 'note', 'date'}
DATE_PATTERN = re.compile(r'^\d{4}-\d{2}-\d{2}$')
OTHER_CONSUMER_PREFIX = 'other:'


class CatalogError(RuntimeError):
    pass


# --- small helpers --------------------------------------------------------------------------

def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(READ_CHUNK), b''):
            digest.update(chunk)
    return digest.hexdigest()


def relative(path):
    return Path(path).resolve().relative_to(ROOT).as_posix()


def find_bmdconv(explicit):
    candidates = [explicit, os.environ.get(BMDCONV_ENV)] + sorted(str(p) for p in ROOT.glob(BMDCONV_GLOB))
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    raise CatalogError(f'bmdconv not found; pass --bmdconv, set ${BMDCONV_ENV} or build tools/bmdconv '
                       f'(cmake --build --preset macos-arm64 --target bmdconv)')


def git_revisions(root, folders):
    """Repo path -> full sha of the newest commit of HEAD that changed it."""
    done = subprocess.run(['git', 'log', '--format=%x01%H', '--name-only', '--', *folders],
                          cwd=root, capture_output=True, text=True, check=False)
    if done.returncode != 0:
        raise CatalogError(f'git log failed: {done.stderr.strip()}')
    revisions, current = {}, None
    for line in done.stdout.splitlines():
        if line.startswith('\x01'):
            current = line[1:]
        elif line and current:
            revisions.setdefault(line, current)
    return revisions


def git_modified(root, folders):
    done = subprocess.run(['git', 'status', '--porcelain', '--', *folders], cwd=root, capture_output=True, text=True, check=False)
    return sorted(line[3:] for line in done.stdout.splitlines() if line.strip())


# --- inputs ---------------------------------------------------------------------------------

def load_models():
    models = read_json(MODELS_FILE)
    if models.get('schema') != MODELS_SCHEMA:
        raise CatalogError(f'{relative(MODELS_FILE)}: expected schema {MODELS_SCHEMA}')
    return models


def load_openmu(path):
    if not path.exists():
        return None
    data = read_json(path)
    if data.get('schema') != OPENMU_SCHEMA:
        raise CatalogError(f'{relative(path)}: expected schema {OPENMU_SCHEMA}')
    return data


def load_client_review(path, keys):
    if not path.exists():
        return {}
    data = read_json(path)
    if not isinstance(data, dict):
        raise CatalogError('client-review.json must be an object keyed by item key')
    for key, entry in data.items():
        if key not in keys:
            raise CatalogError(f'client-review.json: {key} is not an item of the catalog')
        if not isinstance(entry, dict) or set(entry) != CLIENT_REVIEW_KEYS:
            raise CatalogError(f'client-review.json: {key} must have exactly {sorted(CLIENT_REVIEW_KEYS)}')
        if entry['verdict'] not in VERDICTS or not DATE_PATTERN.match(str(entry['date'])) or not isinstance(entry['note'], str):
            raise CatalogError(f'client-review.json: {key} has an invalid verdict, date or note')
    return data


def load_assignments():
    """Request id -> the worker label the coordinator assigned it to (assignments.json)."""
    if not ASSIGNMENTS_FILE.exists():
        return {}
    data = read_json(ASSIGNMENTS_FILE)
    if not isinstance(data, dict):
        raise CatalogError('assignments.json must be an object keyed by worker label')
    return {entry['request']: worker for worker, entry in sorted(data.items())
            if isinstance(entry, dict) and isinstance(entry.get('request'), str)}


def load_requests():
    """[{id, status, kind, items, assigned_to, path}] for every request folder, sorted by id."""
    assigned = load_assignments()
    requests = []
    for path in sorted(REQUESTS_DIR.glob('*/request.json')):
        try:
            request = read_json(path)
        except ValueError:
            continue
        targets = request.get('targets') if isinstance(request.get('targets'), list) else []
        requests.append({
            'id': request.get('id', path.parent.name),
            'status': request.get('status'),
            'kind': request.get('kind'),
            'items': [t.get('key') for t in targets if isinstance(t, dict)],
            'assigned_to': assigned.get(request.get('id', path.parent.name)),
            'path': relative(path.parent),
        })
    return requests


# --- models ---------------------------------------------------------------------------------

class ModelFacts:
    """bmdconv facts, textures and git revision of every model file, computed once per path."""

    def __init__(self, bmdconv, revisions):
        self.bmdconv = bmdconv
        self.revisions = revisions
        self.folders = bmd_facts.FolderIndex(ROOT)
        self.cache = {}

    def describe(self, record):
        """A model entry of the catalog from an item_models.json record."""
        path = record['file']['path']
        dirs = record['texture_dirs']
        entry = {'bmd': path, 'exists': record['file']['exists'], 'folder': path.rsplit('/', 1)[0],
                 'texture_dirs': dirs, 'loaded_by': record['file']['loaded_by']}
        for key in ('role', 'class', 'condition', 'model'):
            if key in record:
                entry[key] = record[key]
        if not record['file']['exists']:
            entry.update(structure=None, textures={}, texture_status={}, original=None)
            return entry
        structure = self.structure(path)
        textures, status = {}, {}
        for name in structure['mesh_textures']:
            container, found = bmd_facts.resolve_texture(self.folders, name, dirs)
            textures[name] = container
            if found != 'found':
                status[name] = found
        entry.update(structure=structure, textures=textures, texture_status=status,
                     original={'revision': self.revisions.get(path), 'sha256': sha256_file(ROOT / path)})
        return entry

    def structure(self, path):
        if path not in self.cache:
            self.cache[path] = bmd_facts.bmd_info(self.bmdconv, ROOT / path)
        return self.cache[path]

    def prefetch(self, paths):
        """Run bmdconv for many files in parallel; the results do not depend on the order."""
        missing = sorted({path for path in paths if path not in self.cache})
        with ThreadPoolExecutor(max_workers=os.cpu_count() or 1) as pool:
            for path, info in zip(missing, pool.map(lambda p: bmd_facts.bmd_info(self.bmdconv, ROOT / p), missing)):
                self.cache[path] = info


def item_models(record, classes, facts):
    """Primary model first, then the extra models the engine swaps in (class variants only for
    classes that can equip the item)."""
    models = [facts.describe(dict(record, role=ROLE_ITEM))]
    for extra in record['extra_models']:
        if extra['role'] == ROLE_CLASS_VARIANT and not classes.get(extra['class']):
            continue
        models.append(facts.describe(extra))
    return models


# --- item facts -----------------------------------------------------------------------------

def class_stages(fields):
    return dict(zip(item_table.CLASS_KEYS, fields['RequireClass']))


def table_facts(fields):
    return {
        'width': fields['Width'],
        'height': fields['Height'],
        'two_hand': bool(fields['TwoHand']),
        'drop_level': fields['Level'],
        'require_level': fields['RequireLevel'],
        'classes': class_stages(fields),
        'item_slot': fields['m_byItemSlot'],
    }


OPENMU_FIELDS = ('name', 'drop_level', 'maximum_drop_level', 'drops_from_monsters', 'maximum_item_level',
                 'maximum_sockets', 'is_ammunition', 'qualified_classes', 'ancient_sets', 'set_bonus_groups',
                 'options', 'excellent', 'width', 'height')


def badges_for(key, family, item_type, client, openmu):
    socket = key in client['sockets'] or bool(openmu and openmu['maximum_sockets'] > 0)
    ancient = bool(openmu['ancient_sets']) if openmu else item_type in client['set_types']
    excellent = openmu['excellent'] if openmu else family in tiers.EXCELLENT_FAMILIES_WITHOUT_OPENMU
    return {
        'socket': socket,
        'set': ancient,
        'option380': item_type in client['add_options'],
        'excellent': excellent,
        'max_item_level': openmu['maximum_item_level'] if openmu else None,
    }


def client_facts(key, item_type, client):
    return {
        'socket_item': key in client['sockets'],
        'set_types': client['set_types'].get(item_type, []),
        'option380': client['add_options'].get(item_type),
    }


def armour_sets(entries):
    """Armour parts (groups 7-11) that share an index form a set, named after the armour part."""
    sets = {}
    for key, entry in entries.items():
        if entry['group'] in ARMOUR_GROUPS:
            sets.setdefault(entry['index'], []).append(key)
    result = {}
    for index, keys in sets.items():
        if len(keys) < 2:
            continue
        keys.sort(key=lambda k: entries[k]['group'])
        named = [entries[k]['name'] for k in keys if entries[k]['name']]
        body = next((entries[k]['name'] for k in keys if entries[k]['group'] == ARMOUR_SET_GROUP and entries[k]['name']), None)
        label = (body or (named[0] if named else '')).rsplit(' ', 1)[0] or None
        for key in keys:
            result[key] = {'index': index, 'name': label, 'parts': keys}
    return result


def shared_textures(entries, other_models):
    """Container -> every consumer (item keys, and other:<MODEL> for non-item models)."""
    consumers = {}
    for key, entry in entries.items():
        for model in entry['models']:
            for container in model['textures'].values():
                if container:
                    consumers.setdefault(container, set()).add(key)
    for model in other_models:
        for container in model['textures'].values():
            if container:
                consumers.setdefault(container, set()).add(OTHER_CONSUMER_PREFIX + model['model'])
    return consumers


def sort_key(key):
    group, index = key.split('-')
    return int(group), int(index)


# --- assembly -------------------------------------------------------------------------------

def build(bmdconv, table_path=ITEM_TABLE, openmu_path=OPENMU_FILE):
    models = load_models()
    table = item_table.read_item_table(Path(table_path).read_bytes())
    client = {
        'sockets': set(models['socket_items']),
        'set_types': item_table.read_set_types(SET_TYPE_FILE.read_bytes()),
        'add_options': item_table.read_add_options(ADD_OPTION_FILE.read_bytes()),
    }
    openmu = load_openmu(openmu_path)
    openmu_items = openmu['items'] if openmu else {}
    facts = ModelFacts(bmdconv, git_revisions(ROOT, GIT_DATA_DIRS))
    records = list(models['items'].values()) + models['other_models']
    records += [extra for record in models['items'].values() for extra in record['extra_models']]
    facts.prefetch(record['file']['path'] for record in records if record['file']['exists'])

    entries = {}
    for key in sorted(models['items'], key=sort_key):
        record = models['items'][key]
        group, index = record['group'], record['index']
        item_type = group * item_table.MAX_ITEM_INDEX + index
        fields = table['items'][item_type]
        family = tiers.family_of(group, index)
        server = openmu_items.get(key)
        entries[key] = {
            'key': key, 'group': group, 'index': index,
            'name': fields['Name'] or None, 'in_table': bool(fields['Name']),
            'family': family,
            'table': table_facts(fields),
            'openmu': {f: server[f] for f in OPENMU_FIELDS} if server else None,
            'client': client_facts(key, item_type, client),
            'badges': badges_for(key, family, item_type, client, server),
            'models': item_models(record, class_stages(fields), facts),
        }

    overrides = tiers.validate_overrides(read_json(TIERS_FILE) if TIERS_FILE.exists() else {}, set(entries))
    tier_input = {key: {'group': e['group'], 'index': e['index'], 'family': e['family'],
                        'client_level': e['table']['drop_level'], 'badges': e['badges'],
                        'openmu_drop_level': e['openmu']['drop_level'] if e['openmu'] else None,
                        'drops_from_monsters': e['openmu']['drops_from_monsters'] if e['openmu'] else None}
                  for key, e in entries.items()}
    computed = tiers.compute_tiers(tier_input, overrides)
    other_models = [facts.describe(m) for m in models['other_models'] if m['file']['exists']]
    consumers = shared_textures(entries, other_models)
    sets = armour_sets(entries)
    reviews = load_client_review(CLIENT_REVIEW_FILE, set(entries))
    requests = load_requests()

    for key, entry in entries.items():
        containers = {c for m in entry['models'] for c in m['textures'].values() if c}
        entry['tier'] = computed[key]
        entry['armour_set'] = sets.get(key)
        entry['shared_with'] = {c: sorted(consumers[c] - {key}, key=consumer_sort_key)
                                for c in sorted(containers) if consumers[c] - {key}}
        entry['original'] = entry['models'][0]['original']
        entry['requests'] = [{'id': r['id'], 'status': r['status'], 'assigned_to': r['assigned_to']}
                             for r in requests if key in r['items']]
        entry['client_review'] = reviews.get(key)
    return document(entries, table, models, openmu, requests, other_models, table_path)


def consumer_sort_key(consumer):
    if consumer.startswith(OTHER_CONSUMER_PREFIX):
        return (1, 0, 0, consumer)
    return (0, *sort_key(consumer), '')


def document(entries, table, models, openmu, requests, other_models, table_path):
    missing_models = sorted({m['bmd'] for e in entries.values() for m in e['models'] if not m['exists']})
    missing_textures = sorted({f'{m["bmd"]}: {name}' for e in entries.values() for m in e['models']
                               for name, status in m['texture_status'].items() if status == 'missing'})
    without_model = [{'key': item_table.item_key(*item_table.split_type(t)), 'name': f['Name']}
                     for t, f in item_table.named_items(table)
                     if item_table.item_key(*item_table.split_type(t)) not in entries]
    families, tier_counts = {}, {}
    for entry in entries.values():
        families[entry['family']] = families.get(entry['family'], 0) + 1
        tier_counts[str(entry['tier']['value'])] = tier_counts.get(str(entry['tier']['value']), 0) + 1
    generated_from = [relative(MODELS_FILE), relative(table_path), relative(SET_TYPE_FILE), relative(ADD_OPTION_FILE),
                      'bmdconv info of every model BMD', 'git history of HEAD']
    for optional in (OPENMU_FILE, TIERS_FILE, CLIENT_REVIEW_FILE, ASSIGNMENTS_FILE):
        if optional.exists():
            generated_from.append(relative(optional))
    if requests:
        generated_from.append('assets-work/Items/requests/*/request.json')
    return {
        'schema': SCHEMA,
        'generated_by': 'tools/item_editor/build_item_catalog.py',
        'generated_from': sorted(generated_from),
        'item_table': {'file': relative(table_path), 'format': table['format'],
                       'named_items': len(item_table.named_items(table))},
        'openmu': {'source': openmu['source'], 'count': openmu['count']} if openmu else None,
        'counts': {
            'items': len(entries),
            'families': dict(sorted(families.items())),
            'tiers': dict(sorted(tier_counts.items())),
            'with_openmu': sum(1 for e in entries.values() if e['openmu']),
            'owner_tiers': sum(1 for e in entries.values() if e['tier']['source'] == tiers.SOURCE_OWNER),
            'models': sum(len(e['models']) for e in entries.values()),
            'missing_models': len(missing_models),
            'missing_textures': len(missing_textures),
            'table_items_without_model': len(without_model),
            'requests': len(requests),
        },
        'items': entries,
        'missing_models': missing_models,
        'missing_textures': missing_textures,
        'table_items_without_model': without_model,
        'requests': requests,
    }


def render(doc):
    return json.dumps(doc, indent=1, sort_keys=True, ensure_ascii=False) + '\n'


def main(argv=None):
    parser = argparse.ArgumentParser(description='Build assets-work/Items/catalog.json.')
    parser.add_argument('--bmdconv', help='path to the bmdconv binary')
    parser.add_argument('--item-table', type=Path, default=ITEM_TABLE, help='Item_<lang>.bmd to read')
    parser.add_argument('--output', type=Path, default=OUTPUT)
    parser.add_argument('--check', action='store_true', help='exit 1 when the catalog is out of date')
    args = parser.parse_args(argv)
    try:
        doc = build(find_bmdconv(args.bmdconv), args.item_table.resolve())
    except (CatalogError, item_table.ItemTableError, tiers.TierError, bmd_facts.BmdError, OSError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    text = render(doc)
    for path in git_modified(ROOT, GIT_DATA_DIRS):
        print(f'warning: {path} differs from HEAD; its original.revision describes the committed file')
    if args.check:
        current = args.output.read_text(encoding='utf-8') if args.output.exists() else ''
        if current != text:
            print(f'{relative(args.output)} is out of date; run tools/item_editor/build_item_catalog.py')
            return 1
        print(f'{relative(args.output)} is current')
        return 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding='utf-8')
    print(f'wrote {relative(args.output)} ({len(text) // 1024} KiB): {json.dumps(doc["counts"])}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
