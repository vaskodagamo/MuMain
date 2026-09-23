"""Run the requests of a concept batch against the Images API and store what comes back.

Per item: <batch>/<key>/ref.png (the reference, downscaled to --ref-size, as uploaded; a refine
also parent.png, the variant it revises) and per variant v<i>.prompt.txt, v<i>.meta.json (model,
size, quality, created, request id, the request's usage, estimated and actual cost, parent and
lineage of a refine) and, written last and atomically, v<i>.png: an image that exists is complete.
<batch>/run.log has one JSON record per event.
Nothing written here ever contains the API key: every log record is scrubbed first.

Progress goes to an `events(name, **fields)` callable (request_started, request_done,
request_failed); a Cancellation stops requests that have not started yet.
"""

from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import base64
import datetime
import json
import os
import shutil
import subprocess
import threading

import concept_batch
import concept_cost
import concept_output
import concept_refs
import openai_images

RUN_LOG = 'run.log'
PNG_CONTENT_TYPE = 'image/png'
SIPS = concept_output.SIPS


class RunLog:
    """Thread-safe JSON-lines log; every record is scrubbed of the key."""

    def __init__(self, path, key=None, echo=None):
        self._path = Path(path)
        self._key = key
        self._echo = echo
        self._lock = threading.Lock()

    def __call__(self, record):
        record = dict(record, time=datetime.datetime.now().astimezone().isoformat(timespec='seconds'))
        line = openai_images.scrub(json.dumps(record, sort_keys=True), self._key)
        with self._lock:
            with open(self._path, 'a', encoding='utf-8') as handle:
                handle.write(line + '\n')
            if self._echo:
                self._echo(json.loads(line))


def prepare_reference(source, target, ref_size):
    """Copy the rendered reference, downscaled with sips to at most ref_size px (alpha kept)."""
    target.parent.mkdir(parents=True, exist_ok=True)
    if max(concept_output.png_size(source)) <= ref_size:
        shutil.copyfile(source, target)
        return
    if shutil.which(SIPS) is None:
        raise concept_output.OutputError(f'`{SIPS}` (macOS) is needed for --ref-size {ref_size}')
    command = [SIPS, '-s', 'format', 'png', '-Z', str(ref_size), str(source), '--out', str(target)]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0 or not target.is_file():
        raise concept_output.OutputError(f'sips failed: {result.stderr.strip() or result.stdout.strip()}')


def reference_sources(subject, refs_dir):
    """{file in <batch>/<key>/: where it comes from}: the rendered reference, or a refine's sources."""
    return subject.get('sources') or {
        concept_output.REFERENCE_FILE: str(concept_refs.ref_path(refs_dir, subject['key']))}


def prepare_references(batch_dir, subjects, refs_dir, ref_size):
    for subject in subjects:
        folder = Path(batch_dir) / subject['key']
        for name, source in reference_sources(subject, refs_dir).items():
            if not (folder / name).is_file():
                prepare_reference(Path(source), folder / name, ref_size)
        subject['reference_sha256'] = concept_refs.sha256_file(folder / concept_output.REFERENCE_FILE)


def missing_references(subjects, refs_dir, batch_dir=None):
    """Keys lacking an attached file both in the batch (if any yet) and at its source (no render yet)."""
    def complete(subject):
        folder = Path(batch_dir) / subject['key'] if batch_dir else None
        return all((folder is not None and (folder / name).is_file()) or Path(source).is_file()
                   for name, source in reference_sources(subject, refs_dir).items())
    return [s['key'] for s in subjects if not complete(s)]


def is_done(batch_dir, subject, request):
    folder = Path(batch_dir) / subject['key']
    return all((folder / f'v{number}.png').is_file() for number in request['variants'])


def created_text(payload):
    created = payload.get('created')
    if created is None:
        return None
    return datetime.datetime.fromtimestamp(created, datetime.timezone.utc).isoformat(timespec='seconds')


def variant_meta(batch, subject, request, number, result):
    settings = batch['settings']
    return {
        'batch': batch['batch'], 'key': subject['key'], 'keys': subject['keys'], 'name': subject['name'],
        'family': subject['family'], 'tier': subject['tier'], 'variant': f'v{number}', 'hint': request['hint'],
        'model': settings['model'], 'size': settings['size'], 'quality': settings['quality'],
        'background': settings['background'], 'output_format': settings['output_format'],
        'ref_size': settings['ref_size'], 'reference': concept_output.REFERENCE_FILE,
        'reference_sha256': subject['reference_sha256'], 'request': request['id'], 'images_in_request': request['n'],
        'created': result['created'], 'request_id': result['request_id'], 'usage': result['usage'],
        'estimated_cost_request': round(request['estimate']['total'], 6),
        'actual_cost_request': result['actual'] and round(result['actual']['total'], 6),
        'revised_prompt': result['revised_prompts'].get(number),
        'images': concept_batch.request_images(request), 'note': subject.get('note'),
        'parent': request.get('parent'), 'lineage': subject.get('lineage') or [],
    }


