"""Validate World1 regeneration requests (schema "mu-regen-request/1"); standard library only.

Usage (from anywhere):
    python3 assets-work/World1/requests/validate_request.py <request-dir or request.json> [...]
    python3 assets-work/World1/requests/validate_request.py --all

Checks the shape described by request.schema.json (required fields, types, enums, patterns,
status-dependent fields) and the rules a schema cannot express:
- every path exists and is repo-relative; id/folder/branch/worktree/deliver_to agree;
- targets and shared/frozen textures match coordination/dependency-map.json; owned files
  respect frozen textures and the request kind; a new-variant names a free model name;
- while a request is live (open/claimed/delivered), targets and engine-control rows equal
  catalog.json;
- with git: base_commit exists and holds the targets' current_sha256; a claimed or delivered
  request records start_commit (the main commit the worker branched from, which contains the
  filed request, the coordinator's reservation, and the same target files as base_commit), and
  its working tree differs from start_commit only inside the request folder, the owned files
  and docs/agents/WORKLOG.md; delivered originals equal base_commit and installed files equal
  their exports; captures are JPEGs at most 1920 px wide.
Exit status is 0 when every request is valid; warnings never fail. Nothing is written.
"""

import argparse
from datetime import date, datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
WORLD_DIR = HERE.parent
ROOT = HERE.parents[2]
COORDINATION = WORLD_DIR / 'coordination'
DEPENDENCY_MAP = COORDINATION / 'dependency-map.json'
PRODUCTION_BATCHES = COORDINATION / 'production-batches.json'
CATALOG = WORLD_DIR / 'catalog.json'
OBJECT_DIR = ROOT / 'src/bin/Data/Object1'
REQUEST_FILE = 'request.json'
BRIEF_FILE = 'brief.md'
WORKLOG = 'docs/agents/WORKLOG.md'
GAME_DATA = 'src/bin/Data'
FINDER_METADATA = '.DS_Store'

SCHEMA = 'mu-regen-request/1'
WORLD_NUMBER = 1
REPO = 'vaskodagamo/MuMain'
STATUSES = ('open', 'claimed', 'delivered', 'accepted', 'rejected', 'withdrawn')
LIVE_STATUSES = ('open', 'claimed', 'delivered')        # targets must still match catalog.json
WORKER_STATUSES = ('claimed', 'delivered')              # only exist on the worker branch
STARTED_STATUSES = ('claimed', 'delivered', 'accepted')  # have a claim and a start_commit
DECISION_STATUSES = ('accepted', 'rejected', 'withdrawn')
PRIORITIES = ('low', 'normal', 'high')
KINDS = ('repaint', 'remodel', 'repaint+remodel', 'new-variant')
KINDS_WITH_TEXTURES = ('repaint', 'repaint+remodel')
KINDS_WITH_BMD = ('remodel', 'repaint+remodel')
NEW_VARIANT = 'new-variant'
CAPTURE_VARIANTS = ('current', 'original', 'candidate')
REQUIRED_PROTECTED = 'src/bin/Data/World1/'
DELIVERY_LAYOUT = ('original', 'exports', 'review', 'validation/summary.json', 'notes.md', 'source.blend')
TARGET_CATALOG_FIELDS = ('type', 'bmd', 'textures', 'placement_count', 'last_batch', 'model_dir')
# Fixed when the request is filed; compared with request.json at start_commit.
FROZEN_KEYS = ('schema', 'id', 'created', 'author', 'world', 'priority', 'kind', 'base_commit', 'supersedes',
               'targets', 'change', 'constraints', 'evidence')
FROZEN_HANDOFF_KEYS = ('repo', 'branch', 'worktree', 'deliver_to', 'push_allowed')
DELIVERY_DIR = 'delivery/'
ID_DATE_LENGTH = len('YYYY-MM-DD')
ID_MAX_LENGTH = 80
MAX_CAPTURE_WIDTH = 1920
TEXTURE_SUFFIXES = ('.ozj', '.ozt')
# Start-of-frame markers carry the image size (all SOFn except DHT C4, JPG C8 and DAC CC).
JPEG_SOF_MARKERS = frozenset((0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF))

