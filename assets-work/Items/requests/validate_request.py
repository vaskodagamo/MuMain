"""Validate item regeneration requests (schema "mu-item-regen-request/1"); standard library only.

Usage (from anywhere):
    python3 assets-work/Items/requests/validate_request.py <request-dir or request.json> [...]
    python3 assets-work/Items/requests/validate_request.py --all

Checks the shape described by request.schema.json (required fields, types, enums, patterns,
status-dependent fields) and the rules a schema cannot express:
- every path exists and is repo-relative; id/folder/branch/worktree/deliver_to agree;
- targets are items of assets-work/Items/catalog.json and, while the request is live
  (open/claimed/delivered), every copied fact equals the catalog;
- a set request covers parts of one armour set; shared and frozen textures are complete; owned
  files respect frozen textures and the kind; must_keep holds the lines of the kind;
- while live, constraints.render and the render must_keep lines (blended, alpha-blended and cut-out
  meshes) equal assets-work/Items/render-facts.json (tools/item_editor/render_facts.py);
- with git: base_commit exists and holds the targets' current_sha256; a claimed or delivered
  request records start_commit (the main commit the worker branched from, which contains the
  filed request and the coordinator's assignment, and the same target files as base_commit), and
  its working tree differs from start_commit only inside the request folder (request.json and
  delivery/), the owned files and docs/agents/WORKLOG.md; delivered originals equal base_commit and
  installed files equal their exports; captures and reference images are JPEGs at most 1920 px wide.
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
ITEMS_DIR = HERE.parent
ROOT = HERE.parents[2]
REQUEST_FILE = 'request.json'
BRIEF_FILE = 'brief.md'
OWNER_DECISION_FILE = 'owner-decision.json'
DELIVERY_DIR = 'delivery/'
WORKLOG = 'docs/agents/WORKLOG.md'
GAME_DATA = 'src/bin/Data'
FINDER_METADATA = '.DS_Store'

SCHEMA = 'mu-item-regen-request/1'
REPO = 'vaskodagamo/MuMain'
STATUSES = ('open', 'claimed', 'delivered', 'accepted', 'rejected', 'withdrawn')
LIVE_STATUSES = ('open', 'claimed', 'delivered')
WORKER_STATUSES = ('claimed', 'delivered')
STARTED_STATUSES = ('claimed', 'delivered', 'accepted')
DECISION_STATUSES = ('accepted', 'rejected', 'withdrawn')
PRIORITIES = ('low', 'normal', 'high')
KIND_SET = 'set'
PART_KINDS = ('upscale', 'repaint', 'remodel', 'redesign')
KINDS = PART_KINDS + (KIND_SET,)
KINDS_WITH_BMD = ('remodel', 'redesign')
MODEL_ROLES = ('item', 'class-variant', 'left-hand', 'right-hand', 'inventory')
CLASS_KEYS = ('dw', 'dk', 'elf', 'mg', 'dl', 'sum', 'rf')
CAPTURE_VARIANTS = ('current', 'original', 'candidate')
CAPTURE_VIEWS = ('inventory', 'ground', 'equipped-front', 'equipped-side', 'turntable', 'glow', 'other')
OWNER_VERDICTS = ('accept', 'reject')
ARMOUR_GROUPS = range(7, 12)
MAX_GROUP, MAX_INDEX, MAX_TIER, MAX_ITEM_LEVEL = 15, 511, 7, 15
LIMITS = {'max_triangles': 1500, 'max_texture_size': 1024}
REQUIRED_PROTECTED = ('src/source/', 'src/MuEditor/', 'assets-work/Items/catalog.json')
OWNABLE_FOLDERS = ('src/bin/Data/Item/', 'src/bin/Data/Player/')
DELIVERY_LAYOUT = ('original', 'exports', 'review', 'validation/summary.json', 'notes.md', 'source.blend')
FROZEN_KEYS = ('schema', 'id', 'created', 'author', 'priority', 'kind', 'base_commit', 'supersedes',
               'targets', 'change', 'constraints', 'evidence')
FROZEN_HANDOFF_KEYS = ('repo', 'branch', 'worktree', 'deliver_to', 'push_allowed')
ID_DATE_LENGTH = len('YYYY-MM-DD')
ID_MAX_LENGTH = 80
MAX_IMAGE_WIDTH = 1920
OTHER_CONSUMER_PREFIX = 'other:'
JPEG_SOF_MARKERS = frozenset((0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF))

# Lines every item request keeps (README "What a rebuilt item keeps"), then the lines of each kind.
COMMON_MUST_KEEP = (
    'File names and paths: no renamed, added or deleted game files',
    'Origin, orientation and scale: hands, back and shields attach through the model origin (RenderLinkObject angles are hard-coded)',
    'Mesh count and mesh/material order (the engine hides and blends meshes by index)',
    'Texture name suffixes _R, _S, _H, _N (they set render flags); no new ones',
    'At most 1500 triangles per model; power-of-two textures up to 1024 px, .jpg opaque, 32-bit .tga for alpha',
    'Armour and wings: the skeleton (bone count, order, names, parents) and every action with its key count',
    'Size in the inventory (Width x Height of the item table) and a footprint that fits it',
)
KIND_MUST_KEEP = {
    'upscale': ('Upscale: the design, the mesh and the UV layout (unless the request says otherwise), the texture name suffixes',),
    'repaint': ('Repaint: the mesh and the UV layout',),
    'remodel': ('Remodel: the silhouette and the function; origin, orientation, scale and mesh order',),
    'redesign': ('Redesign: origin, grip, size in the inventory slot and mesh order',),
}
SET_MUST_KEEP = 'Set: every part keeps its skeleton and actions, and the parts still read as one set'

ID_PATTERN = re.compile(r'^(\d{4}-\d{2}-\d{2})-(\d{1,2}-\d{1,3})-([a-z0-9]+(?:-[a-z0-9]+){0,4})$')
TIMESTAMP_PATTERN = re.compile(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$')
DATE_PATTERN = re.compile(r'^\d{4}-\d{2}-\d{2}$')
COMMIT_PATTERN = re.compile(r'^[0-9a-f]{7,40}$')
FULL_COMMIT_PATTERN = re.compile(r'^[0-9a-f]{40}$')
SHA256_PATTERN = re.compile(r'^[0-9a-f]{64}$')
KEY_PATTERN = re.compile(r'^\d{1,2}-\d{1,3}$')
GAME_PATTERN = re.compile(r'^src/bin/Data/(Item|Player)/.+\.(bmd|oz[jt])$', re.I)
CONTAINER_PATTERN = re.compile(r'^src/bin/Data/[^/]+/.+\.oz[jt]$', re.I)
BMD_PATTERN = re.compile(r'^src/bin/Data/(Item|Player)/.+\.bmd$', re.I)
CAPTURE_PATTERN = re.compile(r'^assets-work/Items/requests/([^/]+)/captures/(?!ref-)[^/]+\.(jpg|jpeg)$')
REFERENCE_PATTERN = re.compile(r'^assets-work/Items/requests/([^/]+)/captures/ref-[^/]+\.(jpg|jpeg)$')
PR_URL_PATTERN = re.compile(r'^https://github\.com/vaskodagamo/MuMain/pull/\d+$')
DRIVE_PATTERN = re.compile(r'^[A-Za-z]:')

TOP_KEYS = {'schema', 'id', 'created', 'author', 'status', 'status_history', 'priority', 'kind',
            'base_commit', 'targets', 'change', 'constraints', 'evidence', 'handoff', 'result', 'decision'}
TOP_OPTIONAL = {'supersedes'}
HISTORY_KEYS = {'status', 'at', 'by'}
TARGET_KEYS = {'key', 'group', 'index', 'name', 'family', 'tier', 'classes', 'size', 'models', 'original'}
TARGET_OPTIONAL = {'armour_set'}
MODEL_KEYS = {'role', 'bmd', 'current_sha256', 'textures'}
CHANGE_KEYS = {'summary', 'details', 'keep', 'avoid'}
CONSTRAINT_KEYS = {'shared_textures', 'frozen_textures', 'owned_files', 'protected_paths', 'limits', 'must_keep',
                   'render'}
RENDER_FACTS_SCHEMA = 'mu-item-render-facts/1'
RENDER_CONTEXTS = ('worn', 'dropped', 'inventory')
EVIDENCE_KEYS = {'captures', 'offline_previews'}
CAPTURE_KEYS = {'file', 'variant', 'view', 'resolution', 'client_commit'}
CAPTURE_OPTIONAL = {'angle', 'item_level', 'excellent', 'ancient', 'note'}
HANDOFF_KEYS = {'repo', 'branch', 'worktree', 'deliver_to', 'push_allowed', 'claimed_by', 'claimed_at', 'start_commit'}
RESULT_KEYS = {'delivered_at', 'source_commits', 'installed_files', 'validation', 'review_images', 'notes', 'pr_url'}
DECISION_KEYS = {'status', 'at', 'by', 'reason', 'ledger_entry'}
OWNER_DECISION_KEYS = {'verdict', 'notes', 'date'}


class Report:
    def __init__(self):
        self.errors, self.warnings = [], []

    def error(self, message):
        self.errors.append(message)

    def warn(self, message):
        self.warnings.append(message)


class Git:
    """Read-only git queries run at the repository root."""

    def __init__(self, root=ROOT):
        self.root = root
        self.available = True

    def run(self, args, text=True):
        if not self.available:
            return None
        try:
            return subprocess.run(['git', *args], cwd=self.root, capture_output=True, text=text)
        except OSError:
            self.available = False
            return None

    def output(self, args):
        done = self.run(args)
        return done.stdout if done is not None and done.returncode == 0 else None

    def commit(self, revision):
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
        """Path -> git status letter for every file that differs between revision and the working tree
        (untracked files count as added; game data under src/bin is ignored by .gitignore, so ignored
        untracked files there are listed as well)."""
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


def is_int(value, low=None, high=None):
    return (isinstance(value, int) and not isinstance(value, bool)
            and (low is None or value >= low) and (high is None or value <= high))


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
    if not isinstance(value, str) or not TIMESTAMP_PATTERN.match(value):
        report.error(f'{where}: {value!r} is not an RFC 3339 timestamp like 2026-09-23T10:15:00+02:00')
        return
    try:
        datetime.fromisoformat(value.replace('Z', '+00:00'))
    except ValueError:
        report.error(f'{where}: {value!r} is not a valid date and time')


def check_pattern(value, pattern, where, report, nullable=False):
    if value is None and nullable:
        return
    if not isinstance(value, str) or not pattern.match(value):
        report.error(f'{where}: {value!r} does not match {pattern.pattern}')


def repo_path(value, where, report, root, must_exist=True):
    if not is_text(value):
        report.error(f'{where}: expected a repo-relative path')
        return None
    parts = value.rstrip('/').split('/')
    if value.startswith('/') or '\\' in value or DRIVE_PATTERN.match(value) or '..' in parts:
        report.error(f'{where}: {value!r} must be relative to the repository root (no "/", "\\", "..")')
        return None
    path = root / value
    if must_exist and not path.exists():
        report.error(f'{where}: {value} does not exist')
    return path


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def section(request, key):
    value = request.get(key)
    return value if isinstance(value, dict) else {}


def is_protected(path, protected):
    return any(path == p or (p.endswith('/') and path.startswith(p)) for p in protected)


def is_ownable(path):
    return any(path.startswith(folder) for folder in OWNABLE_FOLDERS)


def consumer_sort_key(consumer):
    if consumer.startswith(OTHER_CONSUMER_PREFIX):
        return (1, 0, 0, consumer)
    group, index = consumer.split('-')
    return (0, int(group), int(index), '')


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


def part_kinds(request):
    """The kinds whose rules apply: the kind itself, or change.set_kind for a set."""
    kind = request.get('kind')
    if kind == KIND_SET:
        set_kind = section(request, 'change').get('set_kind')
        return (set_kind,) if set_kind in PART_KINDS else ()
    return (kind,) if kind in PART_KINDS else ()


def expected_must_keep(request):
    lines = list(COMMON_MUST_KEEP)
    for kind in part_kinds(request):
        lines += KIND_MUST_KEEP[kind]
    if request.get('kind') == KIND_SET:
        lines.append(SET_MUST_KEEP)
    return lines


# --- reference data -------------------------------------------------------------------------

def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def load_reference(root=ROOT, items_dir=ITEMS_DIR, git_root=None):
    """Catalog items, assignments and git; root resolves the paths inside requests (tests pass a
    scratch copy of the tree and the real clone as git_root)."""
    problems = []
    catalog = None
    catalog_path = items_dir / 'catalog.json'
    if catalog_path.exists():
        try:
            catalog = read_json(catalog_path)
        except ValueError as error:
            problems.append(f'catalog.json is not valid JSON ({error})')
    render_facts = None
    render_path = items_dir / 'render-facts.json'
    if render_path.exists():
        try:
            render_facts = read_json(render_path)
        except ValueError as error:
            problems.append(f'render-facts.json is not valid JSON ({error})')
        if isinstance(render_facts, dict) and render_facts.get('schema') != RENDER_FACTS_SCHEMA:
            problems.append(f'render-facts.json is not {RENDER_FACTS_SCHEMA}')
    assignments = {}
    assignments_path = items_dir / 'assignments.json'
    if assignments_path.exists():
        try:
            assignments = read_json(assignments_path)
        except ValueError as error:
            problems.append(f'assignments.json is not valid JSON ({error})')
    return {'root': root, 'items_dir': items_dir,
            'items': catalog.get('items', {}) if isinstance(catalog, dict) else None,
            'render': render_facts.get('items', {}) if isinstance(render_facts, dict) else None,
            'assignments': assignments if isinstance(assignments, dict) else {},
            'git': Git(git_root or root), 'problems': problems}


# --- sections -------------------------------------------------------------------------------

def check_top_level(request, report):
    check_keys(request, TOP_KEYS, 'request', report, TOP_OPTIONAL)
    if request.get('schema') != SCHEMA:
        report.error(f'schema: expected "{SCHEMA}"')
    if 'world' in request:
        report.error('world: item requests have no world')
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
        report.error(f'id: {request_id!r} is not <YYYY-MM-DD>-<item key>-<slug> (a slug of one to five lower-case '
                     f'words, <= {ID_MAX_LENGTH} chars)')
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
    first = targets[0].get('key') if isinstance(targets, list) and targets and isinstance(targets[0], dict) else None
    if isinstance(first, str) and first != match.group(2):
        report.error(f'id: item part "{match.group(2)}" must be the first target\'s key ({first})')
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


def check_model(model, where, reference, report):
    if not check_keys(model, MODEL_KEYS, where, report, {'class'}):
        return
    check_enum(model.get('role'), MODEL_ROLES, f'{where}.role', report)
    if 'class' in model:
        check_enum(model['class'], CLASS_KEYS, f'{where}.class', report)
    check_pattern(model.get('bmd'), BMD_PATTERN, f'{where}.bmd', report)
    repo_path(model.get('bmd'), f'{where}.bmd', report, reference['root'])
    check_pattern(model.get('current_sha256'), SHA256_PATTERN, f'{where}.current_sha256', report)
    textures = model.get('textures')
    if not isinstance(textures, dict):
        report.error(f'{where}.textures: expected an object')
        return
    for name, container in textures.items():
        if container is None:
            continue
        check_pattern(container, CONTAINER_PATTERN, f'{where}.textures.{name}', report)
        repo_path(container, f'{where}.textures.{name}', report, reference['root'])


def check_target_shape(target, where, reference, report):
    check_pattern(target.get('key'), KEY_PATTERN, f'{where}.key', report)
    if not is_int(target.get('group'), 0, MAX_GROUP) or not is_int(target.get('index'), 0, MAX_INDEX):
        report.error(f'{where}: group must be 0..{MAX_GROUP} and index 0..{MAX_INDEX}')
    elif target.get('key') != f'{target["group"]}-{target["index"]}':
        report.error(f'{where}.key: must be "<group>-<index>"')
    if target.get('name') is not None and not isinstance(target.get('name'), str):
        report.error(f'{where}.name: expected text or null')
    if not is_text(target.get('family')):
        report.error(f'{where}.family: expected a family name')
    if not is_int(target.get('tier'), 1, MAX_TIER):
        report.error(f'{where}.tier: expected 1..{MAX_TIER}')
    classes = target.get('classes')
    if check_keys(classes, set(CLASS_KEYS), f'{where}.classes', report):
        if not all(is_int(value, 0) for value in classes.values()):
            report.error(f'{where}.classes: stages are non-negative integers')
    size = target.get('size')
    if not (isinstance(size, list) and len(size) == 2 and all(is_int(v, 1) for v in size)):
        report.error(f'{where}.size: expected [width, height]')
    models = target.get('models')
    if not isinstance(models, list) or not models:
        report.error(f'{where}.models: expected a non-empty list')
        models = []
    for number, model in enumerate(models):
        check_model(model, f'{where}.models[{number}]', reference, report)
    if models and isinstance(models[0], dict) and models[0].get('role') != 'item':
        report.error(f'{where}.models[0]: the item\'s own model (role "item") comes first')
    original = target.get('original')
    if check_keys(original, {'revision', 'sha256'}, f'{where}.original', report):
        check_pattern(original.get('revision'), COMMIT_PATTERN, f'{where}.original.revision', report, nullable=True)
        check_pattern(original.get('sha256'), SHA256_PATTERN, f'{where}.original.sha256', report)


def check_targets(request, reference, report):
    """Shape of every target; returns the keys of the targets that are items of the catalog."""
    targets = request.get('targets')
    if not isinstance(targets, list) or not targets:
        report.error('targets: expected a non-empty list')
        return []
    keys = []
    for number, target in enumerate(targets):
        where = f'targets[{number}]'
        if not check_keys(target, TARGET_KEYS, where, report, TARGET_OPTIONAL):
            continue
        check_target_shape(target, where, reference, report)
        key = target.get('key')
        if reference['items'] is not None and key not in reference['items']:
            report.error(f'{where}.key: {key!r} is not an item of catalog.json')
            continue
        if key in keys:
            report.error(f'{where}.key: {key} is listed twice')
        keys.append(key)
    return keys


def catalog_target(entry):
    """The target fields a live request copies from a catalog entry."""
    table = entry['table']
    return {
        'key': entry['key'], 'group': entry['group'], 'index': entry['index'], 'name': entry['name'],
        'family': entry['family'], 'tier': entry['tier']['value'], 'classes': table['classes'],
        'size': [table['width'], table['height']],
        'armour_set': entry['armour_set']['index'] if entry.get('armour_set') else None,
        'models': [{key: model[key] for key in ('role', 'class', 'bmd', 'textures') if key in model}
                   for model in entry['models']],
        'original': entry['original'],
    }


def check_catalog_facts(request, keys, reference, report):
    """While live, every copied target fact equals catalog.json."""
    if request.get('status') not in LIVE_STATUSES:
        return
    if reference['items'] is None:
        report.error('catalog.json is missing; run tools/item_editor/build_item_catalog.py')
        return
    targets = {t.get('key'): t for t in request.get('targets', []) if isinstance(t, dict)}
    for key in keys:
        expected, target = catalog_target(reference['items'][key]), targets[key]
        for field, value in expected.items():
            actual = target.get(field)
            if field == 'models' and isinstance(actual, list):
                actual = [{k: m.get(k) for k in ('role', 'class', 'bmd', 'textures') if k in m}
                          for m in actual if isinstance(m, dict)]
            if field == 'armour_set':
                actual = target.get('armour_set')
            if actual != value:
                report.error(f'{key}.{field}: {actual!r} but catalog.json says {value!r}')
        models = target.get('models') if isinstance(target.get('models'), list) else []
        catalog_models = reference['items'][key]['models']
        for model, known in zip(models, catalog_models):
            original = known.get('original') or {}
            if isinstance(model, dict) and model.get('current_sha256') != original.get('sha256'):
                report.warn(f'{key}: {model.get("bmd")} current_sha256 differs from catalog.json '
                            '(the catalog is older than base_commit?)')


def render_block(facts):
    """constraints.render[<key>] of a live request: every model's per-mesh draw modes and the
    effect sentences, copied from render-facts.json."""
    models = [{'role': model['role'], 'bmd': model['bmd'],
               'meshes': [{'mesh': mesh['mesh'], 'texture': mesh['texture'],
                           **{context: mesh[context] for context in RENDER_CONTEXTS}} for mesh in model['meshes']]}
              for model in facts['models']]
    return {'models': models, 'effects': facts['request']['effects']}


def expected_render_lines(keys, reference):
    lines = []
    for key in keys:
        for line in reference['render'][key]['request']['must_keep']:
            if line not in lines:
                lines.append(line)
    return lines


def check_render(request, keys, reference, report):
    """While live, constraints.render and the render must_keep lines equal render-facts.json."""
    constraints = request.get('constraints') if isinstance(request.get('constraints'), dict) else {}
    if 'render' not in constraints:
        return  # check_keys reports the missing field
    render = constraints.get('render')
    if not isinstance(render, dict):
        report.error('constraints.render: must be an object keyed by target key')
        return
    if sorted(render) != sorted(keys):
        report.error(f'constraints.render: keys {sorted(render)} but the targets are {sorted(keys)}')
    if request.get('status') not in LIVE_STATUSES:
        return
    if reference['render'] is None:
        report.error('render-facts.json is missing; run tools/item_editor/render_facts.py')
        return
    missing_keys = [key for key in keys if key not in reference['render']]
    if missing_keys:
        report.error(f'render-facts.json has no entry for {missing_keys}; rebuild it')
        return
    for key in keys:
        if render.get(key) != render_block(reference['render'][key]):
            report.error(f'constraints.render.{key}: differs from render-facts.json (copy its models and '
                         'request.effects)')
    must_keep = constraints.get('must_keep') if isinstance(constraints.get('must_keep'), list) else []
    missing = [line for line in expected_render_lines(keys, reference) if line not in must_keep]
    if missing:
        report.error(f'constraints.must_keep: missing the render lines of render-facts.json: {missing}')


def check_change(request, request_id, reference, report):
    change = request.get('change')
    if not check_keys(change, CHANGE_KEYS, 'change', report, {'set_kind', 'reference_images'}):
        return
    if not is_text(change.get('summary')):
        report.error('change.summary: expected a non-empty string')
    for key in ('details', 'keep', 'avoid'):
        string_list(change.get(key), f'change.{key}', report)
    if request.get('kind') == KIND_SET:
        check_enum(change.get('set_kind'), PART_KINDS, 'change.set_kind', report)
    elif 'set_kind' in change:
        report.error('change.set_kind: only a set request names the kind of its parts')
    references = change.get('reference_images', [])
    if not isinstance(references, list):
        report.error('change.reference_images: expected a list')
        return
    for index, path in enumerate(references):
        where = f'change.reference_images[{index}]'
        match = REFERENCE_PATTERN.match(path) if isinstance(path, str) else None
        if not match or match.group(1) != request_id:
            report.error(f'{where}: expected assets-work/Items/requests/{request_id}/captures/ref-<name>.jpg')
            continue
        check_image(path, where, None, reference, report)


def check_set(request, keys, reference, report):
    """A set request: two or more parts (groups 7-11) of the same armour set."""
    if request.get('kind') != KIND_SET or reference['items'] is None:
        return
    if len(keys) < 2:
        report.error('targets: a set request has at least two parts of one armour set')
    sets = set()
    for key in keys:
        entry = reference['items'][key]
        if entry['group'] not in ARMOUR_GROUPS or not entry.get('armour_set'):
            report.error(f'targets: {key} is not an armour part (groups 7-11)')
            continue
        sets.add(entry['armour_set']['index'])
    if len(sets) > 1:
        report.error(f'targets: the parts belong to different armour sets {sorted(sets)}')


def target_containers(keys, items):
    """Container -> the target keys that use it."""
    containers = {}
    for key in keys:
        for model in items[key]['models']:
            for container in model['textures'].values():
                if container:
                    containers.setdefault(container, set()).add(key)
    return containers


def target_bmds(keys, items):
    return {model['bmd'] for key in keys for model in items[key]['models']}


def check_shared_and_frozen(constraints, keys, containers, items, root, report):
    consumers = {}
    for container in containers:
        users = set(containers[container])
        for key in keys:
            users |= set(items[key]['shared_with'].get(container, []))
        consumers[container] = users
    expected = {c: sorted(u, key=consumer_sort_key) for c, u in consumers.items() if len(u) > 1}
    shared = constraints.get('shared_textures')
    if not isinstance(shared, dict):
        report.error('constraints.shared_textures: expected an object')
    elif shared != expected:
        report.error('constraints.shared_textures: must list every shared target container with all its '
                     f'consumers: expected {json.dumps(expected, sort_keys=True)}')
    frozen = string_list(constraints.get('frozen_textures'), 'constraints.frozen_textures', report)
    for index, path in enumerate(frozen):
        check_pattern(path, CONTAINER_PATTERN, f'constraints.frozen_textures[{index}]', report)
        repo_path(path, f'constraints.frozen_textures[{index}]', report, root)
    must_freeze = sorted(c for c, users in consumers.items() if users - set(keys) or not is_ownable(c))
    missing = sorted(set(must_freeze) - set(frozen))
    if missing:
        report.error('constraints.frozen_textures: shared with items or models outside the targets, or outside '
                     f'Data/Item and Data/Player, must be frozen: {missing}')
    stray = sorted(set(frozen) - set(containers))
    if stray:
        report.warn(f'constraints.frozen_textures: not a texture of any target: {stray}')
    return set(frozen)


def check_owned(request, constraints, keys, containers, frozen, items, root, report):
    owned = string_list(constraints.get('owned_files'), 'constraints.owned_files', report)
    for index, path in enumerate(owned):
        check_pattern(path, GAME_PATTERN, f'constraints.owned_files[{index}]', report)
        repo_path(path, f'constraints.owned_files[{index}]', report, root)
    if len(set(owned)) != len(owned):
        report.error('constraints.owned_files: duplicate entries')
    bmds = target_bmds(keys, items)
    outside = sorted(set(owned) - bmds - set(containers))
    if outside:
        report.error(f'constraints.owned_files: not a target BMD or target texture: {outside}')
    overlap = sorted(set(owned) & frozen)
    if overlap:
        report.error(f'constraints.owned_files: frozen textures cannot be owned: {overlap}')
    owned_bmds = sorted(set(owned) & bmds)
    kinds = part_kinds(request)
    if owned_bmds and not any(kind in KINDS_WITH_BMD for kind in kinds):
        report.error(f'constraints.owned_files: kind {request.get("kind")!r} changes textures only; it may not '
                     f'replace BMDs: {owned_bmds}')
    if not owned:
        report.warn('constraints.owned_files: empty; the worker cannot install anything')
    return set(owned)


def check_protected(constraints, owned, git, root, report):
    protected = string_list(constraints.get('protected_paths'), 'constraints.protected_paths', report)
    for index, path in enumerate(protected):
        found = repo_path(path, f'constraints.protected_paths[{index}]', report, root, must_exist=False)
        if found and not found.exists() and not git.has_path('HEAD', path.rstrip('/')):
            report.error(f'constraints.protected_paths[{index}]: {path} does not exist')
    for required in REQUIRED_PROTECTED:
        if required not in protected:
            report.error(f'constraints.protected_paths: must contain {required}')
    for path in sorted(owned):
        if is_protected(path, protected):
            report.error(f'constraints.owned_files: {path} is inside a protected path')
    return protected


def check_constraints(request, keys, reference, report):
    constraints = request.get('constraints')
    if not check_keys(constraints, CONSTRAINT_KEYS, 'constraints', report):
        return set(), set(), []
    items, root = reference['items'], reference['root']
    if constraints.get('limits') != LIMITS:
        report.error(f'constraints.limits: must be {json.dumps(LIMITS, sort_keys=True)}')
    must_keep = string_list(constraints.get('must_keep'), 'constraints.must_keep', report, minimum=1)
    missing = [line for line in expected_must_keep(request) if line not in must_keep]
    if missing:
        report.error(f'constraints.must_keep: missing the lines of the README for this kind: {missing}')
    if items is None:
        return set(), set(), string_list(constraints.get('protected_paths'), 'constraints.protected_paths', report)
    containers = target_containers(keys, items)
    frozen = check_shared_and_frozen(constraints, keys, containers, items, root, report)
    owned = check_owned(request, constraints, keys, containers, frozen, items, root, report)
    protected = check_protected(constraints, owned, reference['git'], root, report)
    return owned, frozen, protected


def check_image(path, where, resolution, reference, report):
    full = repo_path(path, where, report, reference['root'])
    if full is None or not full.is_file():
        return
    size = jpeg_size(full)
    if size is None:
        report.error(f'{where}: {path} is not a JPEG')
    elif size[0] > MAX_IMAGE_WIDTH:
        report.error(f'{where}: {path} is {size[0]} px wide (at most {MAX_IMAGE_WIDTH})')
    elif resolution is not None and list(size) != resolution:
        report.error(f'{where}.resolution: {resolution} but {path} is {list(size)}')


def check_capture(capture, where, request_id, reference, report):
    if not check_keys(capture, CAPTURE_KEYS, where, report, CAPTURE_OPTIONAL):
        return
    file = capture.get('file')
    match = CAPTURE_PATTERN.match(file) if isinstance(file, str) else None
    check_enum(capture.get('variant'), CAPTURE_VARIANTS, f'{where}.variant', report)
    check_enum(capture.get('view'), CAPTURE_VIEWS, f'{where}.view', report)
    if 'item_level' in capture and not is_int(capture['item_level'], 0, MAX_ITEM_LEVEL):
        report.error(f'{where}.item_level: expected 0..{MAX_ITEM_LEVEL}')
    for flag in ('excellent', 'ancient'):
        if flag in capture and not isinstance(capture[flag], bool):
            report.error(f'{where}.{flag}: expected true or false')
    if 'angle' in capture and (not isinstance(capture['angle'], (int, float)) or isinstance(capture['angle'], bool)):
        report.error(f'{where}.angle: expected a number')
    check_pattern(capture.get('client_commit'), COMMIT_PATTERN, f'{where}.client_commit', report, nullable=True)
    resolution = capture.get('resolution')
    if not (isinstance(resolution, list) and len(resolution) == 2 and all(is_int(v, 1) for v in resolution)):
        report.error(f'{where}.resolution: expected [width, height]')
        resolution = None
    elif resolution[0] > MAX_IMAGE_WIDTH:
        report.error(f'{where}.resolution: at most {MAX_IMAGE_WIDTH} px wide')
    if not match or match.group(1) != request_id:
        report.error(f'{where}.file: expected assets-work/Items/requests/{request_id}/captures/<name>.jpg (not ref-*)')
        return
    check_image(file, f'{where}.file', resolution, reference, report)


def check_evidence(request, request_id, reference, report):
    evidence = request.get('evidence')
    if not check_keys(evidence, EVIDENCE_KEYS, 'evidence', report):
        return
    captures = evidence.get('captures')
    if not isinstance(captures, list):
        report.error('evidence.captures: expected a list')
        captures = []
    for index, capture in enumerate(captures):
        check_capture(capture, f'evidence.captures[{index}]', request_id, reference, report)
    if not captures:
        report.warn('evidence.captures: no in-client capture attached')
    previews = evidence.get('offline_previews')
    if not isinstance(previews, list):
        report.error('evidence.offline_previews: expected a list')
        return
    for index, path in enumerate(previews):
        repo_path(path, f'evidence.offline_previews[{index}]', report, reference['root'])


def sibling_collisions(request_id, folder):
    """Other request folders whose <item key>-<slug> (and therefore branch name) equals this one."""
    suffix = request_id[ID_DATE_LENGTH:]
    return sorted(p.parent.name for p in folder.parent.glob(f'*/{REQUEST_FILE}')
                  if p.parent != folder and p.parent.name[ID_DATE_LENGTH:] == suffix)


def check_handoff(request, request_id, folder, report):
    handoff = request.get('handoff')
    if not check_keys(handoff, HANDOFF_KEYS, 'handoff', report):
        return
    name = request_id[ID_DATE_LENGTH + 1:]
    expected = {'repo': REPO, 'branch': f'codex/item-req-{name}', 'worktree': f'../MuMain-item-req-{name}',
                'deliver_to': f'assets-work/Items/requests/{request_id}/{DELIVERY_DIR}'}
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


def check_delivery_layout(request, keys, reference, report):
    deliver_to = section(request, 'handoff').get('deliver_to')
    if not isinstance(deliver_to, str):
        return
    for key in keys:
        for item in DELIVERY_LAYOUT:
            path = f'{deliver_to}{key}/{item}'
            if not (reference['root'] / path).exists():
                report.error(f'delivery: {path} is missing (per-item layout: {", ".join(DELIVERY_LAYOUT)})')


def check_installed(request, result, owned, reference, report):
    installed = result.get('installed_files')
    if not isinstance(installed, dict):
        report.error('result.installed_files: expected an object')
        return {}
    deliver_to = section(request, 'handoff').get('deliver_to') or ''
    for game, export in sorted(installed.items()):
        where = f'result.installed_files[{game}]'
        if game not in owned:
            report.error(f'{where}: not in constraints.owned_files')
        game_path = repo_path(game, where, report, reference['root'])
        export_path = repo_path(export, where, report, reference['root'])
        if isinstance(export, str) and not export.startswith(deliver_to):
            report.error(f'{where}: export {export} is outside {deliver_to}')
        ready = game_path and export_path and game_path.is_file() and export_path.is_file()
        if request.get('status') == 'delivered' and ready and sha256_file(game_path) != sha256_file(export_path):
            report.error(f'{where}: installed file differs from its delivered export')
    return installed


def check_result(request, owned, keys, reference, report):
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
    installed = check_installed(request, result, owned, reference, report)
    for key, minimum in (('validation', 1), ('review_images', 0), ('notes', 1)):
        for index, path in enumerate(string_list(result.get(key), f'result.{key}', report, minimum)):
            repo_path(path, f'result.{key}[{index}]', report, reference['root'])
    pr_url = result.get('pr_url')
    check_pattern(pr_url, PR_URL_PATTERN, 'result.pr_url', report, nullable=True)
    if pr_url and not section(request, 'handoff').get('push_allowed'):
        report.warn('result.pr_url is set while handoff.push_allowed is false; confirm the owner authorized it')
    check_delivery_layout(request, keys, reference, report)
    return installed


def check_decision(request, request_id, report):
    decision = request.get('decision')
    if decision is None or not check_keys(decision, DECISION_KEYS, 'decision', report):
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


def check_owner_decision(folder, report):
    """owner-decision.json, written by the editor's Accept / Reject with notes (optional)."""
    path = folder / OWNER_DECISION_FILE
    if not path.exists():
        return
    try:
        verdict = read_json(path)
    except ValueError as error:
        report.error(f'{OWNER_DECISION_FILE}: not valid JSON ({error})')
        return
    if not check_keys(verdict, OWNER_DECISION_KEYS, OWNER_DECISION_FILE, report):
        return
    check_enum(verdict.get('verdict'), OWNER_VERDICTS, f'{OWNER_DECISION_FILE}.verdict', report)
    if not isinstance(verdict.get('notes'), str):
        report.error(f'{OWNER_DECISION_FILE}.notes: expected text')
    if not isinstance(verdict.get('date'), str) or not DATE_PATTERN.match(verdict['date']):
        report.error(f'{OWNER_DECISION_FILE}.date: expected YYYY-MM-DD')


