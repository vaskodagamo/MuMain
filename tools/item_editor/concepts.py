#!/usr/bin/env python3
"""Item concept images through the OpenAI Images API; standard library only (+ macOS sips, Blender).

The owner picks designs from these images; the picked concept becomes the item request's
reference image (captures/ref-concept.jpg) and Codex models it in Blender. Workflow and costs:
assets-work/Items/concepts/README.md.

    concepts.py plan  --study-top 10                 dry run (the default command): prompts + cost
    concepts.py refs  --study-top 10                 render the current look of each item (Blender)
    concepts.py run   --study-top 10 --yes           spend: one batch in out/item-concepts/<batch>/
    concepts.py sheet <batch>                        contact sheet <batch>/sheet.html
    concepts.py pick  <batch> <key> <variant>        -> assets-work/Items/concepts/<key>/
    concepts.py unpick <key>
    concepts.py plan  --batch <batch>                estimated vs actual cost of a finished run

The API key is read only from $OPENAI_API_KEY, only by `run --yes`, and is never printed or written.
Exit codes: 0 ok, 1 error, 3 refused (over --max-images / --max-cost).
"""

from pathlib import Path
import argparse
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import concept_batch  # noqa: E402
import concept_cost  # noqa: E402
import concept_output  # noqa: E402
import concept_prompt  # noqa: E402
import concept_refs  # noqa: E402
import concept_run  # noqa: E402
import concept_select  # noqa: E402
import openai_images  # noqa: E402

ROOT = concept_select.ROOT
OUT_DIR = ROOT / 'out' / 'item-concepts'
REFS_SUBDIR = 'refs'
COMMANDS = ('plan', 'run', 'refs', 'sheet', 'pick', 'unpick')
DEFAULT_COMMAND = 'plan'
DEFAULT_CONCURRENCY = 2
DEFAULT_MAX_IMAGES = 30
DEFAULT_MAX_COST = 5.0
EXIT_OK, EXIT_ERROR, EXIT_REFUSED = 0, 1, 3
PROMPT_INDENT = '      '
PROGRESS_EVENTS = {'saved', 'failed', 'retry'}


class UsageError(RuntimeError):
    pass


# --- arguments ---------------------------------------------------------------------------------

def add_paths(parser):
    parser.add_argument('--out-dir', type=Path, default=OUT_DIR, help='batches (default out/item-concepts)')
    parser.add_argument('--refs-dir', type=Path, help='reference renders (default <out-dir>/refs)')
    parser.add_argument('--catalog', type=Path, default=concept_select.CATALOG_FILE)
    parser.add_argument('--prices', type=Path, default=concept_cost.PRICES_FILE)


def add_selection(parser):
    parser.add_argument('--keys', type=concept_select.parse_keys, default=[], help='item keys, e.g. 0-19,6-1')
    parser.add_argument('--family', action='append', default=[], help='catalog family, e.g. swords (repeatable)')
    parser.add_argument('--tier', type=concept_select.parse_tiers, help='tier filter: T1-T3, T2, 1-3')
    parser.add_argument('--study-top', type=int, default=0, help="the art study's first N rework targets")


def add_generation(parser):
    parser.add_argument('--preset', choices=('explore', 'final'), help='model/quality/variants/ref size preset')
    parser.add_argument('--model', help='e.g. gpt-image-2.5-flare, gpt-image-2, gpt-image-1-mini')
    parser.add_argument('--quality', help='low/medium/high (2.5 models also xhigh/max)')
    parser.add_argument('--size', help='WIDTHxHEIGHT (default 1024x1024, 1536x1024 with --sheet)')
    parser.add_argument('--variants', type=int, help='images per item')
    parser.add_argument('--hints', choices=(concept_prompt.HINTS_SAME, concept_prompt.HINTS_DISTINCT),
                        help='same: one request with n images; distinct: one request per variant hint')
    parser.add_argument('--ref-size', type=int, help='longest side of the uploaded reference in px')
    parser.add_argument('--sheet', action='store_true', help='front + side turnaround on one canvas')
    parser.add_argument('--background', choices=('transparent', 'opaque', 'auto'))
    parser.add_argument('--note', action='append', help='"text" for every item or "KEY=text" (repeatable)')
    parser.add_argument('--max-images', type=int, default=DEFAULT_MAX_IMAGES)
    parser.add_argument('--max-cost', type=float, default=DEFAULT_MAX_COST, help='USD')