# <date>-<model>-<slug>; the slug is one to five lower-case words.
ID_PATTERN = re.compile(r'^(\d{4}-\d{2}-\d{2})-([a-z0-9]+)-([a-z0-9]+(?:-[a-z0-9]+){0,4})$')
TIMESTAMP_PATTERN = re.compile(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$')
COMMIT_PATTERN = re.compile(r'^[0-9a-f]{7,40}$')
FULL_COMMIT_PATTERN = re.compile(r'^[0-9a-f]{40}$')
SHA256_PATTERN = re.compile(r'^[0-9a-f]{64}$')
MODEL_PATTERN = re.compile(r'^[A-Za-z][A-Za-z0-9]*$')
NEW_MODEL_PATTERN = re.compile(r'^([A-Z][A-Za-z]*)(\d{2,})$')
TRAILING_DIGITS = re.compile(r'^(.*?)(\d+)$')
TEXTURE_FLAG = re.compile(r'_[RHSN]$', re.I)
GAME_PATTERN = re.compile(r'^src/bin/Data/Object1/[^/]+\.(bmd|oz[jt])$', re.I)
CONTAINER_PATTERN = re.compile(r'^src/bin/Data/Object1/[^/]+\.oz[jt]$', re.I)
BMD_PATTERN = re.compile(r'^src/bin/Data/Object1/[^/]+\.bmd$')
CAPTURE_PATTERN = re.compile(r'^assets-work/World1/requests/([^/]+)/captures/[^/]+\.(jpg|jpeg)$')
PR_URL_PATTERN = re.compile(r'^https://github\.com/vaskodagamo/MuMain/pull/\d+$')
DRIVE_PATTERN = re.compile(r'^[A-Za-z]:')

TOP_KEYS = {'schema', 'id', 'created', 'author', 'world', 'status', 'status_history', 'priority',
            'kind', 'base_commit', 'targets', 'change', 'constraints', 'evidence', 'handoff',
            'result', 'decision'}
TOP_OPTIONAL = {'supersedes'}
HISTORY_KEYS = {'status', 'at', 'by'}
TARGET_KEYS = {'model', 'type', 'bmd', 'current_sha256', 'textures', 'placement_count',
               'picked_instances', 'last_batch', 'model_dir', 'original'}
PLACEMENT_KEYS = {'position', 'rotation', 'scale'}
CHANGE_KEYS = {'summary', 'details', 'keep', 'avoid'}
CONSTRAINT_KEYS = {'shared_textures', 'frozen_textures', 'owned_files', 'protected_paths',
                   'engine_controls', 'must_keep'}
CONTROL_KEYS = {'model', 'source_row', 'controls', 'requirement'}
EVIDENCE_KEYS = {'captures', 'offline_previews'}
CAPTURE_KEYS = {'file', 'variant', 'camera', 'resolution', 'client_commit'}
HANDOFF_KEYS = {'repo', 'branch', 'worktree', 'deliver_to', 'push_allowed', 'claimed_by', 'claimed_at',
                'start_commit'}
RESULT_KEYS = {'delivered_at', 'source_commits', 'installed_files', 'validation', 'review_images',
               'notes', 'pr_url'}
DECISION_KEYS = {'status', 'at', 'by', 'reason', 'ledger_entry'}


class Report:
    def __init__(self):
        self.errors, self.warnings = [], []

    def error(self, message):
        self.errors.append(message)

    def warn(self, message):
        self.warnings.append(message)


class Git:
    """Read-only git queries run at the repository root."""

    def __init__(self):
        self.available = True

    def run(self, args, text=True):
        if not self.available:
            return None
        try:
            return subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=text)
        except OSError:
            self.available = False
            return None

    def output(self, args):
        done = self.run(args)
        return done.stdout if done is not None and done.returncode == 0 else None

    def commit(self, revision):
        """Full sha of a commit, or None when the clone does not have it."""
        found = self.output(['rev-parse', '--verify', '--quiet', f'{revision}^{{commit}}'])
        return found.strip() if found else None

    def is_ancestor(self, older, newer):
        done = self.run(['merge-base', '--is-ancestor', older, newer])
        return done is not None and done.returncode == 0

    def has_path(self, revision, path):
        done = self.run(['cat-file', '-e', f'{revision}:{path}'])
        return done is not None and done.returncode == 0

    def blob_id(self, revision, path):
        found = self.output(['rev-parse', '--verify', '--quiet', f'{revision}:{path}'])
        return found.strip() if found else None

    def json_at(self, revision, path):
        found = self.output(['show', f'{revision}:{path}'])
        try:
            return json.loads(found) if found else None
        except ValueError:
            return None

    def blob_sha256(self, revision, path):
        done = self.run(['cat-file', 'blob', f'{revision}:{path}'], text=False)
        if done is None or done.returncode != 0:
            return None
        return hashlib.sha256(done.stdout).hexdigest()

    def changes(self, revision):
        """Path -> git status letter for every file that differs between revision and the working tree.

        Tracked changes (committed, staged or not) come from git diff; untracked files count as
        added. Game data is ignored by .gitignore (**/bin/**), so ignored untracked files under
        src/bin/Data are listed as well.
        """
        diff = self.output(['diff', '--name-status', '--no-renames', '-z', revision, '--'])
        untracked = self.output(['ls-files', '--others', '--exclude-standard', '-z'])
        untracked_game = self.output(['ls-files', '--others', '-z', '--', GAME_DATA])
        if diff is None or untracked is None or untracked_game is None:
            return None
        fields = diff.split('\0')
        changes = {fields[i + 1]: fields[i] for i in range(0, len(fields) - 1, 2) if fields[i]}
        for path in (untracked + untracked_game).split('\0'):
            if path and Path(path).name != FINDER_METADATA:
                changes.setdefault(path, 'A')
        return changes


# --- small shape helpers --------------------------------------------------------------------

def check_keys(value, required, where, report, optional=frozenset()):
    """True when value is an object; reports missing and unknown keys."""
    if not isinstance(value, dict):
        report.error(f'{where}: expected an object')
        return False
    for key in sorted(required - set(value)):
        report.error(f'{where}: missing "{key}"')
    for key in sorted(set(value) - required - set(optional)):
        report.error(f'{where}: unknown field "{key}"')
    return True


def is_text(value):
    return isinstance(value, str) and value.strip() != ''


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def is_vector(value, size):
    return isinstance(value, list) and len(value) == size and all(is_number(v) for v in value)


def string_list(value, where, report, minimum=0):
    if not isinstance(value, list) or not all(is_text(v) for v in value):
        report.error(f'{where}: expected a list of non-empty strings')
        return []
    if len(value) < minimum:
        report.error(f'{where}: needs at least {minimum} entr{"y" if minimum == 1 else "ies"}')
    return value


def check_enum(value, allowed, where, report):
    if value not in allowed:
        report.error(f'{where}: {value!r} is not one of {", ".join(allowed)}')


def check_timestamp(value, where, report):
    """RFC 3339 date-time with seconds and a UTC offset or Z (for example 2026-09-23T10:15:00+02:00)."""
    if not isinstance(value, str) or not TIMESTAMP_PATTERN.match(value):
        report.error(f'{where}: {value!r} is not an RFC 3339 timestamp like 2026-09-23T10:15:00+02:00')
        return
    try:
        datetime.fromisoformat(value)
    except ValueError:
        report.error(f'{where}: {value!r} is not a valid date and time')


def check_pattern(value, pattern, where, report, nullable=False):
    if value is None and nullable:
        return
    if not isinstance(value, str) or not pattern.match(value):
        report.error(f'{where}: {value!r} does not match {pattern.pattern}')


def repo_path(value, where, report, must_exist=True):
    """Checks a repo-relative path; returns the absolute path or None."""
    if not is_text(value):
        report.error(f'{where}: expected a repo-relative path')
        return None
    parts = value.rstrip('/').split('/')
    if value.startswith('/') or '\\' in value or DRIVE_PATTERN.match(value) or '..' in parts:
        report.error(f'{where}: {value!r} must be relative to the repository root (no "/", "\\", "..")')
        return None
    path = ROOT / value
    if must_exist and not path.exists():
        report.error(f'{where}: {value} does not exist')
    return path


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def section(request, key):
    """request[key] when it is an object, else {} (shape errors are reported elsewhere)."""
    value = request.get(key)
    return value if isinstance(value, dict) else {}


def canonical(value):
    return json.dumps(value, sort_keys=True)


def is_protected(path, protected):
    return any(path == p or (p.endswith('/') and path.startswith(p)) for p in protected)


