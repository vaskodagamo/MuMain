"""The plan of a batch (a new selection, or a refine from earlier variants) and its JSON shapes.

plan_json() is the `plan --json` object and cost_summary() the cost block of the `finished` event
(assets-work/Items/concepts/README.md, "Editor protocol").
"""

from collections import namedtuple
from pathlib import Path

import concept_batch
import concept_cost
import concept_output
import concept_prompt
import concept_refine
import concept_run
import concept_select

Plan = namedtuple('Plan', 'prices settings subjects kind slug')
REASON_MISSING_REFERENCE = 'missing_reference'
REASON_NO_KEY = 'no_api_key'


def load_template(args):
    return concept_prompt.read_sections(args.paths.prompt_template), concept_prompt.load_style(args.paths.baseline)


def selection_slug(args):
    if args.name:
        return args.name
    if args.study_top:
        return f'study-top-{args.study_top}'
    return '-'.join(args.family + args.keys[:3]) or 'batch'


def make_plan(args):
    """The batch this command line asks for (nothing is written)."""
    prices = concept_cost.load_prices(args.prices)
    template = load_template(args)
    notes = concept_batch.parse_notes(args.note)
    if args.from_variant:
        return refine_plan(args, prices, template, notes)
    settings = concept_batch.resolve_settings(args, prices, template[0])
    subjects = concept_select.select(concept_select.load_catalog(args.catalog), args.keys, args.family, args.tier,
                                     args.study_top, args.paths.baseline)
    planned = [concept_batch.plan_subject(s, settings, prices, template, notes) for s in subjects]
    return Plan(prices, settings, planned, concept_batch.KIND_GENERATE, selection_slug(args))


def refine_plan(args, prices, template, notes):
    specs = concept_refine.parse_from(args.from_variant)
    concept_refine.check_selection(args, specs)
    parents = [concept_refine.resolve_parent(args.out_dir, spec) for spec in specs]
    settings = concept_refine.refine_settings(args, prices, template[0], parents)
    subjects = [concept_refine.plan_refine_subject(parent, settings, prices, template, notes) for parent in parents]
    return Plan(prices, settings, subjects, concept_batch.KIND_REFINE, args.name or concept_refine.slug(parents))


# --- JSON --------------------------------------------------------------------------------------

def file_state(path):
    path = Path(path).resolve()
    return {'path': str(path), 'exists': path.is_file()}


def request_json(request, show_prompt):
    record = {'id': request['id'], 'n': request['n'], 'variants': [f'v{n}' for n in request['variants']],
              'hint': request['hint'], 'images': concept_batch.request_images(request), 'estimate': request['estimate']}
    if show_prompt:
        record['prompt'] = request['prompt']
    return record


def item_json(subject, refs_dir, show_prompts):
    sources = concept_run.reference_sources(subject, refs_dir)
    item = {name: subject.get(name) for name in ('key', 'keys', 'name', 'family', 'tier', 'kind', 'study_rank', 'note')}
    item.update({
        'reference': file_state(sources[concept_output.REFERENCE_FILE]),
        'parent': subject.get('parent'), 'lineage': subject.get('lineage') or [],
        'parent_image': file_state(sources[concept_output.PARENT_FILE]) if subject.get('parent') else None,
        'requests': [request_json(request, show_prompts) for request in subject['requests']],
        'estimate': concept_batch.sum_costs([request['estimate'] for request in subject['requests']]),
    })
    return item


def refusal_reasons(total, args, missing, key_status):
    """[{code, message}] why `run --yes` would refuse this plan (empty: it would send it)."""
    caps = concept_cost.cap_reasons(total['images'], total['total'], args.max_images, args.max_cost)
    reasons = [{'code': code, 'message': text} for code, text in caps]
    if missing:
        message = f'no reference image for {", ".join(missing)}; run concepts.py refs --keys {",".join(missing)}'
        reasons.append({'code': REASON_MISSING_REFERENCE, 'keys': missing, 'message': message})
    if key_status is not None and not key_status['found']:
        reasons.append({'code': REASON_NO_KEY, 'message': 'no API key (OPENAI_API_KEY or the Keychain item)'})
    return reasons


def plan_json(args, plan, key_status):
    total = concept_batch.totals(plan.subjects)
    missing = concept_run.missing_references(plan.subjects, args.refs_dir)
    reasons = refusal_reasons(total, args, missing, key_status)
    return {
        'repo_root': str(args.paths.root), 'out_dir': str(args.out_dir), 'refs_dir': str(args.refs_dir),
        'kind': plan.kind, 'settings': plan.settings,
        'items': [item_json(subject, args.refs_dir, not args.no_prompts) for subject in plan.subjects],
        'totals': total, 'caps': {'max_images': args.max_images, 'max_cost': args.max_cost},
        'flags': total['flags'], 'api_key': key_status,
        'would_refuse': bool(reasons), 'reasons': reasons,
    }


def cost_summary(requests):
    """Estimated vs actual cost of the given request records (actual only where usage came back)."""
    estimated = concept_batch.sum_costs([request['estimate'] for request in requests])
    actual_costs = [request['result']['actual'] for request in requests
                    if request.get('result') and request['result'].get('actual')]
    actual = concept_batch.sum_costs(actual_costs) if actual_costs else None
    return {'estimated': estimated, 'actual': actual, 'requests': len(requests),
            'requests_with_usage': len(actual_costs)}


def batch_costs_json(batch_dir, batch):
    requests = []
    for subject in batch['subjects']:
        for request in subject['requests']:
            result = request['result'] or {}
            requests.append({'key': subject['key'], 'request': request['id'], 'estimated': request['estimate'],
                             'actual': result.get('actual'), 'error': result.get('error'),
                             'ok': result.get('ok')})
    all_requests = [r for s in batch['subjects'] for r in s['requests']]
    return dict(cost_summary(all_requests), batch=batch['batch'], dir=str(Path(batch_dir).resolve()),
                settings=batch['settings'], per_request=requests)

