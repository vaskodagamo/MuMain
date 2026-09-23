"""Refine: a new round of concepts from one earlier variant plus the owner's comment.

    run --from <batch>/<key>/<variant> --note "comment"              one item (--keys K may repeat it)
    run --from KEY=<batch>/<variant> --from ... --note KEY="comment"  several items at once

Per item the new batch folder gets parent.png (the chosen variant, downscaled to --ref-size) and
ref.png (the original reference of the parent batch); each request attaches parent.png first and
ref.png second, with the `## refine` prompt of concept_prompt.md. The settings default to the first
parent batch's settings (explicit flags win); hints are always `same`. Each variant's meta.json
records `parent` {batch, key, variant} and `lineage` (parent, grandparent, ...).
"""

from pathlib import Path
import re

import concept_batch
import concept_output
import concept_prompt

FROM_WITH_KEY = re.compile(r'^(\d+-\d+)=(.+)/(v?\d+)$')
FROM_PATH = re.compile(r'^(.+)/(\d+-\d+)/(v?\d+)$')
SUBJECT_FIELDS = ('key', 'kind', 'name', 'family', 'tier', 'keys', 'models', 'study_rank')


class RefineError(ValueError):
    pass


def parse_from(values):
    """--from values -> [{'key', 'batch', 'variant'}], one per item."""
    specs = []
    for value in values or ():
        match = FROM_WITH_KEY.match(value)
        if match:
            key, batch, variant = match.groups()
        else:
            match = FROM_PATH.match(value)
            if not match:
                raise RefineError(f'--from {value!r}: use <batch>/<key>/<variant> or <key>=<batch>/<variant>')
            batch, key, variant = match.groups()
        specs.append({'key': key, 'batch': batch, 'variant': concept_output.variant_name(variant)})
    keys = [spec['key'] for spec in specs]
    duplicates = sorted({key for key in keys if keys.count(key) > 1})
    if duplicates:
        raise RefineError(f'--from names {", ".join(duplicates)} more than once')
    return specs


def batch_dir_of(out_dir, batch):
    path = Path(batch)
    return path if path.is_dir() else Path(out_dir) / batch


def find_subject(batch, key):
    for subject in batch['subjects']:
        if key == subject['key'] or key in subject['keys']:
            return subject
    raise RefineError(f'batch {batch["batch"]} has no item {key}')


def resolve_parent(out_dir, spec):
    """The variant a refine starts from: its files, batch settings, subject and lineage."""
    batch_dir = batch_dir_of(out_dir, spec['batch']).resolve()
    batch = concept_batch.load_batch(batch_dir)
    subject = find_subject(batch, spec['key'])
    folder = batch_dir / subject['key']
    image = folder / f'{spec["variant"]}.png'
    reference = folder / concept_output.REFERENCE_FILE
    for path in (image, reference):
        if not path.is_file():
            raise RefineError(f'no {path}')
    meta_file = folder / f'{spec["variant"]}.meta.json'
    meta = concept_output.read_json(meta_file) if meta_file.is_file() else {}
    link = {'batch': batch['batch'], 'key': subject['key'], 'variant': spec['variant']}
    return {'link': link, 'lineage': [link] + list(meta.get('lineage') or []), 'image': image,
            'reference': reference, 'settings': batch['settings'],
            'subject': {name: subject[name] for name in SUBJECT_FIELDS if name in subject}}


def check_selection(args, specs):
    """--from picks the items itself: --keys may only repeat them, other selections are refused."""
    if args.family or args.study_top or args.tier:
        raise RefineError('--from selects the items; drop --family, --study-top and --tier')
    unmatched = [key for key in args.keys if key not in {spec['key'] for spec in specs}]
    if unmatched:
        raise RefineError(f'--keys {", ".join(unmatched)} without a --from variant to refine')


def plan_refine_subject(parent, settings, prices, template, notes):
    sections, style = template
    subject = parent['subject']
    note = concept_batch.note_for(notes, subject)
    if not note:
        raise RefineError(f'{subject["key"]}: a refine needs a comment: --note "..." or --note {subject["key"]}="..."')
    prompt = concept_prompt.build_refine_prompt(sections, style, subject, concept_batch.view_of(settings), note)
    images = (concept_output.PARENT_FILE, concept_output.REFERENCE_FILE)
    requests = []
    for hint, variants in concept_batch.request_groups(settings):
        request = concept_batch.make_request(len(requests) + 1, hint, variants, prompt, settings, prices, images)
        requests.append(dict(request, parent=parent['link']))
    sources = {concept_output.PARENT_FILE: str(parent['image']),
               concept_output.REFERENCE_FILE: str(parent['reference'])}
    return dict(subject, note=note, parent=parent['link'], lineage=parent['lineage'], sources=sources,
                requests=requests)


def refine_settings(args, prices, sections, parents):
    inherited = dict(parents[0]['settings'], hints=concept_prompt.HINTS_SAME)
    settings = concept_batch.resolve_settings(args, prices, sections, inherited)
    return dict(settings, hints=concept_prompt.HINTS_SAME)


def slug(parents):
    return 'refine-' + '-'.join(parent['link']['key'] for parent in parents[:3])
