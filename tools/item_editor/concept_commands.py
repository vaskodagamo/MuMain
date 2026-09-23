"""The commands of concepts.py other than `run`: plan, refs, sheet, pick, unpick, discard, list.

Each handler takes the parsed arguments and a concept_events.Reporter and returns an exit code;
human text goes through print(), protocol records through the reporter.
"""

from pathlib import Path

import concept_batch
import concept_cancel
import concept_exit
import concept_key
import concept_library
import concept_lock
import concept_output
import concept_plan
import concept_refs
import concept_run_command
import concept_select
import concept_text


# --- plan --------------------------------------------------------------------------------------

def command_plan(args, reporter):
    if args.batch:
        return plan_batch_costs(args, reporter)
    plan = concept_plan.make_plan(args)
    print(f'Plan: {len(plan.subjects)} item(s), {concept_text.settings_line(plan.settings)}')
    for subject in plan.subjects:
        concept_text.print_subject(subject, args.refs_dir, not args.no_prompts, args.paths.root)
    concept_text.print_totals(concept_batch.totals(plan.subjects), args.max_images, args.max_cost)
    if reporter.is_json:
        reporter.result(concept_plan.plan_json(args, plan, concept_key.key_status()))
    return concept_exit.EXIT_OK


def plan_batch_costs(args, reporter):
    batch_dir = concept_run_command.resolve_batch(args, args.batch)
    batch = concept_batch.load_batch(batch_dir)
    concept_text.print_batch_costs(batch)
    reporter.result(concept_plan.batch_costs_json(batch_dir, batch))
    return concept_exit.EXIT_OK


# --- refs --------------------------------------------------------------------------------------

def command_refs(args, reporter):
    subjects = concept_select.select(concept_select.load_catalog(args.catalog), args.keys, args.family,
                                     args.tier, args.study_top, args.paths.baseline)
    refs_dir = args.refs_dir
    with concept_lock.FileLock(refs_dir / concept_lock.LOCK_FILE, 'refs').acquire():
        stale = concept_refs.stale_subjects(subjects, refs_dir, args.paths.root, args.force)
        reporter.event('started', dir=str(refs_dir), requests=len(stale), items=len(subjects))
        for subject in subjects:
            if subject not in stale:
                report_reference(reporter, refs_dir, subject['key'], cached=True, root=args.paths.root)
        if stale and render(args, reporter, stale) == concept_exit.EXIT_CANCELLED:
            return concept_exit.EXIT_CANCELLED
        reporter.event('finished', dir=str(refs_dir), rendered=len(stale), cached=len(subjects) - len(stale), failed=0)
    return concept_exit.EXIT_OK


def report_reference(reporter, refs_dir, key, cached, root):
    path = concept_refs.ref_path(refs_dir, key).resolve()
    print(f'{key}: {"current" if cached else "rendered"} ({concept_text.display_path(path, root)})')
    reporter.event('request_done', key=key, reference=str(path), cached=cached)


def render(args, reporter, stale):
    root = args.paths.root
    blender = concept_refs.find_blender(args.blender)
    bmdconv = concept_refs.find_bmdconv(args.bmdconv, root)
    for subject in stale:
        reporter.event('request_started', key=subject['key'])
    cancel = concept_cancel.Cancellation()
    with cancel.signals_installed():
        try:
            concept_refs.render_refs(stale, args.refs_dir, blender, bmdconv, root, cancel.is_set)
        except concept_refs.RenderCancelled:
            print(f'Cancelled ({cancel.signal_name}); run refs again to render the rest.')
            reporter.event('cancelled', reason=cancel.signal_name, dir=str(args.refs_dir))
            return concept_exit.EXIT_CANCELLED
        except concept_refs.RefError as error:
            for subject in stale:
                reporter.event('request_failed', key=subject['key'], error=str(error))
            raise
    for subject in stale:
        report_reference(reporter, args.refs_dir, subject['key'], cached=False, root=root)
    return concept_exit.EXIT_OK


# --- sheet, pick, unpick, discard ------------------------------------------------------------

def command_sheet(args, reporter):
    sheet = concept_output.write_sheet(concept_run_command.resolve_batch(args, args.batch), args.concepts_dir)
    print(f'wrote {concept_text.display_path(sheet, args.paths.root)}')
    reporter.result({'sheet': str(sheet.resolve())})
    return concept_exit.EXIT_OK