def check_status_fields(request, report):
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
        'accepted': (claimed and isinstance(result, dict) and isinstance(decision, dict) and decision.get('status') == 'accepted',
                     'an accepted request has the claim and result merged from the worker branch and an accepted decision'),
        'rejected': (isinstance(decision, dict) and decision.get('status') == 'rejected', 'a rejected request has a rejected decision'),
        'withdrawn': (isinstance(decision, dict) and decision.get('status') == 'withdrawn', 'a withdrawn request has a withdrawn decision'),
    }
    if status in rules and not rules[status][0]:
        report.error(f'status {status}: {rules[status][1]}')


def check_assignment(request, request_id, assignments, report):
    """A claimed or delivered request is assigned to exactly the worker that claimed it."""
    if request.get('status') not in WORKER_STATUSES:
        return
    workers = sorted(worker for worker, entry in assignments.items()
                     if isinstance(entry, dict) and entry.get('request') == request_id)
    claimed_by = section(request, 'handoff').get('claimed_by')
    if not workers:
        report.error(f'handoff.claimed_by: no entry with "request": "{request_id}" in assets-work/Items/assignments.json; '
                     'take only a request the coordinator assigned to you')
    elif claimed_by not in workers:
        report.error(f'handoff.claimed_by: {claimed_by!r} but the coordinator assigned this request to {workers}')


