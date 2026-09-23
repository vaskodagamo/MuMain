"""Plan a concept batch: settings, requests per item, cost estimate; standard library only.

Settings come from a preset of image_prices.json (explore / final), overridden by explicit flags.
Per item, `--variants N` images are made:
  hints same      (default) one request with n = N (the reference is uploaded and billed once);
                  more than 10 images are split into requests of at most 10
  hints distinct  one request per variant, each with its own "## variant <i>" hint of
                  concept_prompt.md (v1 faithful refresh, v2 stronger silhouette, v3 more ornate)

A refine batch (concept_refine.py) starts from one earlier variant per item: its settings default
to the parent batch's (explicit flags still win) and every request attaches two images.

batch.json ("mu-item-concept-batch/1") holds the settings, every item with its requests (prompt,
n, variant numbers, estimate, the attached images and, for a refine, the parent) and, after a
run, each request's result (request id, usage, actual cost, error).
"""

from pathlib import Path
import datetime
import json
import os
import re

import concept_cost
import concept_output
import concept_prompt
import openai_images

SCHEMA = 'mu-item-concept-batch/1'
BATCH_FILE = 'batch.json'
REFERENCE_FILE = concept_output.REFERENCE_FILE
OUTPUT_FORMAT = 'png'
SHEET_SIZE = '1536x1024'
SINGLE_SIZE = '1024x1024'
BACKGROUND_SHEET = 'opaque'
BACKGROUND_SINGLE = 'transparent'
SLUG_PATTERN = re.compile(r'[^a-z0-9]+')
SLUG_MAX = 40
BATCH_TIME_FORMAT = '%Y%m%d-%H%M%S'
COST_PARTS = ('text', 'reference', 'output')


class BatchError(ValueError):
    pass


PRESET_FIELDS = ('model', 'quality', 'variants', 'hints', 'ref_size')
VIEW_FIELDS = ('sheet', 'size', 'background')


def preset_value(args, preset, name):
    value = getattr(args, name, None)
    return preset[name] if value is None else value


def base_preset(args, prices, inherited):
    """(name, values): the explicit --preset, else the inherited settings, else the default preset."""
    if args.preset or not inherited:
        name = args.preset or prices['default_preset']
        return name, prices['presets'][name]
    return inherited.get('preset'), {name: inherited[name] for name in PRESET_FIELDS}


def resolve_settings(args, prices, sections, inherited=None):
    """Model, quality, size, ... from the preset (or the inherited settings) and the explicit flags.

    The view (sheet, size, background) of `inherited` always carries over unless a flag sets it.
    """
    preset_name, preset = base_preset(args, prices, inherited)
    view = {name: inherited[name] for name in VIEW_FIELDS} if inherited else {}
    sheet = bool(args.sheet or view.get('sheet'))
    model = preset_value(args, preset, 'model')
    config = concept_cost.model_config(prices, model)
    single_background = BACKGROUND_SINGLE if config['transparent_background'] else BACKGROUND_SHEET
    settings = {
        'preset': preset_name, 'model': model, 'quality': preset_value(args, preset, 'quality'),
        'variants': preset_value(args, preset, 'variants'), 'hints': preset_value(args, preset, 'hints'),
        'ref_size': preset_value(args, preset, 'ref_size'), 'sheet': sheet,
        'size': args.size or view.get('size') or (SHEET_SIZE if sheet else SINGLE_SIZE),
        'background': args.background or view.get('background') or (BACKGROUND_SHEET if sheet else single_background),
        'output_format': OUTPUT_FORMAT, 'input_fidelity': config['input_fidelity'],
    }
    check_settings(settings, config, sections)
    return settings


def check_settings(settings, config, sections):
    concept_cost.check_quality(config, settings['model'], settings['quality'])
    concept_cost.parse_size(settings['size'])
    if settings['variants'] < 1:
        raise BatchError('--variants must be at least 1')
    if settings['ref_size'] < 1:
        raise BatchError('--ref-size must be positive')
    hint_count = concept_prompt.distinct_hint_count(sections)
    if settings['hints'] == concept_prompt.HINTS_DISTINCT and settings['variants'] > hint_count:
        raise BatchError(f'--hints distinct: concept_prompt.md defines {hint_count} variant hints, '
                         f'not {settings["variants"]}')


