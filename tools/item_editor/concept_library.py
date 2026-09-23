"""Every concept of an item across all batches, discards and the current pick; standard library only.

A batch is a folder of --out-dir with a batch.json. A variant is listed when its image exists.
Discarding only marks a variant in <batch>/discarded.json ({"schema", "variants": {key: [v1,..]}});
no image is ever deleted. The pick is assets-work/Items/concepts/<key>/ (concept_output.pick).
All paths returned are absolute.
"""

from pathlib import Path

import concept_batch
import concept_lock
import concept_output
import concept_refs

DISCARDED_FILE = 'discarded.json'
DISCARDED_SCHEMA = 'mu-item-concept-discarded/1'
REFS_SUBDIR = 'refs'


class LibraryError(ValueError):
    pass


# --- batches -----------------------------------------------------------------------------------

def batch_dirs(out_dir):
    """Batch folders, oldest first (batch ids start with their creation time)."""
    out_dir = Path(out_dir)
    if not out_dir.is_dir():
        return []
    return sorted(path for path in out_dir.iterdir()
                  if path.is_dir() and path.name != REFS_SUBDIR and (path / concept_batch.BATCH_FILE).is_file())


def load_batches(out_dir):
    """[(batch dir, batch)], skipping (and reporting) unreadable batch.json files."""
    batches, problems = [], []
    for path in batch_dirs(out_dir):
        try:
            batches.append((path.resolve(), concept_batch.load_batch(path)))
        except (concept_batch.BatchError, ValueError, OSError) as error:
            problems.append(f'{path.name}: {error}')
    return batches, problems


def is_running(batch_dir):
    return concept_lock.is_locked(Path(batch_dir) / concept_lock.LOCK_FILE)


def subject_for(batch, key):
    return next((s for s in batch['subjects'] if key == s['key'] or key in s['keys']), None)


# --- discards ----------------------------------------------------------------------------------

def load_discarded(batch_dir):
    path = Path(batch_dir) / DISCARDED_FILE
    if not path.is_file():
        return {}
    return {key: set(names) for key, names in concept_output.read_json(path).get('variants', {}).items()}


def set_discarded(batch_dir, key, variant, discarded):
    """Mark or unmark one variant; returns whether anything changed. The image must exist."""
    batch_dir = Path(batch_dir)
    subject = subject_for(concept_batch.load_batch(batch_dir), key)
    if subject is None:
        raise LibraryError(f'batch {batch_dir.name} has no item {key}')
    name = concept_output.variant_name(variant)
    if not (batch_dir / subject['key'] / f'{name}.png').is_file():
        raise LibraryError(f'no image {batch_dir / subject["key"] / (name + ".png")}')
    marks = load_discarded(batch_dir)
    names = marks.setdefault(subject['key'], set())
    changed = (name in names) != discarded
    (names.add if discarded else names.discard)(name)
    record = {'schema': DISCARDED_SCHEMA,
              'variants': {k: sorted(v, key=variant_number) for k, v in sorted(marks.items()) if v}}
    concept_batch.write_json_atomic(batch_dir / DISCARDED_FILE, record)
    return {'batch': batch_dir.name, 'key': subject['key'], 'variant': name, 'discarded': discarded,
            'changed': changed, 'image': str((batch_dir / subject['key'] / f'{name}.png').resolve())}


def variant_number(name):
    return int(str(name).lstrip('v'))


# --- picks -------------------------------------------------------------------------------------

def current_pick(concepts_dir, out_dir, key):
    """The committed pick of `key` (its concepts/<key>/ folder), or None."""
    folder = Path(concepts_dir) / key
    meta_file = folder / concept_output.PICK_META
    if not meta_file.is_file():
        return None
    meta = concept_output.read_json(meta_file)
    source = Path(out_dir) / meta['picked_from'] if meta.get('picked_from') else None
    return {'image': str((folder / concept_output.PICK_IMAGE).resolve()), 'meta': str(meta_file.resolve()),
            'batch': meta.get('batch'), 'variant': meta.get('variant'), 'picked': meta.get('picked'),
            'source_image': str(source.resolve()) if source and source.is_file() else None}


# --- variants ----------------------------------------------------------------------------------

def per_image(cost, count):
    return round(cost / count, 6) if cost is not None and count else None


