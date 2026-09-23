"""Run the requests of a concept batch against the Images API and store what comes back.

Per item: <batch>/<key>/ref.png (the reference, downscaled to --ref-size, as uploaded) and per
variant v<i>.png, v<i>.prompt.txt and v<i>.meta.json (model, size, quality, created, request id,
the request's usage, estimated and actual cost). <batch>/run.log has one JSON record per event.
Nothing written here ever contains the API key: every log record is scrubbed first.
"""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import base64
import datetime
import json
import shutil
import subprocess
import threading

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


def prepare_references(batch_dir, subjects, refs_dir, ref_size):
    for subject in subjects:
        target = Path(batch_dir) / subject['key'] / concept_output.REFERENCE_FILE
        if not target.is_file():
            prepare_reference(concept_refs.ref_path(refs_dir, subject['key']), target, ref_size)
        subject['reference_sha256'] = concept_refs.sha256_file(target)


def missing_references(batch_dir, subjects, refs_dir):
    """Keys with neither a reference in the batch nor a rendered one in refs_dir."""
    def has_reference(key):
        return ((Path(batch_dir) / key / concept_output.REFERENCE_FILE).is_file()
                or concept_refs.ref_path(refs_dir, key).is_file())
    return [s['key'] for s in subjects if not has_reference(s['key'])]


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
    }


def save_images(batch_dir, batch, subject, request, payload, result):
    folder = Path(batch_dir) / subject['key']
    images = [entry for entry in payload.get('data') or [] if entry.get('b64_json')]
    saved = []
    for number, entry in zip(request['variants'], images):
        (folder / f'v{number}.png').write_bytes(base64.b64decode(entry['b64_json']))
        (folder / f'v{number}.prompt.txt').write_text(request['prompt'] + '\n', encoding='utf-8')
        if entry.get('revised_prompt'):
            result['revised_prompts'][number] = entry['revised_prompt']
        meta = variant_meta(batch, subject, request, number, result)
        (folder / f'v{number}.meta.json').write_text(json.dumps(meta, indent=1, sort_keys=True) + '\n',
                                                     encoding='utf-8')
        saved.append(number)
    return saved


def request_fields(settings, request):
    return openai_images.edit_fields(settings['model'], request['prompt'], settings['size'], settings['quality'],
                                     settings['background'], request['n'], settings['output_format'],
                                     settings['input_fidelity'])


def run_request(client, prices, batch_dir, batch, subject, request, log):
    """One API call; returns the request's result record (never raises for API failures)."""
    tag = f'{subject["key"]}/{request["id"]}'
    reference = (Path(batch_dir) / subject['key'] / concept_output.REFERENCE_FILE).read_bytes()
    images = [(f'{subject["key"]}.png', reference, PNG_CONTENT_TYPE)]
    try:
        payload, request_id, attempts = client.edit(request_fields(batch['settings'], request), images, tag)
    except openai_images.ApiError as error:
        log({'event': 'failed', 'tag': tag, 'status': error.status, 'message': str(error)})
        return {'ok': False, 'error': str(error), 'status': error.status, 'request_id': error.request_id}
    usage = payload.get('usage')
    result = {'ok': True, 'request_id': request_id, 'attempts': attempts, 'usage': usage,
              'created': created_text(payload), 'revised_prompts': {},
              'actual': concept_cost.actual_cost(prices, batch['settings']['model'], usage)}
    result['saved'] = save_images(batch_dir, batch, subject, request, payload, result)
    result['ok'] = len(result['saved']) == request['n']
    del result['revised_prompts']
    log({'event': 'saved', 'tag': tag, 'variants': result['saved'], 'request_id': request_id})
    return result


def run_batch(client, prices, batch_dir, batch, concurrency, log):
    """Run every request whose images are not there yet; returns the number of failed requests."""
    todo = [(s, r) for s in batch['subjects'] for r in s['requests'] if not is_done(batch_dir, s, r)]
    with ThreadPoolExecutor(max_workers=concurrency) as pool:
        futures = [(request, pool.submit(run_request, client, prices, batch_dir, batch, subject, request, log))
                   for subject, request in todo]
        for request, future in futures:
            request['result'] = future.result()
    return sum(1 for request, _ in futures if not request['result']['ok'])
