"""Plan a concept batch: settings, requests per item, cost estimate; standard library only.

Settings come from a preset of image_prices.json (explore / final), overridden by explicit flags.
Per item, `--variants N` images are made:
  hints same      (default) one request with n = N (the reference is uploaded and billed once);
                  more than 10 images are split into requests of at most 10
  hints distinct  one request per variant, each with its own "## variant <i>" hint of
                  concept_prompt.md (v1 faithful refresh, v2 stronger silhouette, v3 more ornate)

batch.json ("mu-item-concept-batch/1") holds the settings, every item with its requests (prompt,
n, variant numbers, estimate) and, after a run, each request's result (request id, usage, actual
cost, error).
"""

from pathlib import Path
import datetime
import json
import re

import concept_cost
import concept_prompt
import openai_images

SCHEMA = 'mu-item-concept-batch/1'
BATCH_FILE = 'batch.json'
REFERENCE_COUNT = 1
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


def preset_value(args, preset, name):
    value = getattr(args, name, None)
    return preset[name] if value is None else value


def resolve_settings(args, prices, sections):
    """Model, quality, size, ... from the preset and the explicit flags."""
    preset_name = args.preset or prices['default_preset']
    preset = prices['presets'][preset_name]
    model = preset_value(args, preset, 'model')
    config = concept_cost.model_config(prices, model)
    single_background = BACKGROUND_SINGLE if config['transparent_background'] else BACKGROUND_SHEET
    settings = {
        'preset': preset_name, 'model': model, 'quality': preset_value(args, preset, 'quality'),
        'variants': preset_value(args, preset, 'variants'), 'hints': preset_value(args, preset, 'hints'),
        'ref_size': preset_value(args, preset, 'ref_size'), 'sheet': bool(args.sheet),
        'size': args.size or (SHEET_SIZE if args.sheet else SINGLE_SIZE),
        'background': args.background or (BACKGROUND_SHEET if args.sheet else single_background),
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


def plan_subject(subject, settings, prices, template, notes):
    sections, style = template
    view = concept_prompt.VIEW_SHEET if settings['sheet'] else concept_prompt.VIEW_SINGLE
    note = note_for(notes, subject)
    requests = []
    for hint, variants in request_groups(settings):
        prompt = concept_prompt.build_prompt(sections, style, subject, view, settings['hints'], hint, note)
        estimate = concept_cost.estimate_request(prices, settings['model'], settings['size'], settings['quality'],
                                                 len(variants), len(prompt), REFERENCE_COUNT, settings['ref_size'])
        requests.append({'id': f'r{len(requests) + 1}', 'hint': hint, 'prompt': prompt, 'n': len(variants),
                         'variants': variants, 'estimate': estimate, 'result': None})
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


def new_batch(batch, settings, subjects, now=None):
    now = now or datetime.datetime.now().astimezone()
    return {'schema': SCHEMA, 'batch': batch, 'created': now.isoformat(timespec='seconds'),
            'settings': settings, 'subjects': subjects}


def load_batch(batch_dir):
    path = Path(batch_dir) / BATCH_FILE
    if not path.is_file():
        raise BatchError(f'no {BATCH_FILE} in {batch_dir}')
    batch = json.loads(path.read_text(encoding='utf-8'))
    if batch.get('schema') != SCHEMA:
        raise BatchError(f'{path}: schema is not {SCHEMA}')
    return batch


def save_batch(batch_dir, batch):
    path = Path(batch_dir) / BATCH_FILE
    path.write_text(json.dumps(batch, indent=1) + '\n', encoding='utf-8')
    return path
