"""Build assets-work/World1/catalog.json, the per-model file the in-client world editor reads.

Coordinator-owned, read-only with respect to game data: it only reads the coordination files,
the per-model evidence folders, the request folders, the owner's client-review.json, the BMD
files under src/bin/Data and the git history of HEAD, and writes one JSON file. Output is deterministic for a given commit
(sorted keys, stable ordering, no timestamps) and every path in it is relative to the
repository root.

Usage (from anywhere):
    python3 assets-work/World1/coordination/build_editor_catalog.py           # write catalog.json
    python3 assets-work/World1/coordination/build_editor_catalog.py --check   # exit 1 if stale

Catalog shape (schema "mu-world-catalog/1"):
    models          object keyed by the decimal World1 object type ("0" .. "153"). The key equals
                    OBJECT::Type of a placed Lorencia object (the i16 type in EncTerrain1.obj).
    untyped_models  object keyed by model name, for BMDs in Object1 that have no World1 type
                    (animated fauna spawned by code, never placed through EncTerrain1.obj).
    requests        every request folder found under assets-work/World1/requests/.
client-review.json (owner-owned, written by the world editor's "Looks good in client" and
"Needs work" buttons) maps a model name to {"verdict", "note", "date"}; each entry becomes
that model's client_review, and client_verified is true for the verdict "looks-good".
See assets-work/World1/requests/README.md for the field list and the request contract.
"""

import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
WORLD_DIR = HERE.parent
ROOT = HERE.parents[2]
OUTPUT = WORLD_DIR / 'catalog.json'
REQUESTS_DIR = WORLD_DIR / 'requests'
CLIENT_REVIEW = WORLD_DIR / 'client-review.json'

SCHEMA = 'mu-world-catalog/1'
REQUEST_SCHEMA = 'mu-regen-request/1'
WORLD_NUMBER = 1
WORLD_NAME = 'Lorencia'
SHORT_SHA = 8
READ_CHUNK = 1 << 20

STATUS_ACCEPTED = 'accepted'
STATUS_BLOCKED = 'blocked'
STATUS_IN_PROGRESS = 'in-progress'
STATUS_UNCHANGED = 'unchanged'
ROLE_PRIMARY = 'primary'
ROLE_COMPATIBILITY = 'compatibility'
PILOT_BATCH_NAME = 'pilot'
REQUEST_STATUSES = ('open', 'claimed', 'delivered', 'accepted', 'rejected', 'withdrawn')
OPEN_REQUEST_STATUSES = ('open', 'claimed', 'delivered')
REQUEST_LEDGER_PREFIX = 'requests/'
VERDICT_LOOKS_GOOD = 'looks-good'
VERDICT_NEEDS_WORK = 'needs-work'
VERDICTS = (VERDICT_LOOKS_GOOD, VERDICT_NEEDS_WORK)
CLIENT_REVIEW_KEYS = {'verdict', 'note', 'date'}
DATE_PATTERN = re.compile(r'^\d{4}-\d{2}-\d{2}$')

DEPENDENCY_MAP = HERE / 'dependency-map.json'
IDENTITIES = HERE / 'identities.json'
LEDGER = HERE / 'integration-ledger.json'
PRODUCTION_BATCHES = HERE / 'production-batches.json'
PROTECTED_BASELINE = HERE / 'protected-baseline.json'
COMBINED_VALIDATION = HERE / 'combined-validation.json'
COMBINED_MODEL_INFO = HERE / 'combined-model-info.json'
PUBLICATION = HERE / 'publication.json'
ENGINE_CONTROLS = HERE / 'engine-controls.md'
BOARD_SCRIPT = HERE / 'update_board.py'
PILOT_HANDOFF = WORLD_DIR / 'notes.md'
FINAL_INSPECTION = HERE / 'final-inspection'
BASELINE_INSPECTION = HERE / 'inspection'
TEXTURE_BASELINE = HERE / 'texture-baseline'
PROVENANCE_FILE = 'provenance.json'
BASELINE_PREVIEW = 'baseline-offline.png'
FINAL_PREVIEW = 'final-offline.png'
PUBLICATION_KEYS = ('branch', 'merge_commit', 'merged_at', 'merged_into_main', 'pr_head_commit',
                    'pr_number', 'pr_url')
