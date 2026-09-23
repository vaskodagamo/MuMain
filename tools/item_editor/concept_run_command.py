"""`concepts.py run`: checks, key, locks, cancellation and progress around concept_run.run_batch.

Order: plan (or load the batch to resume) -> refuse / dry run -> key -> claim the batch folder under
the global lock and take its batch lock -> copy references -> send -> save, sheet, `finished` (or
`cancelled`). Nothing is written before the key is known and the caps passed.
"""

from pathlib import Path

import concept_batch
import concept_cancel
import concept_cost
import concept_exit
import concept_key
import concept_lock
import concept_output
import concept_plan
import concept_run
import concept_text
import openai_images

BATCH_SUFFIX_LIMIT = 100


def load_or_plan(args):
    """(prices, batch dir, batch, is new); a new batch is not written yet."""
    if args.resume:
        batch_dir = resolve_batch(args, args.resume)
        return concept_cost.load_prices(args.prices), batch_dir, concept_batch.load_batch(batch_dir), False
    plan = concept_plan.make_plan(args)
    batch_dir = unique_batch_dir(args.out_dir, concept_batch.batch_id(plan.slug))
    batch = concept_batch.new_batch(batch_dir.name, plan.settings, plan.subjects, kind=plan.kind)
    return plan.prices, batch_dir, batch, True


def resolve_batch(args, name):
    path = Path(name)
    return (path if path.is_dir() else args.out_dir / name).resolve()


def pending_subjects(batch_dir, batch):
    return [dict(s, requests=[r for r in s['requests'] if not concept_run.is_done(batch_dir, s, r)])
            for s in batch['subjects']]


def check_run(args, reporter, batch_dir, batch):
    """Print (and emit) what the run would do; returns an exit code, or None to go ahead."""
    pending = [s for s in pending_subjects(batch_dir, batch) if s['requests']]
    print(f'Batch {batch["batch"]}: {concept_text.settings_line(batch["settings"])}')
    if not pending:
        print('Nothing to do: every image of the batch exists.')
        reporter.event('finished', batch=batch['batch'], dir=str(batch_dir), nothing_to_do=True, done=0, failed=0,
                       not_started=0, cost=concept_plan.cost_summary([]))
        return concept_exit.EXIT_OK
    missing = concept_run.missing_references(pending, args.refs_dir, batch_dir)
    if missing:
        raise concept_exit.UsageError(f'no reference image for {", ".join(missing)}; '
                                      f'run concepts.py refs --keys {",".join(missing)}')
    for subject in pending:
        concept_text.print_subject(subject, args.refs_dir, False, args.paths.root)
    total = concept_batch.totals(pending)
    problems = concept_text.print_totals(total, args.max_images, args.max_cost)
    if problems:
        print('Refused: over a hard cap. Select fewer items or raise the cap explicitly.')
        reasons = concept_cost.cap_reasons(total['images'], total['total'], args.max_images, args.max_cost)
        reporter.event('refused', reasons=[{'code': code, 'message': text} for code, text in reasons], estimate=total)
        return concept_exit.EXIT_REFUSED
    if not args.yes:
        print('Nothing was sent. Add --yes to run it.')
        reporter.event('dry_run', batch=batch['batch'], estimate=total, requests=total['requests'],
                       images=total['images'])
        return concept_exit.EXIT_OK
    return None


def claim_batch(args, batch_dir, batch, is_new):
    """Create a new batch folder (unique name) or reuse the resumed one; returns (dir, batch, lock)."""
    with concept_lock.global_lock(args.out_dir, f'run: claim {batch["batch"]}'):
        if is_new:
            batch_dir = unique_batch_dir(args.out_dir, batch['batch'])
            batch['batch'] = batch_dir.name
            batch_dir.mkdir(parents=True)
        lock = concept_lock.batch_lock(batch_dir, f'run {batch_dir.name}')
    if not is_new:
        batch = concept_batch.load_batch(batch_dir)  # as the last run left it
    return batch_dir.resolve(), batch, lock


def unique_batch_dir(out_dir, name):
    """out_dir/name, or name-2, name-3, ... when a batch of the same second and slug exists."""
    for number in range(1, BATCH_SUFFIX_LIMIT):
        candidate = out_dir / (name if number == 1 else f'{name}-{number}')
        if not candidate.exists():
            return candidate
    raise concept_exit.UsageError(f'too many batches named {name}')


