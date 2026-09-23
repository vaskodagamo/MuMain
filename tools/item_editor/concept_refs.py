"""Reference images ("current look") for concept requests, rendered offline in Blender.

Pipeline of the art study (assets-work/Items/study/README.md, "Before previews"), reused as is:
prepare_previews.import_model() turns each BMD into a .blend through tools/blender/mu_bmd_import.py,
then render_concept_refs.py (in Blender) renders them with the study's render_previews.py. An
armour set is rendered as its five parts together.

Output: <refs dir>/<key>.png plus <key>.json (the models and their sha256). A reference is
re-rendered only when a model's sha256 or RENDER_VERSION changed.
"""

from pathlib import Path
from types import SimpleNamespace
import hashlib
import importlib.util
import json
import os
import subprocess
import tempfile

import concept_select

HERE = Path(__file__).resolve().parent
ROOT = concept_select.ROOT
DATA_ROOT = ROOT / 'src' / 'bin' / 'Data'
STUDY_PREPARE = ROOT / 'assets-work' / 'Items' / 'study' / 'prepare_previews.py'
RENDER_SCRIPT = HERE / 'render_concept_refs.py'
DEFAULT_BLENDER = Path('/Applications/Blender.app/Contents/MacOS/Blender')
BLENDER_ENV = 'MU_BLENDER'
BMDCONV_ENV = 'MU_BMDCONV'
BMDCONV_GLOB = 'out/build/*/tools/bmdconv/*/bmdconv'
# The main checkout next to a worktree usually has the tools build (read-only use).
SIBLING_CHECKOUT = 'MuMain'
RENDER_VERSION = 1
READ_CHUNK = 1 << 20
BLEND_SUFFIX = '.blend'


class RefError(RuntimeError):
    pass


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(READ_CHUNK), b''):
            digest.update(chunk)
    return digest.hexdigest()


def find_tool(explicit, env, candidates, what, hint):
    for candidate in [explicit, os.environ.get(env)] + candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    raise RefError(f'{what} not found; pass {hint} or set ${env}')


def find_bmdconv(explicit=None):
    candidates = sorted(str(p) for p in ROOT.glob(BMDCONV_GLOB))
    candidates += sorted(str(p) for p in (ROOT.parent / SIBLING_CHECKOUT).glob(BMDCONV_GLOB))
    return find_tool(explicit, BMDCONV_ENV, candidates, 'bmdconv', '--bmdconv')


def find_blender(explicit=None):
    return find_tool(explicit, BLENDER_ENV, [str(DEFAULT_BLENDER)], 'Blender', '--blender')


def ref_path(refs_dir, key):
    return Path(refs_dir) / f'{key}.png'


def model_hashes(subject):
    hashes = {}
    for bmd in subject['models']:
        path = ROOT / bmd
        if not path.is_file():
            raise RefError(f'{subject["key"]}: model {bmd} does not exist')
        hashes[bmd] = sha256_file(path)
    return hashes


def cache_record(subject):
    return {'key': subject['key'], 'render_version': RENDER_VERSION, 'models': model_hashes(subject)}


def is_current(refs_dir, subject):
    record_file = Path(refs_dir) / f'{subject["key"]}.json'
    if not (ref_path(refs_dir, subject['key']).is_file() and record_file.is_file()):
        return False
    return json.loads(record_file.read_text(encoding='utf-8')) == cache_record(subject)


def load_study_prepare():
    spec = importlib.util.spec_from_file_location('study_prepare_previews', STUDY_PREPARE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def data_relative(bmd):
    return (ROOT / bmd).relative_to(DATA_ROOT).as_posix()


def import_models(subjects, blender, bmdconv, imports_dir):
    prepare = load_study_prepare()
    args = SimpleNamespace(blender=blender, bmdconv=bmdconv, imports_dir=Path(imports_dir))
    for bmd in sorted({bmd for subject in subjects for bmd in subject['models']}):
        prepare.import_model(data_relative(bmd), args, DATA_ROOT)


def render_jobs(subjects):
    return [{'name': subject['key'],
             'files': [str(Path(data_relative(bmd)).with_suffix(BLEND_SUFFIX)) for bmd in subject['models']]}
            for subject in subjects]


def run_blender(blender, subjects, imports_dir, refs_dir):
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False, encoding='utf-8') as handle:
        json.dump(render_jobs(subjects), handle)
        jobs_file = handle.name
    command = [str(blender), '-b', '--python', str(RENDER_SCRIPT), '--', '--imports-dir', str(imports_dir),
               '--output-dir', str(refs_dir), '--jobs', jobs_file]
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace')
    finally:
        os.unlink(jobs_file)
    if result.returncode != 0:
        raise RefError(f'Blender render failed (exit {result.returncode}):\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}')


def render_refs(subjects, refs_dir, blender, bmdconv, force=False, report=print):
    """Render the references that are missing or stale; returns the keys rendered."""
    refs_dir = Path(refs_dir)
    stale = [s for s in subjects if force or not is_current(refs_dir, s)]
    for subject in subjects:
        if subject not in stale:
            report(f'{subject["key"]}: current ({ref_path(refs_dir, subject["key"])})')
    if not stale:
        return []
    imports_dir = refs_dir / 'imports'
    import_models(stale, blender, bmdconv, imports_dir)
    run_blender(blender, stale, imports_dir, refs_dir)
    for subject in stale:
        record = cache_record(subject)
        (refs_dir / f'{subject["key"]}.json').write_text(json.dumps(record, indent=1) + '\n', encoding='utf-8')
        report(f'{subject["key"]}: rendered {ref_path(refs_dir, subject["key"])}')
    return [subject['key'] for subject in stale]