PILOT_BASE_PATTERN = re.compile(r'created from `main` at `([0-9a-f]{7,40})`')
STRUCTURE_PATTERN = re.compile(r'meshes: (\d+)\s+bones: (\d+)\s+actions: (\d+)\s+triangles: (\d+)')
MESH_TEXTURE_PATTERN = re.compile(r'^\s*mesh \d+: .*texture=(.*)$', re.M)
BLEND_MESH_PATTERN = re.compile(r'BlendMesh=(\d+)')
# Items in the "Existing control" column are separated by ';' or ', ' (offsets such as
# "±150,-150,140" have no space after their commas and stay whole).
CONTROL_SEPARATOR = re.compile(r';|,\s+')
TRAILING_DIGITS = re.compile(r'^(.*?)(\d+)$')
# update_board.py writes batch worktrees as WORKTREE_ROOT + '<sibling folder>'.
WORKTREE_ROOT_NAME = 'WORKTREE_ROOT'


class CatalogError(Exception):
    """Inconsistent coordination data; the catalog must not be written."""


def rel(path):
    return Path(path).resolve().relative_to(ROOT).as_posix()


def git(args, stdin=None):
    try:
        done = subprocess.run(['git', *args], cwd=ROOT, input=stdin, capture_output=True, text=True)
    except OSError as error:
        raise CatalogError(f'git is required to check commit reachability: {error}') from error
    if done.returncode != 0:
        raise CatalogError(f'git {" ".join(args)} failed: {done.stderr.strip()}')
    return done.stdout


def reachable_commits(commits):
    """Commit (as written in the coordination files) -> whether HEAD's history contains it.

    Batch branches were deleted after integration, so some recorded commits exist only as loose
    objects in the clone that made them (or not at all); `git show` fails for them in a fresh
    clone. Ancestry of HEAD is a property of the commit graph, so the result does not depend on
    which clone runs this.
    """
    if git(['rev-parse', '--is-shallow-repository']).strip() == 'true':
        raise CatalogError('shallow clone: run `git fetch --unshallow` so commit reachability is exact')
    names = sorted(set(commits))
    lookup = git(['cat-file', '--batch-check'], ''.join(f'{name}^{{commit}}\n' for name in names))
    full = {}
    for name, line in zip(names, lookup.splitlines()):
        parts = line.split()
        if len(parts) >= 2 and parts[1] == 'commit':
            full[name] = parts[0]
    history = set(git(['rev-list', 'HEAD']).split())
    return {name: full.get(name) in history for name in names}


def worktree_name(value):
    """Sibling worktree folder name; None for absolute paths (never written to the catalog)."""
    if not value or value.startswith('/'):
        return None
    return Path(value).name


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(READ_CHUNK), b''):
            digest.update(chunk)
    return digest.hexdigest()


def existing(path):
    """Repo-relative path when the file or folder exists, else None."""
    return rel(path) if path.exists() else None


# --- data that exists only inside update_board.py and the pilot handoff ---------------------