# --- git checks -----------------------------------------------------------------------------

def target_models(request):
    for target in request.get('targets', []) if isinstance(request.get('targets'), list) else []:
        if isinstance(target, dict) and isinstance(target.get('models'), list):
            for model in target['models']:
                if isinstance(model, dict) and isinstance(model.get('bmd'), str):
                    yield target.get('key'), model


def check_base(request, git, report):
    """base_commit exists and holds the bytes the owner saw (models[].current_sha256)."""
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
    for key, model in target_models(request):
        digest = git.blob_sha256(full, model['bmd'])
        if digest is None:
            report.error(f'{key}: {model["bmd"]} does not exist at base_commit')
        elif model.get('current_sha256') != digest:
            report.error(f'{key}: current_sha256 of {model["bmd"]} differs from base_commit ({digest})')
    return full


def request_files(keys, owned, items):
    files = set(owned)
    for key in keys:
        for model in items[key]['models']:
            files.add(model['bmd'])
            files.update(c for c in model['textures'].values() if c)
    return sorted(files)


def check_start(request, request_id, keys, owned, reference, base, report):
    """start_commit: the main commit the worker branched from; same request bytes as base_commit."""
    git = reference['git']
    if request.get('status') not in STARTED_STATUSES or base is None or reference['items'] is None:
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
    filed = f'assets-work/Items/requests/{request_id}/{REQUEST_FILE}'
    if git.blob_id(start, filed) is None:
        report.error(f'handoff.start_commit: {start} does not contain {filed}; branch from the main commit that '
                     'contains the filed request')
    stale = [p for p in request_files(keys, owned, reference['items']) if git.blob_id(start, p) != git.blob_id(base, p)]
    if stale:
        report.error(f'handoff.start_commit: these files changed between base_commit and start_commit, so the request '
                     f'is stale; stop and ask the owner to file it again: {stale}')
    return start


