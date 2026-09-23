#!/usr/bin/env python3
"""Item concept images through the OpenAI Images API; standard library only (+ macOS sips, Blender).

The owner picks designs from these images; the picked concept becomes the item request's
reference image (captures/ref-concept.jpg) and Codex models it in Blender. Workflow, costs and the
item editor's JSON protocol: assets-work/Items/concepts/README.md.

    concepts.py plan  --study-top 10                 dry run (the default command): prompts + cost
    concepts.py refs  --study-top 10                 render the current look of each item (Blender)
    concepts.py run   --study-top 10 --yes           spend: one batch in out/item-concepts/<batch>/
    concepts.py run   --from <batch>/<key>/v2 --note "thinner guard" --yes     refine one variant
    concepts.py sheet <batch>                        contact sheet <batch>/sheet.html
    concepts.py list  [--key K]                      every variant of an item / all items
    concepts.py pick  <batch> <key> <variant>        -> assets-work/Items/concepts/<key>/
    concepts.py unpick <key>
    concepts.py discard <batch> <key> <variant>      mark only (undiscard reverts); nothing is deleted
    concepts.py plan  --batch <batch>                estimated vs actual cost of a finished run

Paths resolve from the repository root (found from this script, or --repo-root), never from the
working directory. The API key comes from $OPENAI_API_KEY, else the macOS Keychain item
(concept_key.py); only `run --yes` reads it, and it is never printed or written.
--json (one result object) and --json-progress (JSON Lines events) are the editor's protocol.
Exit codes: 0 ok, 1 error, 2 usage, 3 refused by a cap, 4 some requests failed, 5 no API key,
6 busy (locked by another run), 130 cancelled.
"""

from pathlib import Path
import argparse
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import concept_commands  # noqa: E402
import concept_events  # noqa: E402
import concept_exit  # noqa: E402
import concept_paths  # noqa: E402
import concept_prompt  # noqa: E402
import concept_select  # noqa: E402
import openai_images  # noqa: E402

COMMANDS = tuple(concept_commands.HANDLERS)
DEFAULT_COMMAND = 'plan'
DEFAULT_CONCURRENCY = 2
DEFAULT_MAX_IMAGES = 30
DEFAULT_MAX_COST = 5.0
REFS_SUBDIR = 'refs'
EXIT_OK, EXIT_ERROR, EXIT_USAGE = concept_exit.EXIT_OK, concept_exit.EXIT_ERROR, concept_exit.EXIT_USAGE
EXIT_REFUSED, EXIT_FAILED, EXIT_NO_KEY = concept_exit.EXIT_REFUSED, concept_exit.EXIT_FAILED, concept_exit.EXIT_NO_KEY
EXIT_BUSY, EXIT_CANCELLED = concept_exit.EXIT_BUSY, concept_exit.EXIT_CANCELLED


# --- arguments ---------------------------------------------------------------------------------

def add_paths(parser):
    parser.add_argument('--repo-root', type=Path, help='the MuMain checkout (default: found from this script)')
    parser.add_argument('--out-dir', type=Path, help='batches (default <repo>/out/item-concepts)')
    parser.add_argument('--refs-dir', type=Path, help='reference renders (default <out-dir>/refs)')
    parser.add_argument('--concepts-dir', type=Path, help='picks (default <repo>/assets-work/Items/concepts)')
    parser.add_argument('--catalog', type=Path, help='default <repo>/assets-work/Items/catalog.json')
    parser.add_argument('--prices', type=Path, help='default <repo>/tools/item_editor/image_prices.json')


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
    parser.add_argument('--from', dest='from_variant', action='append', default=[],
                        help='refine: <batch>/<key>/<variant> or <key>=<batch>/<variant> (repeatable)')
    parser.add_argument('--name', help='batch slug (default: from the selection)')
    parser.add_argument('--max-images', type=int, default=DEFAULT_MAX_IMAGES)
    parser.add_argument('--max-cost', type=float, default=DEFAULT_MAX_COST, help='USD')