def literal_text(node):
    """A string constant, or the folder in WORKTREE_ROOT + '<folder>'; else None."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if (isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add) and isinstance(node.left, ast.Name)
            and node.left.id == WORKTREE_ROOT_NAME and isinstance(node.right, ast.Constant)):
        return node.right.value
    return None


def literal_constants(nodes):
    """Name -> string for plain and tuple assignments of string literals."""
    values = {}
    for node in nodes:
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            names = target.elts if isinstance(target, ast.Tuple) else [target]
            sources = node.value.elts if isinstance(node.value, ast.Tuple) else [node.value]
            if len(names) != len(sources):
                continue
            for name, source in zip(names, sources):
                value = literal_text(source)
                if isinstance(name, ast.Name) and value is not None:
                    values[name.id] = value
    return values


def read_board_literals():
    """PREVIOUS/TAVERN dicts plus the owner/branch/worktree literals update_board.py assigns for them.

    The four pilot props and the tavern identity/owner/branch are hard-coded in update_board.py;
    this reads them with ast instead of copying them, so the catalog and the board cannot drift.
    """
    tree = ast.parse(BOARD_SCRIPT.read_text(encoding='utf-8'))
    tables, branches = {}, {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name) \
                and node.targets[0].id in ('PREVIOUS', 'TAVERN'):
            tables[node.targets[0].id] = ast.literal_eval(node.value)
    for node in ast.walk(tree):
        if not isinstance(node, ast.If) or not isinstance(node.test, ast.Compare):
            continue
        table = node.test.comparators[0]
        if isinstance(table, ast.Name) and table.id in ('PREVIOUS', 'TAVERN'):
            branches[table.id] = literal_constants(node.body)
    for key in ('PREVIOUS', 'TAVERN'):
        if key not in tables or not {'branch', 'owner'} <= set(branches.get(key, {})):
            raise CatalogError(f'update_board.py no longer defines {key} and its owner/branch literals')
    return tables['PREVIOUS'], tables['TAVERN'], branches['PREVIOUS'], branches['TAVERN']


def read_pilot_base():
    match = PILOT_BASE_PATTERN.search(PILOT_HANDOFF.read_text(encoding='utf-8'))
    if not match:
        raise CatalogError(f'{rel(PILOT_HANDOFF)} no longer names the pilot base commit')
    return match.group(1)[:SHORT_SHA]


def board_identity(name, model, context):
    """Identity text exactly as update_board.py prints it on the asset board."""
    if model['scope_exclusion']:
        return model['scope_exclusion']
    if name in context['tavern']:
        return context['tavern'][name]
    if name in context['previous']:
        return context['previous'][name][0]
    if name not in context['identities']:
        raise CatalogError(f'{name}: in scope but missing from identities.json')
    return context['identities'][name]


# --- engine-controls.md ---------------------------------------------------------------------

def expand_model_cell(cell, known):
    """'Tree07, Furniture06/07' -> ['Tree07', 'Furniture06', 'Furniture07']."""
    names = []
    for part in (p.strip() for p in cell.split(',')):
        first, *suffixes = [s.strip() for s in part.split('/')]
        names.append(first)
        match = TRAILING_DIGITS.match(first)
        for suffix in suffixes:
            if not match:
                raise CatalogError(f'engine-controls.md: cannot expand "{part}"')
            names.append(match.group(1) + suffix)
    unknown = [n for n in names if n not in known]
    if unknown:
        raise CatalogError(f'engine-controls.md names unknown models: {unknown}')
    return names


def table_cells(line):
    return [cell.strip() for cell in line.strip().strip('|').split('|')]


def control_rows(lines, known):
    """Model -> control rows of the markdown table (header and separator lines skipped)."""
    controls = {}
    for line in [line for line in lines if line.startswith('|')][2:]:
        cell, control, requirement = table_cells(line)
        entry = {'source_row': cell,
                 'controls': [c.strip() for c in CONTROL_SEPARATOR.split(control) if c.strip()],
                 'requirement': requirement}
        blend = BLEND_MESH_PATTERN.search(control)
        if blend:
            entry['blend_mesh'] = int(blend.group(1))
        for name in expand_model_cell(cell, known):
            controls.setdefault(name, []).append(entry)
    return controls


def prose_paragraphs(lines):
    """Paragraphs outside headings and tables, each joined into one line."""
    notes, paragraph = [], []
    for line in lines + ['']:
        if line.strip() and not line.startswith(('#', '|')):
            paragraph.append(line.strip())
            continue
        if paragraph:
            notes.append(' '.join(paragraph))
        paragraph = []
    return notes


def read_engine_controls(known):
    """Model -> list of control rows, plus the free-text notes around the table."""
    lines = ENGINE_CONTROLS.read_text(encoding='utf-8').splitlines()
    return control_rows(lines, known), prose_paragraphs(lines)


# --- per-model facts ------------------------------------------------------------------------

def bmd_structure(info):
    match = STRUCTURE_PATTERN.search(info or '')
    if not match:
        return None
    meshes, bones, actions, triangles = (int(v) for v in match.groups())
    return {'meshes': meshes, 'bones': bones, 'actions': actions, 'triangles': triangles,
            'mesh_textures': [t.strip() for t in MESH_TEXTURE_PATTERN.findall(info)]}


def model_controls(name, controls, structure):
    """Engine-control rows for a model, with the texture at its BlendMesh index resolved."""
    result = []
    for row in controls.get(name, []):
        entry = dict(row)
        textures = structure['mesh_textures'] if structure else []
        if 'blend_mesh' in entry and entry['blend_mesh'] < len(textures):
            entry['blend_mesh_texture'] = textures[entry['blend_mesh']]
        result.append(entry)
    return result


def previews(name):
    found = {}
    baseline = existing(BASELINE_INSPECTION / name / BASELINE_PREVIEW)
    final = existing(FINAL_INSPECTION / name / FINAL_PREVIEW)
    if baseline:
        found['baseline'] = baseline
    if final:
        found['final'] = final
    return found


def provenance_files(name):
    """Game path -> sha256 recorded by inspect_final.py for the model's final offline review."""
    path = FINAL_INSPECTION / name / PROVENANCE_FILE
    return read_json(path).get('game_files', {}) if path.exists() else {}