class RunProgress:
    """Turns run events and client log records into protocol events and human lines."""

    def __init__(self, reporter, verbose):
        self._reporter = reporter
        self._verbose = verbose

    def __call__(self, name, **fields):
        self._reporter.event(name, **fields)
        tag = f'{fields.get("key")}/{fields.get("request")}'
        if name == 'request_started':
            print(f'  sending {tag}: {", ".join(fields["variants"])}')
        elif name == 'request_done':
            print(f'  saved {tag}: {", ".join(v["variant"] for v in fields["variants"])}')
        elif name == 'request_failed':
            print(f'  failed {tag}: {fields["error"]}')
        elif name == 'request_cancelled':
            print(f'  not sent {tag}: cancelled')

    def log_record(self, record):
        if self._verbose:
            print(record)
        if record.get('event') != 'retry':
            return
        key, _, request = (record.get('tag') or '').partition('/')
        self._reporter.event('retry', key=key, request=request, attempt=record['attempt'],
                             delay_s=record['delay_s'], reason=record.get('reason'))
        print(f'  retry {record.get("tag")} in {record["delay_s"]} s: {record.get("reason")}')


def command_run(args, reporter):
    prices, batch_dir, batch, is_new = load_or_plan(args)
    decision = check_run(args, reporter, batch_dir, batch)
    if decision is not None:
        return decision
    key = concept_key.api_key()
    reporter.hide_secret(key)
    cancel = concept_cancel.Cancellation()
    reporter.on_closed(lambda: cancel.set('stdout closed'))
    with cancel.signals_installed():
        batch_dir, batch, lock = claim_batch(args, batch_dir, batch, is_new)
        try:
            return execute(args, reporter, prices, batch_dir, batch, key, cancel)
        finally:
            lock.release()


def execute(args, reporter, prices, batch_dir, batch, key, cancel):
    concept_run.prepare_references(batch_dir, batch['subjects'], args.refs_dir, batch['settings']['ref_size'])
    concept_batch.save_batch(batch_dir, batch)
    progress = RunProgress(reporter, args.verbose)
    log = concept_run.RunLog(batch_dir / concept_run.RUN_LOG, key, progress.log_record)
    client = openai_images.ImagesClient(key, args.api_base, args.timeout, args.attempts, log,
                                        sleep=cancel.wait, cancelled=cancel.is_set)
    pending = [request for _, request in concept_run.pending_requests(batch_dir, batch)]
    total = concept_batch.totals(pending_subjects(batch_dir, batch))
    reporter.event('started', batch=batch['batch'], dir=str(batch_dir), kind=batch.get('kind'),
                   estimate=total, requests=total['requests'], images=total['images'], concurrency=args.concurrency)
    try:
        counts = concept_run.run_batch(client, prices, batch_dir, batch, args.concurrency, log, progress, cancel.event)
    finally:
        concept_batch.save_batch(batch_dir, batch)
    return finish(args, reporter, batch_dir, batch, pending, counts, cancel)


def finish(args, reporter, batch_dir, batch, pending, counts, cancel):
    sheet = concept_output.write_sheet(batch_dir, args.concepts_dir)
    concept_text.print_batch_costs(batch)
    print(f'Contact sheet: {concept_text.display_path(sheet, args.paths.root)}')
    fields = dict(counts, batch=batch['batch'], dir=str(batch_dir), sheet=str(sheet.resolve()),
                  cost=concept_plan.cost_summary(pending))
    if cancel.is_set() and counts['not_started']:
        print(f'Cancelled ({cancel.signal_name}): {counts["not_started"]} request(s) not sent; '
              f'continue with: concepts.py run --resume {batch["batch"]} --yes')
        reporter.event('cancelled', reason=cancel.signal_name, resume=['run', '--resume', batch['batch'], '--yes'],
                       **fields)
        return concept_exit.EXIT_CANCELLED
    reporter.event('finished', nothing_to_do=False, **fields)
    if counts['failed']:
        print(f'{counts["failed"]} request(s) failed; run again with --resume {batch["batch"]}')
        return concept_exit.EXIT_FAILED
    return concept_exit.EXIT_OK