def check_frozen(request, request_id, start, git, report):
    if start is None:
        return
    filed = git.json_at(start, f'assets-work/Items/requests/{request_id}/{REQUEST_FILE}')
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
    """On the worker branch, only request.json, delivery/, owned files and the work log may differ."""
    if request.get('status') not in WORKER_STATUSES or start is None:
        return
    changes = git.changes(start)
    if changes is None:
        report.error('scope: git could not list the changes since start_commit')
        return
    own_folder = f'assets-work/Items/requests/{request_id}/'
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


def check_originals(request, keys, owned, reference, base, report):
    """delivery/<key>/original/ holds the item's own BMD and its owned files exactly as at base_commit."""
    if request.get('result') is None or base is None or reference['items'] is None:
        return
    deliver_to = section(request, 'handoff').get('deliver_to')
    if not isinstance(deliver_to, str):
        return
    git = reference['git']
    for key in keys:
        entry = reference['items'][key]
        files = {entry['models'][0]['bmd']}
        for model in entry['models']:
            files |= {f for f in [model['bmd'], *model['textures'].values()] if f in owned}
        for game in sorted(files):
            path = reference['root'] / f'{deliver_to}{key}/original/{Path(game).name}'
            if not path.is_file():
                report.error(f'delivery: {deliver_to}{key}/original/{Path(game).name} is missing (the untouched {game})')
            elif sha256_file(path) != git.blob_sha256(base, game):
                report.error(f'delivery: {deliver_to}{key}/original/{Path(game).name} differs from {game} at base_commit')


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
    for problem in reference['problems']:
        report.error(problem)
    check_folder(folder, report)
    check_top_level(request, report)
    request_id = check_id(request, folder, report) or folder.name
    check_history(request, report)
    keys = check_targets(request, reference, report)
    check_change(request, request_id, reference, report)
    check_set(request, keys, reference, report)
    owned, frozen, protected = check_constraints(request, keys, reference, report)
    check_catalog_facts(request, keys, reference, report)
    check_render(request, keys, reference, report)
    check_evidence(request, request_id, reference, report)
    check_handoff(request, request_id, folder, report)
    installed = check_result(request, owned, keys, reference, report)
    check_decision(request, request_id, report)
    check_owner_decision(folder, report)
    check_status_fields(request, report)
    check_assignment(request, request_id, reference['assignments'], report)
    git = reference['git']
    base = check_base(request, git, report)
    start = check_start(request, request_id, keys, owned, reference, base, report)
    check_frozen(request, request_id, start, git, report)
    check_scope(request, request_id, owned, frozen, protected, installed, start, git, report)
    check_originals(request, keys, owned, reference, base, report)
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
    parser = argparse.ArgumentParser(description='Validate item regeneration requests.')
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