def accepted_sha256(name, bmd):
    return provenance_files(name).get(bmd)


def first_placement(placements):
    if not placements:
        return None
    first = placements[0]
    return {key: first[key] for key in ('position', 'rotation', 'scale', 'tile')}


def texture_links(model, consumers, name):
    textures, shared = {}, {}
    for texture, containers in model['textures'].items():
        if len(containers) != 1:
            raise CatalogError(f'{name}: texture {texture} resolves to {containers}')
        container = containers[0]
        textures[texture] = container
        shared[texture] = sorted(set(consumers.get(container, [])) - {name})
    return textures, shared


# --- batches --------------------------------------------------------------------------------

def match_production_batches(ledger, batches):
    """Ledger batch name -> (agent label, production-batches entry).

    An entry may name its ledger batch explicitly ("ledger": "<name>", used for requests).
    Legacy entries match the ledger batch whose model list starts with the entry's models (the
    longest such entry wins), and each entry matches at most one ledger batch. Unmatched
    entries are still in progress.
    """
    matched, used = {}, set()
    for agent, claim in batches.items():
        if claim.get('ledger'):
            matched[claim['ledger']] = (agent, claim)
            used.add(agent)
    for batch in ledger:
        if batch['name'] in matched:
            continue
        candidates = [agent for agent, claim in batches.items() if agent not in used
                      and not claim.get('ledger')
                      and claim['models'] == batch['models'][:len(claim['models'])]]
        if not candidates:
            continue
        longest = max(len(batches[agent]['models']) for agent in candidates)
        best = [agent for agent in candidates if len(batches[agent]['models']) == longest]
        if len(best) > 1:
            raise CatalogError(f'Ledger batch {batch["name"]} matches several reservations: {best}')
        matched[batch['name']] = (best[0], batches[best[0]])
        used.add(best[0])
    return matched


def original_record(revision, archive, digest, context):
    """Pre-rebuild BMD: revision (8 characters), repo archive, its sha256, and whether HEAD has the revision."""
    return {'revision': revision, 'archive': archive, 'sha256': digest,
            'reachable': context['reachable'][revision]}


def batch_original(batch, name, context):
    archive = WORLD_DIR / batch['name'] / name / 'original' / f'{name}.bmd'
    revision = batch.get('original_revisions', {}).get(name, context['baseline_short'])[:SHORT_SHA]
    record = context['validated'].get(f'{batch["name"]}/{name}')
    digest = sha256_file(archive) if archive.exists() else None
    if record and (record['revision'][:SHORT_SHA] != revision or record['sha256'] != digest):
        raise CatalogError(f'{batch["name"]}/{name}: original archive differs from combined-validation.json')
    return original_record(revision, existing(archive), digest, context)


def unreachable(commits, context):
    return sorted({commit for commit in commits if not context['reachable'][commit]})


def files_for_model(files, model_dir):
    prefix = model_dir + '/'
    return {game: record for game, record in sorted(files.items())
            if (record if isinstance(record, str) else record.get('export', '')).startswith(prefix)}


def batch_role(name, claim):
    """Primary: the batch rebuilt the model. Compatibility: it reviewed a shared-texture consumer."""
    if claim and name not in claim['models']:
        return ROLE_COMPATIBILITY
    return ROLE_PRIMARY


def board_claim(literals):
    """Agent, branch and worktree that update_board.py hard-codes for batches without a reservation."""
    return {'agent': literals['owner'], 'branch': literals['branch'],
            'worktree': worktree_name(literals.get('worktree'))}