def batch_subject_key(batch_dir, key):
    subject = concept_library.subject_for(concept_batch.load_batch(batch_dir), key)
    if subject is None:
        raise concept_library.LibraryError(f'batch {Path(batch_dir).name} has no item {key}')
    return subject['key']


def command_pick(args, reporter):
    batch_dir = concept_run_command.resolve_batch(args, args.batch)
    key = batch_subject_key(batch_dir, args.key)
    variant = concept_output.variant_name(args.variant)
    if not (batch_dir / key / f'{variant}.png').is_file():
        raise concept_exit.UsageError(f'no image {batch_dir / key / (variant + ".png")}')
    with concept_lock.global_lock(args.out_dir, f'pick {key}'):
        target = concept_output.pick(batch_dir, key, variant, args.concepts_dir)
    print(f'picked {variant} -> {concept_text.display_path(target, args.paths.root)} '
          f'(copy {concept_output.PICK_IMAGE} into the item request as captures/ref-concept.jpg)')
    reporter.result({'key': key, 'batch': batch_dir.name, 'variant': variant,
                     'pick': concept_library.current_pick(args.concepts_dir, args.out_dir, key)})
    return concept_exit.EXIT_OK


def command_unpick(args, reporter):
    with concept_lock.global_lock(args.out_dir, f'unpick {args.key}'):
        removed = concept_output.unpick(args.key, args.concepts_dir)
    print(f'{args.key}: ' + ('pick removed' if removed else 'nothing picked'))
    reporter.result({'key': args.key, 'removed': removed})
    return concept_exit.EXIT_OK


def command_discard(args, reporter):
    discarded = args.command == 'discard'
    batch_dir = concept_run_command.resolve_batch(args, args.batch)
    with concept_lock.global_lock(args.out_dir, f'{args.command} {args.key}'):
        result = concept_library.set_discarded(batch_dir, args.key, args.variant, discarded)
    state = 'discarded' if discarded else 'kept'
    print(f'{result["batch"]}/{result["key"]}/{result["variant"]}: {state}'
          + ('' if result['changed'] else ' (already)'))
    reporter.result(result)
    return concept_exit.EXIT_OK


# --- list --------------------------------------------------------------------------------------

def command_list(args, reporter):
    if args.key:
        listing = concept_library.item_listing(args.out_dir, args.refs_dir, args.concepts_dir, args.key)
        print_item_listing(listing)
    else:
        listing = concept_library.summary(args.out_dir, args.concepts_dir)
        print_summary(listing)
    for warning in listing['warnings']:
        print(f'warning: {warning}')
    reporter.result(listing)
    return concept_exit.EXIT_OK


def print_item_listing(listing):
    item = listing['item'] or {}
    print(f'{listing["key"]}  {item.get("name", "")}  reference: {listing["reference"]["path"]}'
          + ('' if listing['reference']['exists'] else ' (missing)'))
    for record in listing['variants']:
        marks = [mark for mark, on in (('picked', record['picked']), ('discarded', record['discarded'])) if on]
        parent = record['parent']
        origin = f' <- {parent["batch"]}/{parent["key"]}/{parent["variant"]}' if parent else ''
        print(f'  {record["batch"]} {record["variant"]}{origin}  {" ".join(marks)}')
    pick = listing['pick']
    print(f'  pick: {pick["batch"]}/{pick["variant"]} ({pick["image"]})' if pick else '  pick: none')


def print_summary(listing):
    for row in listing['items']:
        picked = f'picked {row["pick"]["batch"]}/{row["pick"]["variant"]}' if row['picked'] else 'no pick'
        print(f'{row["key"]:7} {row.get("name") or "":32} {row["variants"]:3} variant(s), '
              f'{row["discarded"]} discarded, {len(row["batches"])} batch(es), {picked}')
    running = [batch['batch'] for batch in listing['batches'] if batch['running']]
    if running:
        print(f'running now: {", ".join(running)}')


HANDLERS = {'plan': command_plan, 'run': concept_run_command.command_run, 'refs': command_refs,
            'sheet': command_sheet, 'pick': command_pick, 'unpick': command_unpick, 'discard': command_discard,
            'undiscard': command_discard, 'list': command_list}