def jpeg_size(path):
    """(width, height) from the first start-of-frame marker, or None when path is not a JPEG."""
    data = path.read_bytes()
    if data[:2] != b'\xff\xd8':
        return None
    index = 2
    while index + 4 <= len(data):
        if data[index] != 0xFF:
            return None
        marker = data[index + 1]
        if marker == 0xFF:
            index += 1
            continue
        if marker == 0x01 or 0xD0 <= marker <= 0xD7:
            index += 2
            continue
        if marker in (0xD9, 0xDA):
            return None
        if marker in JPEG_SOF_MARKERS and index + 9 <= len(data):
            height = int.from_bytes(data[index + 5:index + 7], 'big')
            width = int.from_bytes(data[index + 7:index + 9], 'big')
            return width, height
        index += 2 + int.from_bytes(data[index + 2:index + 4], 'big')
    return None


# --- reference data -------------------------------------------------------------------------

def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def optional_json(path, problems):
    """Parsed JSON, or None when the file is missing or unreadable (then a problem is recorded)."""
    if not path.exists():
        return None
    try:
        return read_json(path)
    except ValueError as error:
        problems.append(f'{path.relative_to(ROOT).as_posix()} is not valid JSON ({error})')
        return None


def catalog_by_name(catalog):
    """Model name -> catalog.json entry, or None when catalog.json is missing."""
    if not isinstance(catalog, dict):
        return None
    entries = list(catalog.get('models', {}).values()) + list(catalog.get('untyped_models', {}).values())
    return {entry['name']: entry for entry in entries}


def load_reference():
    dependency = read_json(DEPENDENCY_MAP)
    problems = []
    catalog = optional_json(CATALOG, problems)
    reservations = optional_json(PRODUCTION_BATCHES, problems)
    return {'models': dependency['models'], 'consumers': dependency['texture_consumers'],
            'catalog': catalog_by_name(catalog),
            'reservations': reservations if isinstance(reservations, dict) else {},
            'object_files': {p.name.lower() for p in OBJECT_DIR.iterdir()} if OBJECT_DIR.is_dir() else set(),
            'git': Git(), 'problems': problems}


# --- sections -------------------------------------------------------------------------------

def check_top_level(request, report):
    check_keys(request, TOP_KEYS, 'request', report, TOP_OPTIONAL)
    if request.get('schema') != SCHEMA:
        report.error(f'schema: expected "{SCHEMA}"')
    if request.get('world') != WORLD_NUMBER:
        report.error(f'world: expected {WORLD_NUMBER}')
    if not is_text(request.get('author')):
        report.error('author: expected a non-empty string')
    check_timestamp(request.get('created'), 'created', report)
    check_enum(request.get('status'), STATUSES, 'status', report)
    check_enum(request.get('priority'), PRIORITIES, 'priority', report)
    check_enum(request.get('kind'), KINDS, 'kind', report)
    check_pattern(request.get('base_commit'), COMMIT_PATTERN, 'base_commit', report)
    supersedes = request.get('supersedes')
    if supersedes is not None and not (isinstance(supersedes, str) and ID_PATTERN.match(supersedes)):
        report.error('supersedes: expected null or a request id')


def check_id(request, folder, report):
    request_id = request.get('id')
    match = ID_PATTERN.match(request_id) if isinstance(request_id, str) else None
    if not match or len(request_id) > ID_MAX_LENGTH:
        report.error(f'id: {request_id!r} is not <YYYY-MM-DD>-<model>-<slug> (lower case, a slug of one to '
                     f'five words, <= {ID_MAX_LENGTH} chars)')
        return None
    if request_id != folder.name:
        report.error(f'id: {request_id} differs from the folder name {folder.name}')
    try:
        date.fromisoformat(match.group(1))
    except ValueError:
        report.error(f'id: {match.group(1)} is not a calendar date')
    created = request.get('created')
    if isinstance(created, str) and not created.startswith(match.group(1)):
        report.warn(f'id date {match.group(1)} differs from created {created}')
    targets = request.get('targets')
    first = targets[0].get('model') if isinstance(targets, list) and targets and isinstance(targets[0], dict) else None
    if isinstance(first, str) and first.lower() != match.group(2):
        report.error(f'id: model part "{match.group(2)}" must be the first target in lower case ({first.lower()})')
    return request_id


def check_history(request, report):
    history = request.get('status_history')
    if not isinstance(history, list) or not history:
        report.error('status_history: expected a non-empty list')
        return
    for index, entry in enumerate(history):
        where = f'status_history[{index}]'
        if not check_keys(entry, HISTORY_KEYS, where, report, {'note'}):
            continue
        check_enum(entry.get('status'), STATUSES, f'{where}.status', report)
        check_timestamp(entry.get('at'), f'{where}.at', report)
        if not is_text(entry.get('by')):
            report.error(f'{where}.by: expected a non-empty string')
    first = history[0] if isinstance(history[0], dict) else {}
    last = history[-1] if isinstance(history[-1], dict) else {}
    if first.get('status') != 'open':
        report.error('status_history: the first entry must be "open"')
    if last.get('status') != request.get('status'):
        report.error('status_history: the last entry must equal status')


def check_placement(value, where, report):
    if not check_keys(value, PLACEMENT_KEYS, where, report, {'obj_index', 'tile'}):
        return
    if not is_vector(value.get('position'), 3) or not is_vector(value.get('rotation'), 3):
        report.error(f'{where}: position and rotation must be three numbers')
    if not is_number(value.get('scale')):
        report.error(f'{where}.scale: expected a number')
    if 'tile' in value and not is_vector(value['tile'], 2):
        report.error(f'{where}.tile: expected two numbers')
    index = value.get('obj_index', 0)
    if not isinstance(index, int) or isinstance(index, bool) or index < 0:
        report.error(f'{where}.obj_index: expected a non-negative integer')


def expected_textures(model):
    return {name: paths[0] for name, paths in model['textures'].items() if paths}


def check_target_against_map(target, where, model, report):
    if model['scope_exclusion']:
        report.error(f'{where}: {target["model"]} is excluded ({model["scope_exclusion"]}); it cannot be requested')
    if target.get('type') != model['type']:
        report.error(f'{where}.type: {target.get("type")!r} but the dependency map says {model["type"]}')
    if target.get('bmd') != model['path']:
        report.error(f'{where}.bmd: {target.get("bmd")!r} but the dependency map says {model["path"]}')
    if target.get('textures') != expected_textures(model):
        report.error(f'{where}.textures: must equal the dependency map / catalog textures of {target["model"]}')
    count = target.get('placement_count')
    if not isinstance(count, int) or isinstance(count, bool) or count < 0:
        report.error(f'{where}.placement_count: expected a non-negative integer')