def unreserved_claim(batch, context):
    """The tavern batch predates production-batches.json; update_board.py records its claim."""
    if set(batch['models']) <= set(context['tavern']):
        return board_claim(context['tavern_literals'])
    return {'agent': None, 'branch': None, 'worktree': None}


def ledger_batch_entry(batch, name, reservation, context):
    if reservation:
        agent, claim = reservation
        found = {'agent': agent, 'branch': claim.get('branch'), 'worktree': worktree_name(claim.get('worktree'))}
    else:
        claim, found = {}, unreserved_claim(batch, context)
    model_dir = f'assets-work/World1/{batch["name"]}/{name}'
    original = batch_original(batch, name, context)
    entry = {
        'name': batch['name'],
        'role': batch_role(name, claim),
        'owner': batch['owner'],
        'agent': found['agent'],
        'branch': found['branch'],
        'worktree': found['worktree'],
        'source_commits': list(batch['source_commits']),
        'integration_commits': list(batch['integration_commits']),
        'unreachable_commits': unreachable(batch['source_commits'] + batch['integration_commits']
                                           + [original['revision']], context),
        'notes': 'assets-work/World1/' + batch.get('notes_path', batch['name'] + '/notes.md'),
        'model_dir': model_dir if (ROOT / model_dir).is_dir() else None,
        'installed': files_for_model(batch['game_files'], model_dir),
        'original': original,
    }
    retained = files_for_model(batch.get('retained_game_files', {}), model_dir)
    superseded = {game: record['superseded_by'] for game, record in
                  files_for_model(batch.get('superseded_game_files', {}), model_dir).items()}
    if retained:
        entry['retained'] = retained
    if superseded:
        entry['superseded_by'] = superseded
    if batch.get('independent_review'):
        entry['independent_review'] = batch['independent_review']
    if batch['name'].startswith(REQUEST_LEDGER_PREFIX):
        entry['request'] = batch['name'][len(REQUEST_LEDGER_PREFIX):].split('/')[0]
    return entry


def pilot_batch_entry(name, record, context):
    _identity, commit, notes = record
    model_dir = WORLD_DIR / Path(notes).parent
    archive = model_dir / 'original' / f'{name}.bmd'
    claim = board_claim(context['pilot_literals'])
    original = original_record(context['pilot_base'], existing(archive),
                               sha256_file(archive) if archive.exists() else None, context)
    return {
        'name': PILOT_BATCH_NAME,
        'role': ROLE_PRIMARY,
        'owner': claim['agent'],
        'agent': claim['agent'],
        'branch': claim['branch'],
        'worktree': claim['worktree'],
        'source_commits': [commit],
        'integration_commits': [],
        'unreachable_commits': unreachable([commit, original['revision']], context),
        'notes': 'assets-work/World1/' + notes,
        'model_dir': existing(model_dir),
        'installed': {},
        'original': original,
    }


def batches_by_model(context):
    """Model -> batch entries in chronological order (pilot first, then ledger order)."""
    result = {}
    for name, record in sorted(context['previous'].items()):
        result.setdefault(name, []).append(pilot_batch_entry(name, record, context))
    matched = match_production_batches(context['ledger'], context['reservations'])
    for batch in context['ledger']:
        for name in batch['models']:
            result.setdefault(name, []).append(
                ledger_batch_entry(batch, name, matched.get(batch['name']), context))
    return result


# --- requests -------------------------------------------------------------------------------

def request_assignments(reservations):
    """Request id -> worker label of the reservation whose "ledger" is requests/<id>/delivery."""
    found = {}
    for agent, claim in sorted(reservations.items()):
        ledger = claim.get('ledger') or ''
        if ledger.startswith(REQUEST_LEDGER_PREFIX):
            found[ledger[len(REQUEST_LEDGER_PREFIX):].split('/')[0]] = agent
    return found