def variant_record(batch_dir, batch, subject, request, number, discarded):
    folder = batch_dir / subject['key']
    name = f'v{number}'
    meta_file = folder / f'{name}.meta.json'
    prompt_file = folder / f'{name}.prompt.txt'
    meta = concept_output.read_json(meta_file) if meta_file.is_file() else {}
    settings = batch['settings']
    actual = meta.get('actual_cost_request')
    estimated = request['estimate']['total']
    return {
        'batch': batch['batch'], 'batch_dir': str(batch_dir), 'kind': batch.get('kind', concept_batch.KIND_GENERATE),
        'key': subject['key'], 'variant': name, 'image': str(folder / f'{name}.png'),
        'prompt': prompt_file.read_text(encoding='utf-8').strip() if prompt_file.is_file() else request['prompt'],
        'meta': str(meta_file) if meta_file.is_file() else None,
        'model': meta.get('model', settings['model']), 'quality': meta.get('quality', settings['quality']),
        'size': meta.get('size', settings['size']), 'created': meta.get('created') or batch.get('created'),
        'note': subject.get('note'), 'request': request['id'],
        'cost': {'request_actual': actual, 'request_estimated': round(estimated, 6),
                 'image_actual': per_image(actual, request['n']),
                 'image_estimated': per_image(estimated, request['n'])},
        'parent': meta.get('parent') or request.get('parent'),
        'lineage': meta.get('lineage') or subject.get('lineage') or [],
        'discarded': name in discarded,
    }


def batch_variants(batch_dir, batch, subject):
    discarded = load_discarded(batch_dir).get(subject['key'], set())
    records = []
    for request in subject['requests']:
        for number in request['variants']:
            if (batch_dir / subject['key'] / f'v{number}.png').is_file():
                records.append(variant_record(batch_dir, batch, subject, request, number, discarded))
    return records


def reference_image(refs_dir, batches, key):
    """The current look: the rendered reference, else the newest batch's ref.png."""
    rendered = concept_refs.ref_path(refs_dir, key)
    if rendered.is_file():
        return {'path': str(rendered.resolve()), 'exists': True, 'source': 'refs'}
    for batch_dir, _ in reversed(batches):
        copy = batch_dir / key / concept_output.REFERENCE_FILE
        if copy.is_file():
            return {'path': str(copy), 'exists': True, 'source': 'batch'}
    return {'path': str(rendered.resolve()), 'exists': False, 'source': 'refs'}


def item_listing(out_dir, refs_dir, concepts_dir, key):
    """Every variant of one item (its subject key or any key it covers), its reference and pick."""
    batches, problems = load_batches(out_dir)
    variants, subject_key, info = [], key, None
    for batch_dir, batch in batches:
        subject = subject_for(batch, key)
        if subject is None:
            continue
        subject_key, info = subject['key'], subject
        variants += batch_variants(batch_dir, batch, subject)
    pick = current_pick(concepts_dir, out_dir, subject_key)
    for record in variants:
        record['picked'] = bool(pick and pick['batch'] == record['batch'] and pick['variant'] == record['variant'])
    return {'key': subject_key, 'requested_key': key, 'item': item_info(info), 'variants': variants,
            'reference': reference_image(refs_dir, batches, subject_key), 'pick': pick, 'warnings': problems}


def item_info(subject):
    if subject is None:
        return None
    return {name: subject.get(name) for name in ('name', 'family', 'tier', 'keys', 'kind')}


# --- summary -----------------------------------------------------------------------------------

def summary(out_dir, concepts_dir):
    """Per item: variant and discard counts, batches, pick; plus every batch with its state."""
    batches, problems = load_batches(out_dir)
    items, batch_rows = {}, []
    for batch_dir, batch in batches:
        images = 0
        for subject in batch['subjects']:
            records = batch_variants(batch_dir, batch, subject)
            images += len(records)
            row = items.setdefault(subject['key'], dict(item_info(subject), key=subject['key'], variants=0,
                                                        discarded=0, batches=[]))
            row['variants'] += len(records)
            row['discarded'] += sum(1 for record in records if record['discarded'])
            row['batches'].append(batch['batch'])
        batch_rows.append({'batch': batch['batch'], 'dir': str(batch_dir), 'kind': batch.get('kind', 'generate'),
                           'created': batch.get('created'), 'items': len(batch['subjects']), 'images': images,
                           'running': is_running(batch_dir)})
    add_picks(items, concepts_dir, out_dir)
    return {'items': sorted(items.values(), key=lambda row: sort_key(row['key'])), 'batches': batch_rows,
            'warnings': problems}


def add_picks(items, concepts_dir, out_dir):
    concepts_dir = Path(concepts_dir)
    picked_keys = [p.name for p in concepts_dir.iterdir() if (p / concept_output.PICK_META).is_file()] \
        if concepts_dir.is_dir() else []
    for key in picked_keys:
        items.setdefault(key, {'key': key, 'variants': 0, 'discarded': 0, 'batches': []})
    for key, row in items.items():
        row['pick'] = current_pick(concepts_dir, out_dir, key)
        row['picked'] = row['pick'] is not None


def sort_key(key):
    group, _, index = key.partition('-')
    return (int(group), int(index)) if group.isdigit() and index.isdigit() else (1 << 30, 0)