def check_target_paths(target, where, report):
    check_pattern(target.get('bmd'), BMD_PATTERN, f'{where}.bmd', report)
    repo_path(target.get('bmd'), f'{where}.bmd', report)
    textures = target.get('textures')
    if not isinstance(textures, dict) or not textures:
        report.error(f'{where}.textures: expected a non-empty object')
        textures = {}
    for name, container in textures.items():
        check_pattern(container, CONTAINER_PATTERN, f'{where}.textures.{name}', report)
        repo_path(container, f'{where}.textures.{name}', report)
    check_pattern(target.get('current_sha256'), SHA256_PATTERN, f'{where}.current_sha256', report)
    if target.get('model_dir') is not None:
        path = repo_path(target['model_dir'], f'{where}.model_dir', report)
        if path and path.exists() and not path.is_dir():
            report.error(f'{where}.model_dir: not a folder')
    last_batch = target.get('last_batch')
    if last_batch is not None and not is_text(last_batch):
        report.error(f'{where}.last_batch: expected null or a batch name')
    original = target.get('original')
    if check_keys(original, {'revision', 'archive'}, f'{where}.original', report):
        check_pattern(original.get('revision'), COMMIT_PATTERN, f'{where}.original.revision', report)
        if original.get('archive') is not None:
            repo_path(original['archive'], f'{where}.original.archive', report)


def check_targets(request, models, report):
    targets = request.get('targets')
    if not isinstance(targets, list) or not targets:
        report.error('targets: expected a non-empty list')
        return []
    names = []
    for index, target in enumerate(targets):
        where = f'targets[{index}]'
        if not check_keys(target, TARGET_KEYS, where, report):
            continue
        name = target.get('model')
        if not isinstance(name, str) or not MODEL_PATTERN.match(name) or name not in models:
            report.error(f'{where}.model: {name!r} is not a World1 model in the dependency map')
            continue
        if name in names:
            report.error(f'{where}.model: {name} is listed twice')
        names.append(name)
        check_target_against_map(target, where, models[name], report)
        check_target_paths(target, where, report)
        picked = target.get('picked_instances')
        if not isinstance(picked, list):
            report.error(f'{where}.picked_instances: expected a list')
            continue
        for number, instance in enumerate(picked):
            check_placement(instance, f'{where}.picked_instances[{number}]', report)
    return names


def check_change(request, report):
    change = request.get('change')
    if not check_keys(change, CHANGE_KEYS, 'change', report, {'reference_images', 'new_model'}):
        return
    if not is_text(change.get('summary')):
        report.error('change.summary: expected a non-empty string')
    for key in ('details', 'keep', 'avoid'):
        string_list(change.get(key), f'change.{key}', report)
    references = change.get('reference_images', [])
    if not isinstance(references, list):
        report.error('change.reference_images: expected a list')
        references = []
    for index, path in enumerate(references):
        repo_path(path, f'change.reference_images[{index}]', report)


def other_new_models(folder):
    """change.new_model of every other request folder (new-variant requests only)."""
    names = {}
    for path in sorted(folder.parent.glob(f'*/{REQUEST_FILE}')):
        if path.parent == folder:
            continue
        try:
            other = read_json(path)
        except ValueError:
            continue
        change = other.get('change') if isinstance(other, dict) else None
        if isinstance(change, dict) and isinstance(change.get('new_model'), str):
            names[change['new_model'].lower()] = path.parent.name
    return names


def check_new_variant(request, names, folder, reference, report):
    """A new-variant names one free model <Family><NN> derived from its single target."""
    change = section(request, 'change')
    new_model = change.get('new_model')
    if request.get('kind') != NEW_VARIANT:
        if 'new_model' in change:
            report.error('change.new_model: only a new-variant request names a new model')
        return None
    if len(names) != 1:
        report.error('targets: a new-variant request has exactly one target, the model it derives from')
    match = NEW_MODEL_PATTERN.match(new_model) if isinstance(new_model, str) else None
    if not match:
        report.error('change.new_model: a new-variant request needs the new model name, <Family><NN> (e.g. Sign03)')
        return None
    source = TRAILING_DIGITS.match(names[0]) if names else None
    family = source.group(1) if source else None
    if family is not None and match.group(1) != family:
        report.error(f'change.new_model: {new_model} must keep the family of {names[0]} ({family}<NN>)')
    if new_model in reference['models']:
        report.error(f'change.new_model: {new_model} is already a World1 model')
    if f'{new_model.lower()}.bmd' in reference['object_files']:
        report.error(f'change.new_model: src/bin/Data/Object1/{new_model}.bmd already exists')
    taken = other_new_models(folder).get(new_model.lower())
    if taken:
        report.error(f'change.new_model: {new_model} is already requested by {taken}')
    return new_model


def target_containers(names, models):
    containers = {}
    for name in names:
        for container in expected_textures(models[name]).values():
            containers.setdefault(container, set()).add(name)
    return containers


def check_shared_and_frozen(constraints, names, containers, consumers, report):
    expected_shared = {c: sorted(consumers.get(c, [])) for c in containers if len(consumers.get(c, [])) > 1}
    shared = constraints.get('shared_textures')
    if not isinstance(shared, dict):
        report.error('constraints.shared_textures: expected an object')
    elif {k: sorted(v) if isinstance(v, list) else v for k, v in shared.items()} != expected_shared:
        report.error('constraints.shared_textures: must list every shared target container with all its '
                     f'consumers: expected {json.dumps(expected_shared, sort_keys=True)}')
    frozen = string_list(constraints.get('frozen_textures'), 'constraints.frozen_textures', report)
    for index, path in enumerate(frozen):
        check_pattern(path, CONTAINER_PATTERN, f'constraints.frozen_textures[{index}]', report)
        repo_path(path, f'constraints.frozen_textures[{index}]', report)
    must_freeze = sorted(c for c in containers if set(consumers.get(c, [])) - set(names))
    missing = sorted(set(must_freeze) - set(frozen))
    if missing:
        report.error(f'constraints.frozen_textures: shared with models outside the targets, must be frozen: {missing}')
    stray = sorted(set(frozen) - set(containers))
    if stray:
        report.warn(f'constraints.frozen_textures: not a texture of any target: {stray}')
    return set(frozen)