def build_parser():
    parser = argparse.ArgumentParser(prog='concepts.py', description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    plan = commands.add_parser('plan', help='dry run: prompts and cost estimate')
    run = commands.add_parser('run', help='call the API (needs --yes and $OPENAI_API_KEY)')
    refs = commands.add_parser('refs', help='render reference images of the current look')
    for sub in (plan, run, refs):
        add_paths(sub)
        add_selection(sub)
    for sub in (plan, run):
        add_generation(sub)
    plan.add_argument('--no-prompts', action='store_true', help='do not print the prompts')
    plan.add_argument('--batch', help='show estimated vs actual cost of a batch instead')
    add_run_arguments(run)
    refs.add_argument('--blender')
    refs.add_argument('--bmdconv')
    refs.add_argument('--force', action='store_true', help='render even when the cache is current')
    for name, extra in (('sheet', ()), ('pick', ('key', 'variant')), ('unpick', ('key',))):
        sub = commands.add_parser(name)
        add_paths(sub)
        if name != 'unpick':
            sub.add_argument('batch', help='batch id (folder name under --out-dir) or path')
        for argument in extra:
            sub.add_argument(argument)
    return parser


def add_run_arguments(run):
    run.add_argument('--yes', action='store_true', help='really spend (otherwise only the estimate)')
    run.add_argument('--name', help='batch slug (default: from the selection)')
    run.add_argument('--resume', help='continue a batch: only its missing images')
    run.add_argument('--concurrency', type=int, default=DEFAULT_CONCURRENCY)
    run.add_argument('--timeout', type=float, default=openai_images.DEFAULT_TIMEOUT, help='seconds per request')
    run.add_argument('--attempts', type=int, default=openai_images.DEFAULT_ATTEMPTS)
    run.add_argument('--api-base', default=openai_images.API_BASE, help=argparse.SUPPRESS)
    run.add_argument('--verbose', action='store_true', help='print every log record (headers redacted)')


def parse_arguments(argv):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or (argv[0] not in COMMANDS and argv[0] not in ('-h', '--help')):
        argv.insert(0, DEFAULT_COMMAND)
    args = build_parser().parse_args(argv)
    if getattr(args, 'refs_dir', None) is None:
        args.refs_dir = args.out_dir / REFS_SUBDIR
    return args


# --- plan output -------------------------------------------------------------------------------

def money(value):
    return f'${value:.4f}' if value < 1 else f'${value:.2f}'


def cost_split(cost):
    parts = cost['parts']
    return (f'text {money(parts["text"])} + reference {money(parts["reference"])} + '
            f'output {money(parts["output"])} = {money(cost["total"])}')


def display_path(path):
    path = Path(path)
    return path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else str(path)


def settings_line(settings):
    return (f'preset {settings["preset"]}: {settings["model"]}, quality {settings["quality"]}, {settings["size"]}, '
            f'background {settings["background"]}, {settings["variants"]} image(s) per item '
            f'(hints {settings["hints"]}), reference {settings["ref_size"]} px')


def print_subject(subject, refs_dir, show_prompts):
    rank = f', study rank {subject["study_rank"]}' if subject.get('study_rank') else ''
    print(f'\n{subject["key"]}  {subject["name"]}  ({subject["family"]}, T{subject["tier"]}{rank})')
    if len(subject['keys']) > 1:
        print(f'  covers: {", ".join(subject["keys"])}')
    reference = concept_refs.ref_path(refs_dir, subject['key'])
    state = '' if reference.is_file() else f'  MISSING: run concepts.py refs --keys {subject["key"]}'
    print(f'  reference: {display_path(reference)}{state}')
    for request in subject['requests']:
        variants = ', '.join(f'v{n}' for n in request['variants'])
        print(f'  {request["id"]} (n={request["n"]}: {variants}): {cost_split(request["estimate"])}')
        if show_prompts:
            print(PROMPT_INDENT + request['prompt'].replace('\n', '\n' + PROMPT_INDENT))


def print_totals(total, args):
    print(f'\nTotal (estimate): {total["images"]} images in {total["requests"]} requests: {cost_split(total)}')
    for flag in total['flags']:
        print(f'  flag: {flag}')
    problems = concept_cost.cap_problems(total['images'], total['total'], args.max_images, args.max_cost)
    caps = f'--max-images {args.max_images}, --max-cost {money(args.max_cost)}'
    print(f'Caps ({caps}): ' + ('; '.join(problems) if problems else 'within both'))
    return problems


def make_plan(args):
    prices = concept_cost.load_prices(args.prices)
    template = (concept_prompt.read_sections(), concept_prompt.load_style())
    settings = concept_batch.resolve_settings(args, prices, template[0])
    subjects = concept_select.select(concept_select.load_catalog(args.catalog), args.keys, args.family,
                                     args.tier, args.study_top)
    notes = concept_batch.parse_notes(args.note)
    planned = [concept_batch.plan_subject(s, settings, prices, template, notes) for s in subjects]
    return prices, settings, planned


def command_plan(args):
    if args.batch:
        return print_batch_costs(resolve_batch(args))
    _, settings, subjects = make_plan(args)
    print(f'Plan: {len(subjects)} item(s), {settings_line(settings)}')
    for subject in subjects:
        print_subject(subject, args.refs_dir, not args.no_prompts)
    print_totals(concept_batch.totals(subjects), args)
    return EXIT_OK


# --- batches -----------------------------------------------------------------------------------

def resolve_batch(args, name=None):
    name = name or args.batch
    path = Path(name)
    return path if path.is_dir() else args.out_dir / name


def print_batch_costs(batch_dir):
    batch = concept_batch.load_batch(batch_dir)
    print(f'Batch {batch["batch"]}: {settings_line(batch["settings"])}')
    estimated, actual = [], []
    for subject in batch['subjects']:
        for request in subject['requests']:
            result = request['result'] or {}
            estimated.append(request['estimate'])
            line = f'{subject["key"]}/{request["id"]}: estimated {money(request["estimate"]["total"])}'
            if result.get('actual'):
                actual.append(result['actual'])
                difference = result['actual']['total'] - request['estimate']['total']
                line += f', actual {cost_split(result["actual"])} (difference {difference:+.4f})'
            elif result.get('error'):
                line += f', failed: {result["error"]}'
            print('  ' + line)
    print_cost_comparison(estimated, actual)
    return EXIT_OK


def print_cost_comparison(estimated, actual):
    total_estimate = concept_batch.sum_costs(estimated)
    print(f'Estimated: {cost_split(total_estimate)}')
    if not actual:
        print('Actual: no usage recorded yet')
        return
    total_actual = concept_batch.sum_costs(actual)
    print(f'Actual (from usage, {len(actual)} of {len(estimated)} requests): {cost_split(total_actual)}')
    for part in concept_batch.COST_PARTS + ('total',):
        guess = total_estimate['total'] if part == 'total' else total_estimate['parts'][part]
        real = total_actual['total'] if part == 'total' else total_actual['parts'][part]
        print(f'  {part:9} estimated {money(guess)}  actual {money(real)}  difference {real - guess:+.4f}')


# --- run ---------------------------------------------------------------------------------------

def selection_slug(args):
    if args.name:
        return args.name
    if args.study_top:
        return f'study-top-{args.study_top}'
    return '-'.join(args.family + args.keys[:3]) or 'batch'


def load_or_plan(args):
    """(prices, batch dir, batch); a new batch is not written yet."""
    if args.resume:
        batch_dir = resolve_batch(args, args.resume)
        return concept_cost.load_prices(args.prices), batch_dir, concept_batch.load_batch(batch_dir)
    prices, settings, subjects = make_plan(args)
    batch = concept_batch.new_batch(concept_batch.batch_id(selection_slug(args)), settings, subjects)
    return prices, args.out_dir / batch['batch'], batch


def pending_subjects(batch_dir, batch):
    return [dict(s, requests=[r for r in s['requests'] if not concept_run.is_done(batch_dir, s, r)])
            for s in batch['subjects']]


def check_run(args, batch_dir, batch):
    """Print what the run would do; returns an exit code, or None to go ahead."""
    pending = [s for s in pending_subjects(batch_dir, batch) if s['requests']]
    print(f'Batch {batch["batch"]}: {settings_line(batch["settings"])}')
    if not pending:
        print('Nothing to do: every image of the batch exists.')
        return EXIT_OK
    missing = concept_run.missing_references(batch_dir, pending, args.refs_dir)
    if missing:
        raise UsageError(f'no reference image for {", ".join(missing)}; run concepts.py refs --keys {",".join(missing)}')
    for subject in pending:
        print_subject(subject, args.refs_dir, False)
    if print_totals(concept_batch.totals(pending), args):
        print('Refused: over a hard cap. Select fewer items or raise the cap explicitly.')
        return EXIT_REFUSED
    if not args.yes:
        print('Nothing was sent. Add --yes to run it.')
        return EXIT_OK
    return None


def progress(verbose):
    def echo(record):
        if verbose:
            print(record)
        elif record.get('event') in PROGRESS_EVENTS:
            detail = record.get('variants') or record.get('message') or f'retry in {record.get("delay_s")} s'
            print(f'  {record["event"]}: {record.get("tag")} {detail}')
    return echo


def command_run(args):
    prices, batch_dir, batch = load_or_plan(args)
    decision = check_run(args, batch_dir, batch)
    if decision is not None:
        return decision
    key = openai_images.api_key_from_env()
    batch_dir.mkdir(parents=True, exist_ok=True)
    concept_run.prepare_references(batch_dir, batch['subjects'], args.refs_dir, batch['settings']['ref_size'])
    concept_batch.save_batch(batch_dir, batch)
    log = concept_run.RunLog(batch_dir / concept_run.RUN_LOG, key, progress(args.verbose))
    client = openai_images.ImagesClient(key, args.api_base, args.timeout, args.attempts, log)
    try:
        failed = concept_run.run_batch(client, prices, batch_dir, batch, args.concurrency, log)
    finally:
        concept_batch.save_batch(batch_dir, batch)
    sheet = concept_output.write_sheet(batch_dir)
    print_batch_costs(batch_dir)
    print(f'Contact sheet: {display_path(sheet)}')
    if failed:
        print(f'{failed} request(s) failed; run again with --resume {batch["batch"]}')
        return EXIT_ERROR
    return EXIT_OK


# --- refs, sheet, pick ------------------------------------------------------------------------

def command_refs(args):
    subjects = concept_select.select(concept_select.load_catalog(args.catalog), args.keys, args.family,
                                     args.tier, args.study_top)
    blender = concept_refs.find_blender(args.blender)
    bmdconv = concept_refs.find_bmdconv(args.bmdconv)
    concept_refs.render_refs(subjects, args.refs_dir, blender, bmdconv, args.force)
    return EXIT_OK


def command_sheet(args):
    print(f'wrote {display_path(concept_output.write_sheet(resolve_batch(args)))}')
    return EXIT_OK


def command_pick(args):
    target = concept_output.pick(resolve_batch(args), args.key, args.variant)
    print(f'picked {concept_output.variant_name(args.variant)} -> {display_path(target)} '
          f'(copy {concept_output.PICK_IMAGE} into the item request as captures/ref-concept.jpg)')
    return EXIT_OK


def command_unpick(args):
    removed = concept_output.unpick(args.key)
    print(f'{args.key}: ' + ('pick removed' if removed else 'nothing picked'))
    return EXIT_OK


HANDLERS = {'plan': command_plan, 'run': command_run, 'refs': command_refs, 'sheet': command_sheet,
            'pick': command_pick, 'unpick': command_unpick}
ERRORS = (UsageError, concept_select.SelectionError, concept_cost.CostError, concept_prompt.PromptError,
          concept_batch.BatchError, concept_refs.RefError, concept_output.OutputError, openai_images.ApiError,
          OSError)


def main(argv=None):
    args = parse_arguments(argv)
    try:
        return HANDLERS[args.command](args)
    except ERRORS as error:
        print(f'error: {error}', file=sys.stderr)
        return EXIT_ERROR


if __name__ == '__main__':
    sys.exit(main())