def read_requests(known, reservations):
    """Every request folder, checked just enough to index it; validate_request.py checks the rest."""
    assignments = request_assignments(reservations)
    found = []
    for path in sorted(REQUESTS_DIR.glob('*/request.json')):
        try:
            request = read_json(path)
        except ValueError as error:
            raise CatalogError(f'{rel(path)}: not valid JSON ({error})') from error
        if not isinstance(request, dict):
            raise CatalogError(f'{rel(path)}: must contain a JSON object')
        folder = path.parent.name
        problems = []
        if request.get('schema') != REQUEST_SCHEMA:
            problems.append(f'schema is not {REQUEST_SCHEMA}')
        if request.get('id') != folder:
            problems.append('id does not match the folder name')
        if request.get('status') not in REQUEST_STATUSES:
            problems.append(f'unknown status {request.get("status")!r}')
        models = [t.get('model') for t in request.get('targets', []) if isinstance(t, dict)]
        if not models or any(m not in known for m in models):
            problems.append(f'targets name unknown models: {models}')
        if problems:
            raise CatalogError(f'{rel(path)}: ' + '; '.join(problems))
        found.append({'id': folder, 'status': request['status'], 'kind': request.get('kind'),
                      'models': models, 'path': rel(path), 'assigned_to': assignments.get(folder)})
    return found


# --- client-review.json --------------------------------------------------------------------

def read_client_reviews(known):
    """Model -> {verdict, note, date} from the owner's client-review.json ({} when it is missing)."""
    if not CLIENT_REVIEW.exists():
        return {}
    try:
        reviews = read_json(CLIENT_REVIEW)
    except ValueError as error:
        raise CatalogError(f'{rel(CLIENT_REVIEW)}: not valid JSON ({error})') from error
    if not isinstance(reviews, dict):
        raise CatalogError(f'{rel(CLIENT_REVIEW)}: must contain a JSON object keyed by model name')
    found = {}
    for name, review in sorted(reviews.items()):
        where = f'{rel(CLIENT_REVIEW)}: {name}'
        if name not in known:
            raise CatalogError(f'{where} is not a World1 model')
        if not isinstance(review, dict) or set(review) != CLIENT_REVIEW_KEYS:
            raise CatalogError(f'{where} must have exactly {sorted(CLIENT_REVIEW_KEYS)}')
        if review['verdict'] not in VERDICTS:
            raise CatalogError(f'{where}: verdict must be one of {", ".join(VERDICTS)}')
        if not isinstance(review['note'], str) or not isinstance(review['date'], str) \
                or not DATE_PATTERN.match(review['date']):
            raise CatalogError(f'{where}: note must be text and date YYYY-MM-DD')
        found[name] = {key: review[key] for key in sorted(CLIENT_REVIEW_KEYS)}
    return found


def open_requests_for(name, requests):
    return [{'id': r['id'], 'status': r['status'], 'assigned_to': r['assigned_to']} for r in requests
            if name in r['models'] and r['status'] in OPEN_REQUEST_STATUSES]


# --- assembly -------------------------------------------------------------------------------

def model_status(name, model, batches, context):
    """accepted covers the ledger batches and the four pilot props, as on the asset board."""
    if model['scope_exclusion']:
        return STATUS_BLOCKED
    if batches:
        return STATUS_ACCEPTED
    if any(name in claim['models'] for claim in context['reservations'].values()):
        return STATUS_IN_PROGRESS
    return STATUS_UNCHANGED


def model_original(model, batches, context):
    if batches:
        return dict(batches[0]['original'])
    return original_record(context['baseline_short'], None, context['protected'].get(model['path']), context)


def model_entry(name, model, context):
    batches = context['batches'].get(name, [])
    bmd_path = ROOT / model['path']
    current = sha256_file(bmd_path) if bmd_path.exists() else None
    structure = bmd_structure(context['model_info'].get(name))
    textures, shared = texture_links(model, context['consumers'], name)
    original = model_original(model, batches, context)
    latest = batches[-1] if batches else None
    review = context['client_reviews'].get(name)
    return {
        'name': name,
        'type': model['type'],
        'bmd': model['path'],
        'identity': board_identity(name, model, context),
        'in_scope': not model['scope_exclusion'],
        'exclusion': model['scope_exclusion'],
        'status': model_status(name, model, batches, context),
        'client_verified': review is not None and review['verdict'] == VERDICT_LOOKS_GOOD,
        'client_review': review,
        'placement_count': len(model['placements']),
        'first_placement': first_placement(model['placements']),
        'textures': textures,
        'shared_with': shared,
        'structure': structure,
        'engine_controls': model_controls(name, context['controls'], structure),
        'current_sha256': current,
        'accepted_sha256': accepted_sha256(name, model['path']),
        'bmd_modified': current is not None and current != original['sha256'],
        'original': original,
        'last_batch': latest['name'] if latest else None,
        'model_dir': latest['model_dir'] if latest else None,
        'batches': batches,
        'previews': previews(name),
        'requests': open_requests_for(name, context['requests']),
    }