def check_owned(request, constraints, names, containers, frozen, models, report):
    owned = string_list(constraints.get('owned_files'), 'constraints.owned_files', report)
    bmds = {models[name]['path'] for name in names}
    allowed = bmds | set(containers)
    for index, path in enumerate(owned):
        check_pattern(path, GAME_PATTERN, f'constraints.owned_files[{index}]', report)
        repo_path(path, f'constraints.owned_files[{index}]', report)
    if len(set(owned)) != len(owned):
        report.error('constraints.owned_files: duplicate entries')
    outside = sorted(set(owned) - allowed)
    if outside:
        report.error(f'constraints.owned_files: not a target BMD or target texture: {outside}')
    overlap = sorted(set(owned) & frozen)
    if overlap:
        report.error(f'constraints.owned_files: frozen textures cannot be owned: {overlap}')
    kind = request.get('kind')
    owned_bmds = sorted(set(owned) & bmds)
    owned_textures = sorted(set(owned) & set(containers))
    if owned_bmds and kind not in KINDS_WITH_BMD:
        report.error(f'constraints.owned_files: kind {kind!r} may not replace BMDs: {owned_bmds}')
    if owned_textures and kind not in KINDS_WITH_TEXTURES:
        report.error(f'constraints.owned_files: kind {kind!r} may not replace textures: {owned_textures}')
    if kind == NEW_VARIANT and owned:
        report.error('constraints.owned_files: a new-variant request installs nothing; leave it empty')
    return set(owned)


def check_protected(constraints, owned, git, report):
    """Protected paths exist in the working tree or, for sparse checkouts, in HEAD."""
    protected = string_list(constraints.get('protected_paths'), 'constraints.protected_paths', report)
    for index, path in enumerate(protected):
        found = repo_path(path, f'constraints.protected_paths[{index}]', report, must_exist=False)
        if found and not found.exists() and not git.has_path('HEAD', path.rstrip('/')):
            report.error(f'constraints.protected_paths[{index}]: {path} does not exist')
    if REQUIRED_PROTECTED not in protected:
        report.error(f'constraints.protected_paths: must contain {REQUIRED_PROTECTED} (placement and terrain are never art requests)')
    for path in sorted(owned):
        if is_protected(path, protected):
            report.error(f'constraints.owned_files: {path} is inside a protected path')
    return protected


def check_engine_control_shape(constraints, names, report):
    rows = constraints.get('engine_controls')
    if not isinstance(rows, list):
        report.error('constraints.engine_controls: expected a list')
        return
    for index, row in enumerate(rows):
        where = f'constraints.engine_controls[{index}]'
        if not check_keys(row, CONTROL_KEYS, where, report, {'blend_mesh', 'blend_mesh_texture'}):
            continue
        if row.get('model') not in names:
            report.error(f'{where}.model: {row.get("model")!r} is not a target')
        if not is_text(row.get('source_row')):
            report.error(f'{where}.source_row: expected a non-empty string')
        string_list(row.get('controls'), f'{where}.controls', report)
        if not is_text(row.get('requirement')):
            report.error(f'{where}.requirement: expected a non-empty string')
        blend = row.get('blend_mesh')
        if blend is not None and (not isinstance(blend, int) or isinstance(blend, bool) or blend < 0):
            report.error(f'{where}.blend_mesh: expected a non-negative integer')


def check_constraints(request, names, models, consumers, git, report):
    constraints = request.get('constraints')
    if not check_keys(constraints, CONSTRAINT_KEYS, 'constraints', report):
        return set(), set(), []
    containers = target_containers(names, models)
    frozen = check_shared_and_frozen(constraints, names, containers, consumers, report)
    owned = check_owned(request, constraints, names, containers, frozen, models, report)
    protected = check_protected(constraints, owned, git, report)
    check_engine_control_shape(constraints, names, report)
    string_list(constraints.get('must_keep'), 'constraints.must_keep', report, minimum=1)
    return owned, frozen, protected


def check_catalog_facts(request, names, catalog, report):
    """A live request still describes the targets exactly as catalog.json does."""
    if request.get('status') not in LIVE_STATUSES:
        return
    if catalog is None:
        report.error('catalog.json is missing; run coordination/build_editor_catalog.py')
        return
    targets = request.get('targets') if isinstance(request.get('targets'), list) else []
    targets = {t.get('model'): t for t in targets if isinstance(t, dict)}
    expected_rows = []
    for name in names:
        entry, target = catalog.get(name), targets.get(name, {})
        if entry is None:
            report.error(f'{name}: not in catalog.json')
            continue
        for field in TARGET_CATALOG_FIELDS:
            if target.get(field) != entry.get(field):
                report.error(f'{name}.{field}: {target.get(field)!r} but catalog.json says {entry.get(field)!r}')
        original = target.get('original') if isinstance(target.get('original'), dict) else {}
        for field in ('revision', 'archive'):
            if original.get(field) != entry['original'].get(field):
                report.error(f'{name}.original.{field}: {original.get(field)!r} but catalog.json says '
                             f'{entry["original"].get(field)!r}')
        expected_rows += [dict(row, model=name) for row in entry.get('engine_controls', [])]
    rows = section(request, 'constraints').get('engine_controls')
    if not isinstance(rows, list):
        return
    actual = {canonical(row) for row in rows if isinstance(row, dict)}
    wanted = {canonical(row) for row in expected_rows}
    for row in sorted(wanted - actual):
        report.error(f'constraints.engine_controls: missing or altered catalog row {row}')
    for row in sorted(actual - wanted):
        report.error(f'constraints.engine_controls: row not in catalog.json {row}')


def check_capture(capture, where, request_id, report):
    if not check_keys(capture, CAPTURE_KEYS, where, report, {'hero_tile', 'note'}):
        return
    file = capture.get('file')
    match = CAPTURE_PATTERN.match(file) if isinstance(file, str) else None
    path = None
    if not match or match.group(1) != request_id:
        report.error(f'{where}.file: expected assets-work/World1/requests/{request_id}/captures/<name>.jpg')
    else:
        path = repo_path(capture['file'], f'{where}.file', report)
    check_enum(capture.get('variant'), CAPTURE_VARIANTS, f'{where}.variant', report)
    camera = capture.get('camera')
    if not isinstance(camera, dict) or not is_vector(camera.get('position'), 3) or not is_vector(camera.get('angle'), 3):
        report.error(f'{where}.camera: needs position and angle (three numbers each)')
    resolution = capture.get('resolution')
    if not (isinstance(resolution, list) and len(resolution) == 2
            and all(isinstance(v, int) and not isinstance(v, bool) and v > 0 for v in resolution)):
        report.error(f'{where}.resolution: expected [width, height]')
        resolution = None
    elif resolution[0] > MAX_CAPTURE_WIDTH:
        report.error(f'{where}.resolution: at most {MAX_CAPTURE_WIDTH} px wide')
    if capture.get('hero_tile') is not None and not is_vector(capture['hero_tile'], 2):
        report.error(f'{where}.hero_tile: expected null or two numbers')
    check_pattern(capture.get('client_commit'), COMMIT_PATTERN, f'{where}.client_commit', report, nullable=True)
    if path is None or not path.is_file():
        return
    size = jpeg_size(path)
    if size is None:
        report.error(f'{where}.file: {file} is not a JPEG')
    elif size[0] > MAX_CAPTURE_WIDTH:
        report.error(f'{where}.file: {file} is {size[0]} px wide (at most {MAX_CAPTURE_WIDTH})')
    elif resolution is not None and list(size) != resolution:
        report.error(f'{where}.resolution: {resolution} but {file} is {list(size)}')