def write_atomic(path, data):
    temporary = path.with_name(f'.{path.name}.tmp')
    temporary.write_bytes(data)
    os.replace(temporary, path)


def save_images(batch_dir, batch, subject, request, payload, result):
    folder = Path(batch_dir) / subject['key']
    images = [entry for entry in payload.get('data') or [] if entry.get('b64_json')]
    saved = []
    for number, entry in zip(request['variants'], images):
        (folder / f'v{number}.prompt.txt').write_text(request['prompt'] + '\n', encoding='utf-8')
        if entry.get('revised_prompt'):
            result['revised_prompts'][number] = entry['revised_prompt']
        meta = variant_meta(batch, subject, request, number, result)
        (folder / f'v{number}.meta.json').write_text(json.dumps(meta, indent=1, sort_keys=True) + '\n',
                                                     encoding='utf-8')
        write_atomic(folder / f'v{number}.png', base64.b64decode(entry['b64_json']))
        saved.append(number)
    return saved


def request_fields(settings, request):
    return openai_images.edit_fields(settings['model'], request['prompt'], settings['size'], settings['quality'],
                                     settings['background'], request['n'], settings['output_format'],
                                     settings['input_fidelity'])


def attached_images(batch_dir, subject, request):
    """[(upload file name, bytes, content type)] in the request's order (a refine: parent first)."""
    folder = Path(batch_dir) / subject['key']
    return [(upload_name(subject['key'], name), (folder / name).read_bytes(), PNG_CONTENT_TYPE)
            for name in concept_batch.request_images(request)]


def upload_name(key, name):
    if name == concept_output.REFERENCE_FILE:
        return f'{key}.png'
    return f'{key}-{Path(name).stem}.png'



def variant_files(batch_dir, subject, numbers):
    folder = Path(batch_dir) / subject['key']
    return [{'variant': f'v{n}', 'image': str(folder / f'v{n}.png'), 'meta': str(folder / f'v{n}.meta.json'),
             'prompt': str(folder / f'v{n}.prompt.txt')} for n in numbers]


def run_request(client, prices, batch_dir, batch, subject, request, log, events):
    """One API call; returns the request's result record, or None when cancelled before sending."""
    tag = f'{subject["key"]}/{request["id"]}'
    ids = {'key': subject['key'], 'request': request['id']}
    try:
        payload, request_id, attempts = client.edit(request_fields(batch['settings'], request),
                                                    attached_images(batch_dir, subject, request), tag)
    except openai_images.RequestCancelled:
        events('request_cancelled', **ids)
        return None
    except openai_images.ApiError as error:
        log({'event': 'failed', 'tag': tag, 'status': error.status, 'message': str(error)})
        events('request_failed', error=str(error), status=error.status, **ids)
        return {'ok': False, 'error': str(error), 'status': error.status, 'request_id': error.request_id}
    usage = payload.get('usage')
    result = {'ok': True, 'request_id': request_id, 'attempts': attempts, 'usage': usage,
              'created': created_text(payload), 'revised_prompts': {},
              'actual': concept_cost.actual_cost(prices, batch['settings']['model'], usage)}
    result['saved'] = save_images(batch_dir, batch, subject, request, payload, result)
    result['ok'] = len(result['saved']) == request['n']
    del result['revised_prompts']
    log({'event': 'saved', 'tag': tag, 'variants': result['saved'], 'request_id': request_id})
    done = dict(ids, variants=variant_files(batch_dir, subject, result['saved']), request_id=request_id,
                actual=result['actual'], estimated=request['estimate'])
    if result['ok']:
        events('request_done', **done)
    else:
        result['error'] = f'the response had {len(result["saved"])} of {request["n"]} images'
        events('request_failed', error=result['error'], status=None, **ids)
    return result


def no_events(name, **fields):
    pass


def pending_requests(batch_dir, batch):
    return [(s, r) for s in batch['subjects'] for r in s['requests'] if not is_done(batch_dir, s, r)]


def run_batch(client, prices, batch_dir, batch, concurrency, log, events=no_events, cancel=None):
    """Run every request whose images are not there yet.

    Returns {'done', 'failed', 'not_started'} counts. A request not started because of `cancel`
    keeps result None, so `run --resume` sends it later.
    """
    def task(subject, request):
        if cancel is not None and cancel.is_set():
            return None
        events('request_started', key=subject['key'], request=request['id'],
               variants=[f'v{n}' for n in request['variants']], estimated=request['estimate'])
        return run_request(client, prices, batch_dir, batch, subject, request, log, events)

    counts = {'done': 0, 'failed': 0, 'not_started': 0}
    with ThreadPoolExecutor(max_workers=concurrency) as pool:
        futures = {pool.submit(task, subject, request): request
                   for subject, request in pending_requests(batch_dir, batch)}
        for future in as_completed(futures):
            result = future.result()
            if result is not None:
                futures[future]['result'] = result
            counts[count_name(result)] += 1
    return counts


def count_name(result):
    if result is None:
        return 'not_started'
    return 'done' if result['ok'] else 'failed'