def generated_from():
    files = [DEPENDENCY_MAP, IDENTITIES, LEDGER, PRODUCTION_BATCHES, PROTECTED_BASELINE,
             COMBINED_VALIDATION, COMBINED_MODEL_INFO, PUBLICATION, ENGINE_CONTROLS, BOARD_SCRIPT,
             PILOT_HANDOFF, CLIENT_REVIEW]
    patterns = ['assets-work/World1/coordination/final-inspection/*/provenance.json',
                'assets-work/World1/coordination/final-inspection/*/' + FINAL_PREVIEW,
                'assets-work/World1/coordination/inspection/*/' + BASELINE_PREVIEW,
                'assets-work/World1/<Batch>/<Model>/original/<Model>.bmd',
                'assets-work/World1/<PilotModel>/original/<PilotModel>.bmd',
                'assets-work/World1/requests/*/request.json',
                'git history of HEAD (commit reachability)',
                'src/bin/Data/Object1/*.bmd']
    return sorted([rel(p) for p in files] + patterns)


def recorded_commits(context):
    """Every commit the catalog names, as written (original revisions shortened to 8 characters)."""
    commits = {context['baseline_short'], context['pilot_base']}
    commits.update(record[1] for record in context['previous'].values())
    for batch in context['ledger']:
        commits.update(batch['source_commits'])
        commits.update(batch['integration_commits'])
        commits.update(r[:SHORT_SHA] for r in batch.get('original_revisions', {}).values())
    return commits


def load_context():
    dependency = read_json(DEPENDENCY_MAP)
    previous, tavern, pilot_literals, tavern_literals = read_board_literals()
    validation = read_json(COMBINED_VALIDATION)
    known = set(dependency['models'])
    controls, notes = read_engine_controls(known)
    reservations = read_json(PRODUCTION_BATCHES)
    context = {
        'models': dependency['models'],
        'consumers': dependency['texture_consumers'],
        'baseline': dependency['baseline'], 'baseline_short': dependency['baseline'][:SHORT_SHA],
        'identities': read_json(IDENTITIES), 'ledger': read_json(LEDGER),
        'reservations': reservations, 'protected': read_json(PROTECTED_BASELINE),
        'validated': validation.get('original_archive_hashes', {}),
        'model_info': read_json(COMBINED_MODEL_INFO), 'publication': read_json(PUBLICATION),
        'previous': previous, 'tavern': tavern, 'pilot_literals': pilot_literals,
        'tavern_literals': tavern_literals, 'pilot_base': read_pilot_base(),
        'controls': controls, 'engine_notes': notes, 'requests': read_requests(known, reservations),
        'client_reviews': read_client_reviews(known),
    }
    context['reachable'] = reachable_commits(recorded_commits(context))
    context['batches'] = batches_by_model(context)
    return context


def count_statuses(entries):
    counts = {}
    for entry in entries:
        counts[entry['status']] = counts.get(entry['status'], 0) + 1
    return counts


def catalog_counts(typed, untyped, context):
    entries = list(typed.values()) + list(untyped.values())
    consumers = context['consumers']
    return {
        'models': len(entries),
        'typed_models': len(typed),
        'untyped_models': len(untyped),
        'placed_types': sum(1 for e in typed.values() if e['placement_count']),
        'placements': sum(e['placement_count'] for e in entries),
        'in_scope': sum(1 for e in entries if e['in_scope']),
        'excluded': sum(1 for e in entries if not e['in_scope']),
        'status': count_statuses(entries),
        'bmd_modified': sum(1 for e in entries if e['bmd_modified']),
        'client_verified': sum(1 for e in entries if e['client_verified']),
        'client_needs_work': sum(1 for e in entries if (e['client_review'] or {}).get('verdict') == VERDICT_NEEDS_WORK),
        'with_engine_controls': sum(1 for e in entries if e['engine_controls']),
        'textures': len(consumers),
        'shared_textures': sum(1 for users in consumers.values() if len(users) > 1),
        'previews': {'baseline': sum(1 for e in entries if 'baseline' in e['previews']),
                     'final': sum(1 for e in entries if 'final' in e['previews'])},
        'ledger_batches': len(context['ledger']),
        'unreachable_commits': sum(1 for reachable in context['reachable'].values() if not reachable),
        'requests': len(context['requests']),
        'open_requests': sum(1 for r in context['requests'] if r['status'] in OPEN_REQUEST_STATUSES),
    }