def check_evidence(request, request_id, report):
    evidence = request.get('evidence')
    if not check_keys(evidence, EVIDENCE_KEYS, 'evidence', report):
        return
    captures = evidence.get('captures')
    if not isinstance(captures, list):
        report.error('evidence.captures: expected a list')
        captures = []
    for index, capture in enumerate(captures):
        check_capture(capture, f'evidence.captures[{index}]', request_id, report)
    if not captures:
        report.warn('evidence.captures: no in-client capture attached')
    previews = evidence.get('offline_previews')
    if not isinstance(previews, list):
        report.error('evidence.offline_previews: expected a list')
        return
    for index, path in enumerate(previews):
        repo_path(path, f'evidence.offline_previews[{index}]', report)


def sibling_collisions(request_id, folder):
    """Other request folders whose <model>-<slug> (and therefore branch name) equals this one."""
    suffix = request_id[ID_DATE_LENGTH:]
    return sorted(p.parent.name for p in folder.parent.glob(f'*/{REQUEST_FILE}')
                  if p.parent != folder and p.parent.name[ID_DATE_LENGTH:] == suffix)


def check_handoff(request, request_id, folder, report):
    handoff = request.get('handoff')
    if not check_keys(handoff, HANDOFF_KEYS, 'handoff', report):
        return
    name = request_id[ID_DATE_LENGTH + 1:]
    expected = {'repo': REPO, 'branch': f'codex/lorencia-req-{name}',
                'worktree': f'../MuMain-lorencia-req-{name}',
                'deliver_to': f'assets-work/World1/requests/{request_id}/delivery/'}
    for key, value in expected.items():
        if handoff.get(key) != value:
            report.error(f'handoff.{key}: expected {value!r}, found {handoff.get(key)!r}')
    if not isinstance(handoff.get('push_allowed'), bool):
        report.error('handoff.push_allowed: expected true or false')
    if handoff.get('claimed_by') is not None and not is_text(handoff['claimed_by']):
        report.error('handoff.claimed_by: expected null or a non-empty string')
    if handoff.get('claimed_at') is not None:
        check_timestamp(handoff['claimed_at'], 'handoff.claimed_at', report)
    check_pattern(handoff.get('start_commit'), FULL_COMMIT_PATTERN, 'handoff.start_commit', report, nullable=True)
    collisions = sibling_collisions(request_id, folder)
    if collisions:
        report.error(f'handoff.branch: {expected["branch"]} is also used by {collisions}; pick another slug')


def delivery_folders(request, names, new_model):
    """Delivery folder name -> the target it replaces or derives from."""
    if request.get('kind') == NEW_VARIANT:
        return {new_model: names[0]} if new_model and names else {}
    return {name: name for name in names}


def check_delivery_layout(request, folders, report):
    deliver_to = section(request, 'handoff').get('deliver_to')
    if not isinstance(deliver_to, str):
        return
    for folder in folders:
        for item in DELIVERY_LAYOUT:
            path = f'{deliver_to}{folder}/{item}'
            if not (ROOT / path).exists():
                report.error(f'delivery: {path} is missing (per-model layout: {", ".join(DELIVERY_LAYOUT)})')


def check_new_variant_exports(request, folders, reference, report):
    """New-variant exports: <NewModel>.bmd plus new containers named <newmodel>_*, never existing files."""
    if request.get('kind') != NEW_VARIANT:
        return
    deliver_to = section(request, 'handoff').get('deliver_to') or ''
    for new_model in folders:
        exports = ROOT / f'{deliver_to}{new_model}/exports'
        if not (exports / f'{new_model}.bmd').is_file():
            report.error(f'delivery: {deliver_to}{new_model}/exports/{new_model}.bmd is missing')
        if not exports.is_dir():
            continue
        for path in sorted(exports.iterdir()):
            if path.suffix.lower() not in TEXTURE_SUFFIXES:
                continue
            if not path.name.lower().startswith(new_model.lower() + '_'):
                report.error(f'delivery: new texture {path.name} must be named {new_model.lower()}_<name>')
            if path.name.lower() in reference['object_files']:
                report.error(f'delivery: new texture {path.name} already exists in src/bin/Data/Object1')
            if TEXTURE_FLAG.search(path.stem):
                report.error(f'delivery: new texture {path.name} ends in an engine name flag (_R/_H/_S/_N)')


def check_installed(request, result, owned, report):
    installed = result.get('installed_files')
    if not isinstance(installed, dict):
        report.error('result.installed_files: expected an object')
        return {}
    deliver_to = section(request, 'handoff').get('deliver_to') or ''
    if request.get('kind') == NEW_VARIANT and installed:
        report.error('result.installed_files: a new-variant request installs nothing')
    for game, export in sorted(installed.items()):
        where = f'result.installed_files[{game}]'
        if game not in owned:
            report.error(f'{where}: not in constraints.owned_files')
        game_path = repo_path(game, where, report)
        export_path = repo_path(export, where, report)
        if isinstance(export, str) and not export.startswith(deliver_to):
            report.error(f'{where}: export {export} is outside {deliver_to}')
        ready = game_path and export_path and game_path.is_file() and export_path.is_file()
        if request.get('status') == 'delivered' and ready and sha256_file(game_path) != sha256_file(export_path):
            report.error(f'{where}: installed file differs from its delivered export')
    return installed


def check_result(request, owned, folders, reference, report):
    result = request.get('result')
    if result is None:
        return {}
    if not check_keys(result, RESULT_KEYS, 'result', report):
        return {}
    check_timestamp(result.get('delivered_at'), 'result.delivered_at', report)
    commits = result.get('source_commits')
    if not isinstance(commits, list) or not commits:
        report.error('result.source_commits: expected a non-empty list')
    for index, commit in enumerate(commits or []):
        check_pattern(commit, COMMIT_PATTERN, f'result.source_commits[{index}]', report)
    installed = check_installed(request, result, owned, report)
    for key, minimum in (('validation', 1), ('review_images', 0), ('notes', 1)):
        for index, path in enumerate(string_list(result.get(key), f'result.{key}', report, minimum)):
            repo_path(path, f'result.{key}[{index}]', report)
    pr_url = result.get('pr_url')
    check_pattern(pr_url, PR_URL_PATTERN, 'result.pr_url', report, nullable=True)
    if pr_url and not section(request, 'handoff').get('push_allowed'):
        report.warn('result.pr_url is set while handoff.push_allowed is false; confirm the owner authorized it')
    check_delivery_layout(request, folders, report)
    check_new_variant_exports(request, folders, reference, report)
    return installed