def parse_notes(values):
    """--note TEXT (every item) / --note KEY=TEXT (one item) -> {key or None: [text]}."""
    notes = {}
    for value in values or ():
        key, separator, text = value.partition('=')
        if separator and re.match(r'^\d+-\d+$', key.strip()):
            notes.setdefault(key.strip(), []).append(text.strip())
        else:
            notes.setdefault(None, []).append(value.strip())
    return notes


def note_for(notes, subject):
    texts = list(notes.get(None, []))
    for key in subject['keys']:
        texts += notes.get(key, [])
    return '; '.join(texts) or None


def request_groups(settings):
    """[(hint, [variant numbers])] of one item."""
    numbers = list(range(1, settings['variants'] + 1))
    if settings['hints'] == concept_prompt.HINTS_DISTINCT:
        return [(number, [number]) for number in numbers]
    step = openai_images.MAX_N
    return [(concept_prompt.HINTS_SAME, numbers[i:i + step]) for i in range(0, len(numbers), step)]


def view_of(settings):
    return concept_prompt.VIEW_SHEET if settings['sheet'] else concept_prompt.VIEW_SINGLE


def make_request(number, hint, variants, prompt, settings, prices, images=(REFERENCE_FILE,)):
    """One request record; `images` are the files of <batch>/<key>/ it attaches, in order."""
    estimate = concept_cost.estimate_request(prices, settings['model'], settings['size'], settings['quality'],
                                             len(variants), len(prompt), len(images), settings['ref_size'])
    return {'id': f'r{number}', 'hint': hint, 'prompt': prompt, 'n': len(variants), 'variants': variants,
            'images': list(images), 'estimate': estimate, 'result': None}


def request_images(request):
    """The attached files of a request (batches before refine only had the reference)."""
    return request.get('images') or [REFERENCE_FILE]


def plan_subject(subject, settings, prices, template, notes):
    sections, style = template
    note = note_for(notes, subject)
    requests = []
    for hint, variants in request_groups(settings):
        prompt = concept_prompt.build_prompt(sections, style, subject, view_of(settings), settings['hints'], hint,
                                             note)
        requests.append(make_request(len(requests) + 1, hint, variants, prompt, settings, prices))
    return dict(subject, note=note, requests=requests)


def sum_costs(costs):
    parts = {name: sum(cost['parts'][name] for cost in costs) for name in COST_PARTS}
    return {'parts': parts, 'total': sum(parts.values())}


def totals(subjects):
    """Images, requests, estimated cost parts and flags of the subjects' requests."""
    requests = [request for subject in subjects for request in subject['requests']]
    total = sum_costs([r['estimate'] for r in requests])
    total['images'] = sum(r['n'] for r in requests)
    total['requests'] = len(requests)
    total['flags'] = sorted({flag for r in requests for flag in r['estimate']['flags']})
    return total


def slugify(text):
    return SLUG_PATTERN.sub('-', text.lower()).strip('-')[:SLUG_MAX].strip('-') or 'batch'


def batch_id(slug, now=None):
    now = now or datetime.datetime.now()
    return f'{now.strftime(BATCH_TIME_FORMAT)}-{slugify(slug)}'


KIND_GENERATE = 'generate'
KIND_REFINE = 'refine'


def new_batch(batch, settings, subjects, now=None, kind=KIND_GENERATE):
    now = now or datetime.datetime.now().astimezone()
    return {'schema': SCHEMA, 'batch': batch, 'kind': kind, 'created': now.isoformat(timespec='seconds'),
            'settings': settings, 'subjects': subjects}


def load_batch(batch_dir):
    path = Path(batch_dir) / BATCH_FILE
    if not path.is_file():
        raise BatchError(f'no {BATCH_FILE} in {batch_dir}')
    batch = json.loads(path.read_text(encoding='utf-8'))
    if batch.get('schema') != SCHEMA:
        raise BatchError(f'{path}: schema is not {SCHEMA}')
    return batch


def write_json_atomic(path, value):
    """Write JSON through a temporary file and a rename: readers never see half a file."""
    path = Path(path)
    temporary = path.with_name(f'.{path.name}.tmp')
    temporary.write_text(json.dumps(value, indent=1) + '\n', encoding='utf-8')
    os.replace(temporary, path)
    return path


def save_batch(batch_dir, batch):
    return write_json_atomic(Path(batch_dir) / BATCH_FILE, batch)