def build_catalog(context):
    typed, untyped = {}, {}
    for name, model in sorted(context['models'].items()):
        entry = model_entry(name, model, context)
        if model['type'] is None:
            untyped[name] = entry
            continue
        key = str(model['type'])
        if key in typed:
            raise CatalogError(f'Type {key} is used by {typed[key]["name"]} and {name}')
        typed[key] = entry
    publication = {key: context['publication'][key] for key in PUBLICATION_KEYS
                   if key in context['publication']}
    return {
        'schema': SCHEMA,
        'world': WORLD_NUMBER,
        'world_name': WORLD_NAME,
        'paths_relative_to': 'repository root',
        'baseline': context['baseline'],
        'pilot_base': context['pilot_base'],
        'texture_baseline': {'dir': rel(TEXTURE_BASELINE), 'revision': context['baseline_short']},
        'publication': publication,
        'generated_from': generated_from(),
        'engine_notes': context['engine_notes'],
        'counts': catalog_counts(typed, untyped, context),
        'models': typed,
        'untyped_models': untyped,
        'requests': [{k: r[k] for k in ('assigned_to', 'id', 'kind', 'models', 'path', 'status')}
                     for r in context['requests']],
    }


def stale_provenance(name):
    """Game files whose working-tree bytes differ from the final-inspection provenance hash."""
    stale = []
    for game, digest in sorted(provenance_files(name).items()):
        path = ROOT / game
        if not path.exists() or sha256_file(path) != digest:
            stale.append(game)
    return stale


def check_catalog(catalog, context):
    """Every placed type resolves to exactly one entry; report drift from accepted hashes."""
    problems, warnings = [], []
    placed = {}
    for name, model in context['models'].items():
        if model['placements']:
            placed.setdefault(model['type'], []).append(name)
    for kind, names in sorted(placed.items(), key=lambda item: (item[0] is None, item[0] or 0)):
        entry = catalog['models'].get(str(kind)) if kind is not None else None
        if len(names) != 1 or entry is None or entry['name'] != names[0]:
            problems.append(f'placed type {kind} ({names}) does not resolve to one catalog entry')
    for entry in list(catalog['models'].values()) + list(catalog['untyped_models'].values()):
        if entry['current_sha256'] is None:
            problems.append(f'{entry["name"]}: {entry["bmd"]} is missing')
            continue
        stale = stale_provenance(entry['name'])
        if stale:
            warnings.append(f'{entry["name"]}: final-inspection provenance is stale for {stale}; '
                            're-run Blender inspect_final.py')
    return problems, warnings, len(placed)


def render(catalog):
    return json.dumps(catalog, indent=1, sort_keys=True, ensure_ascii=False) + '\n'


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--check', action='store_true',
                        help='do not write; exit 1 when catalog.json is missing or out of date')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    try:
        context = load_context()
        catalog = build_catalog(context)
    except CatalogError as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    problems, warnings, placed_types = check_catalog(catalog, context)
    for warning in warnings:
        print(f'warning: {warning}', file=sys.stderr)
    for problem in problems:
        print(f'error: {problem}', file=sys.stderr)
    if problems:
        return 1
    text = render(catalog)
    counts = catalog['counts']
    print(f'{rel(OUTPUT)}: {counts["models"]} models ({counts["typed_models"]} typed, '
          f'{counts["untyped_models"]} untyped); {placed_types} placed types each resolve to one entry; '
          f'status {json.dumps(counts["status"], sort_keys=True)}; {counts["open_requests"]} open requests')
    if args.check:
        current = OUTPUT.read_text(encoding='utf-8') if OUTPUT.exists() else None
        if current != text:
            print(f'{rel(OUTPUT)} is out of date; run without --check', file=sys.stderr)
            return 1
        return 0
    OUTPUT.write_text(text, encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