def check_decision(request, request_id, report):
    decision = request.get('decision')
    if decision is None:
        return
    if not check_keys(decision, DECISION_KEYS, 'decision', report):
        return
    check_enum(decision.get('status'), DECISION_STATUSES, 'decision.status', report)
    check_timestamp(decision.get('at'), 'decision.at', report)
    if not is_text(decision.get('by')):
        report.error('decision.by: expected a non-empty string')
    if not isinstance(decision.get('reason'), str):
        report.error('decision.reason: expected a string')
    ledger = decision.get('ledger_entry')
    expected = f'requests/{request_id}/delivery'
    if decision.get('status') == 'accepted' and ledger != expected:
        report.error(f'decision.ledger_entry: an accepted request is integrated as ledger entry "{expected}"')
    if decision.get('status') != 'accepted' and ledger is not None:
        report.error('decision.ledger_entry: only accepted requests have a ledger entry')


def check_status_fields(request, report):
    """Which of the claim, result and decision must be set for the current status."""
    status = request.get('status')
    handoff = section(request, 'handoff')
    claim = [handoff.get(key) for key in ('claimed_by', 'claimed_at', 'start_commit')]
    claimed = all(value is not None for value in claim)
    unclaimed = all(value is None for value in claim)
    result, decision = request.get('result'), request.get('decision')
    rules = {
        'open': (unclaimed and result is None and decision is None,
                 'an open request has no claim (claimed_by, claimed_at, start_commit), result or decision'),
        'claimed': (claimed and result is None and decision is None,
                    'a claimed request has claimed_by, claimed_at and start_commit, and no result or decision'),
        'delivered': (claimed and isinstance(result, dict) and decision is None,
                      'a delivered request has a claim (with start_commit) and a result, and no decision'),
        'accepted': (claimed and isinstance(result, dict) and isinstance(decision, dict)
                     and decision.get('status') == 'accepted',
                     'an accepted request has the claim and result merged from the worker branch and an accepted decision'),
        'rejected': (isinstance(decision, dict) and decision.get('status') == 'rejected',
                     'a rejected request has a rejected decision'),
        'withdrawn': (isinstance(decision, dict) and decision.get('status') == 'withdrawn',
                      'a withdrawn request has a withdrawn decision'),
    }
    if status in rules and not rules[status][0]:
        report.error(f'status {status}: {rules[status][1]}')


def check_assignment(request, request_id, reservations, report):
    """A claimed or delivered request is reserved for exactly the worker that claimed it."""
    if request.get('status') not in WORKER_STATUSES:
        return
    ledger = f'requests/{request_id}/delivery'
    agents = sorted(agent for agent, claim in reservations.items()
                    if isinstance(claim, dict) and claim.get('ledger') == ledger)
    claimed_by = section(request, 'handoff').get('claimed_by')
    if not agents:
        report.error(f'handoff.claimed_by: no reservation with "ledger": "{ledger}" in '
                     'coordination/production-batches.json; take only a request the coordinator assigned to you')
    elif claimed_by not in agents:
        report.error(f'handoff.claimed_by: {claimed_by!r} but the coordinator reserved this request for {agents}')


# --- git checks -----------------------------------------------------------------------------

def check_base(request, git, report):
    """base_commit exists and holds the bytes the owner saw (targets[].current_sha256)."""
    base = request.get('base_commit')
    if not isinstance(base, str) or not COMMIT_PATTERN.match(base):
        return None
    full = git.commit(base)
    if not git.available:
        report.warn('git is not available; commits, originals and the worker scope were not checked')
        return None
    if full is None:
        report.error(f'base_commit: {base} is not a commit in this clone (it must be pushed to origin)')
        return None
    targets = request.get('targets') if isinstance(request.get('targets'), list) else []
    for index, target in enumerate(targets):
        if not isinstance(target, dict) or not isinstance(target.get('bmd'), str):
            continue
        digest = git.blob_sha256(full, target['bmd'])
        if digest is None:
            report.error(f'targets[{index}].bmd: {target["bmd"]} does not exist at base_commit')
        elif target.get('current_sha256') != digest:
            report.error(f'targets[{index}].current_sha256: differs from {target["bmd"]} at base_commit ({digest})')
    return full


def request_files(names, owned, models):
    """Target BMDs, their texture containers and the owned files: the bytes a worker starts from."""
    files = set(owned)
    for name in names:
        files.add(models[name]['path'])
        files.update(expected_textures(models[name]).values())
    return sorted(files)


def check_start(request, request_id, names, owned, models, base, git, report):
    """start_commit: the main commit the worker branched from; same request bytes as base_commit."""
    if request.get('status') not in STARTED_STATUSES or base is None:
        return None
    start = section(request, 'handoff').get('start_commit')
    if not isinstance(start, str) or not FULL_COMMIT_PATTERN.match(start):
        return None
    if git.commit(start) is None:
        report.error(f'handoff.start_commit: {start} is not a commit in this clone')
        return None
    if request.get('status') in WORKER_STATUSES and not git.is_ancestor(start, 'HEAD'):
        report.error(f'handoff.start_commit: {start} is not an ancestor of HEAD; validate on the request branch')
        return None
    filed = f'assets-work/World1/requests/{request_id}/{REQUEST_FILE}'
    if git.blob_id(start, filed) is None:
        report.error(f'handoff.start_commit: {start} does not contain {filed}; branch from the main commit '
                     'that contains the filed request')
    stale = [path for path in request_files(names, owned, models) if git.blob_id(start, path) != git.blob_id(base, path)]
    if stale:
        report.error(f'handoff.start_commit: these files changed between base_commit and start_commit, so the '
                     f'request is stale; stop and ask the owner to file it again: {stale}')
    return start