def add_run_arguments(run):
    run.add_argument('--yes', action='store_true', help='really spend (otherwise only the estimate)')
    run.add_argument('--resume', help='continue a batch: only its missing images')
    run.add_argument('--concurrency', type=int, default=DEFAULT_CONCURRENCY)
    run.add_argument('--timeout', type=float, default=openai_images.DEFAULT_TIMEOUT, help='seconds per request')
    run.add_argument('--attempts', type=int, default=openai_images.DEFAULT_ATTEMPTS)
    run.add_argument('--api-base', default=openai_images.API_BASE, help=argparse.SUPPRESS)
    run.add_argument('--verbose', action='store_true', help='print every log record (headers redacted)')


def add_json(parser, progress=False):
    if progress:
        parser.add_argument('--json-progress', action='store_true', help='JSON Lines events on stdout (editor)')
    else:
        parser.add_argument('--json', action='store_true', help='one JSON result object on stdout (editor)')


def build_parser():
    parser = argparse.ArgumentParser(prog='concepts.py', description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    plan = commands.add_parser('plan', help='dry run: prompts and cost estimate')
    run = commands.add_parser('run', help='call the API (needs --yes and an API key)')
    refs = commands.add_parser('refs', help='render reference images of the current look')
    for sub in (plan, run, refs):
        add_paths(sub)
        add_selection(sub)
    for sub in (plan, run):
        add_generation(sub)
    plan.add_argument('--no-prompts', action='store_true', help='do not print (or return) the prompts')
    plan.add_argument('--batch', help='show estimated vs actual cost of a batch instead')
    add_run_arguments(run)
    refs.add_argument('--blender')
    refs.add_argument('--bmdconv')
    refs.add_argument('--force', action='store_true', help='render even when the cache is current')
    add_json(plan)
    add_json(run, progress=True)
    add_json(refs, progress=True)
    add_other_commands(commands)
    return parser


def add_other_commands(commands):
    positionals = {'sheet': ('batch',), 'pick': ('batch', 'key', 'variant'), 'unpick': ('key',),
                   'discard': ('batch', 'key', 'variant'), 'undiscard': ('batch', 'key', 'variant'), 'list': ()}
    for name, arguments in positionals.items():
        sub = commands.add_parser(name)
        add_paths(sub)
        add_json(sub)
        for argument in arguments:
            sub.add_argument(argument)
        if name == 'list':
            sub.add_argument('--key', help='one item: every variant, its reference and pick')


def normalized_argv(argv):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or (argv[0] not in COMMANDS and argv[0] not in ('-h', '--help')):
        argv.insert(0, DEFAULT_COMMAND)
    return argv


def absolute(path):
    return Path(path).expanduser().resolve()


def resolve_paths(args):
    """Fill every path from the repository root unless given explicitly; all absolute."""
    paths = concept_paths.repo_paths(args.repo_root)
    args.paths = paths
    args.out_dir = absolute(args.out_dir or paths.out_dir)
    args.refs_dir = absolute(args.refs_dir or args.out_dir / REFS_SUBDIR)
    args.concepts_dir = absolute(args.concepts_dir or paths.concepts_dir)
    args.catalog = absolute(args.catalog or paths.catalog)
    args.prices = absolute(args.prices or paths.prices)


def output_mode(args):
    if getattr(args, 'json_progress', False):
        return concept_events.MODE_PROGRESS
    return concept_events.MODE_JSON if getattr(args, 'json', False) else concept_events.MODE_TEXT


def main(argv=None):
    args = build_parser().parse_args(normalized_argv(argv))
    reporter = concept_events.Reporter(output_mode(args), args.command)
    try:
        with reporter.human_output():
            resolve_paths(args)
            return concept_commands.HANDLERS[args.command](args, reporter)
    except concept_exit.ERRORS as error:
        code = concept_exit.exit_code_for(error)
        message = openai_images.scrub(str(error))
        print(f'error: {message}', file=sys.stderr)
        reporter.error(message, code)
        return code


if __name__ == '__main__':
    sys.exit(main())