def check_frozen(request, request_id, start, git, report):
    """Fields fixed at filing are unchanged since start_commit, and status_history only grew."""
    if start is None:
        return
    filed = git.json_at(start, f'assets-work/World1/requests/{request_id}/{REQUEST_FILE}')
    if not isinstance(filed, dict):
        return
    if filed.get('status') != 'open':
        report.error(f'handoff.start_commit: the request is {filed.get("status")!r} there, not open')
    for key in FROZEN_KEYS:
        if filed.get(key) != request.get(key):
            report.error(f'{key}: changed since the request was filed (start_commit); it is frozen')
    filed_handoff = filed.get('handoff') if isinstance(filed.get('handoff'), dict) else {}
    handoff = section(request, 'handoff')
    for key in FROZEN_HANDOFF_KEYS:
        if filed_handoff.get(key) != handoff.get(key):
            report.error(f'handoff.{key}: changed since the request was filed (start_commit); it is frozen')
    before = filed.get('status_history') if isinstance(filed.get('status_history'), list) else []
    history = request.get('status_history') if isinstance(request.get('status_history'), list) else []
    if history[:len(before)] != before:
        report.error('status_history: entries recorded before start_commit changed; it is append-only')


def check_scope(request, request_id, owned, frozen, protected, installed, start, git, report):
    """On the worker branch, only request.json, delivery/, owned files and the work log may differ from start_commit."""
    if request.get('status') not in WORKER_STATUSES or start is None:
        return
    changes = git.changes(start)
    if changes is None:
        report.error('scope: git could not list the changes since start_commit')
        return
    own_folder = f'assets-work/World1/requests/{request_id}/'
    for path, status in sorted(changes.items()):
        if path == WORKLOG:
            continue
        if path.startswith(own_folder):
            inner = path[len(own_folder):]
            if inner != REQUEST_FILE and not inner.startswith(DELIVERY_DIR):
                report.error(f'scope: {path} belongs to the owner; the worker writes only {REQUEST_FILE} and {DELIVERY_DIR}')
            continue
        if path in owned:
            if status != 'M':
                report.error(f'scope: {path} was {"added" if status == "A" else "deleted"}; owned files are only replaced')
            elif request.get('status') == 'delivered' and path not in installed:
                report.error(f'scope: {path} changed but is not listed in result.installed_files')
        elif path in frozen:
            report.error(f'scope: frozen texture {path} changed')
        elif is_protected(path, protected):
            report.error(f'scope: protected path {path} changed')
        elif path.startswith(GAME_DATA + '/'):
            report.error(f'scope: game file {path} changed but is not in constraints.owned_files')
        else:
            report.error(f'scope: {path} changed outside the request (only {own_folder}{REQUEST_FILE}, '
                         f'{own_folder}{DELIVERY_DIR}, owned files and {WORKLOG})')


def check_originals(request, folders, owned, models, base, git, report):
    """delivery/<Folder>/original/ holds the target BMD and its owned containers exactly as at base_commit."""
    if request.get('result') is None or base is None:
        return
    deliver_to = section(request, 'handoff').get('deliver_to')
    if not isinstance(deliver_to, str):
        return
    for folder, name in sorted(folders.items()):
        model = models[name]
        wanted = [model['path']] + sorted(c for c in set(expected_textures(model).values()) if c in owned)
        for game in wanted:
            filename = f'{name}.bmd' if game == model['path'] else Path(game).name
            path = ROOT / f'{deliver_to}{folder}/original/{filename}'
            if not path.is_file():
                report.error(f'delivery: {deliver_to}{folder}/original/{filename} is missing (the untouched {game})')
            elif sha256_file(path) != git.blob_sha256(base, game):
                report.error(f'delivery: {deliver_to}{folder}/original/{filename} differs from {game} at base_commit')


# --- driver ---------------------------------------------------------------------------------

def check_folder(folder, report):
    if not (folder / BRIEF_FILE).is_file():
        report.error(f'{BRIEF_FILE} is missing next to {REQUEST_FILE}')
    if folder.parent.resolve() != HERE:
        report.warn(f'the request folder is not directly under {HERE.relative_to(ROOT).as_posix()}/')


def validate(folder, reference):
    report = Report()
    try:
        request = read_json(folder / REQUEST_FILE)
    except (OSError, ValueError) as error:
        report.error(f'cannot read {REQUEST_FILE}: {error}')
        return report
    if not isinstance(request, dict):
        report.error(f'{REQUEST_FILE} must contain a JSON object')
        return report
    models, git = reference['models'], reference['git']
    for problem in reference['problems']:
        report.error(problem)
    check_folder(folder, report)
    check_top_level(request, report)
    request_id = check_id(request, folder, report) or folder.name
    check_history(request, report)
    names = check_targets(request, models, report)
    check_change(request, report)
    new_model = check_new_variant(request, names, folder, reference, report)
    owned, frozen, protected = check_constraints(request, names, models, reference['consumers'], git, report)
    check_catalog_facts(request, names, reference['catalog'], report)
    check_evidence(request, request_id, report)
    check_handoff(request, request_id, folder, report)
    folders = delivery_folders(request, names, new_model)
    installed = check_result(request, owned, folders, reference, report)
    check_decision(request, request_id, report)
    check_status_fields(request, report)
    check_assignment(request, request_id, reference['reservations'], report)
    base = check_base(request, git, report)
    start = check_start(request, request_id, names, owned, models, base, git, report)
    check_frozen(request, request_id, start, git, report)
    check_scope(request, request_id, owned, frozen, protected, installed, start, git, report)
    check_originals(request, folders, owned, models, base, git, report)
    return report


def request_folders(args):
    if args.all:
        return sorted(p.parent for p in HERE.glob(f'*/{REQUEST_FILE}'))
    folders = []
    for value in args.paths:
        path = Path(value).resolve()
        folders.append(path.parent if path.name == REQUEST_FILE else path)
    return folders


def display(folder):
    try:
        return folder.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(folder)


def main(argv=None):
    parser = argparse.ArgumentParser(description='Validate World1 regeneration requests.')
    parser.add_argument('paths', nargs='*', help='request folder(s) or request.json file(s)')
    parser.add_argument('--all', action='store_true', help='validate every folder under requests/')
    args = parser.parse_args(argv)
    if not args.paths and not args.all:
        parser.error('give a request folder or request.json, or --all')
    folders = request_folders(args)
    if not folders:
        print('no requests found')
        return 0
    reference = load_reference()
    failed = 0
    for folder in folders:
        report = validate(folder, reference)
        verdict = 'INVALID' if report.errors else 'OK'
        print(f'{display(folder)}: {verdict} ({len(report.errors)} errors, {len(report.warnings)} warnings)')
        for message in report.errors:
            print(f'  error: {message}')
        for message in report.warnings:
            print(f'  warning: {message}')
        failed += bool(report.errors)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
