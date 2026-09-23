#!/usr/bin/env python3
"""Materialize an asset variant for the editors' A/B compare (Map Editor -> Assets tab, Item Editor ->
Browse -> A/B compare).

Usage (from anywhere inside the repository; Python 3.9+, standard library only):

    python3 tools/world_editor/materialize_variant.py original            # World1 (Lorencia)
    python3 tools/world_editor/materialize_variant.py original --world 1
    python3 tools/world_editor/materialize_variant.py original --items    # every item of the item catalog

Variants:

    original  The world's models as they were before the art rebuild, written to
              <repo>/out/ab/original/Data/Object{N}/ (the same layout as src/bin/Data):
              - each catalog model's BMD at its catalog.json "original.revision", checked against
                "original.sha256"; the "original.archive" copy is used when git cannot show it;
              - every texture that original BMD names (tree.jpg -> tree.OZJ, bark.tga -> bark.OZT),
                taken from git at the model's original revision. Where that revision is the
                catalog's "texture_baseline" revision, the copy in the texture baseline folder
                (assets-work/World1/coordination/texture-baseline) must hold the same bytes, and
                is used when git cannot show the file. A texture that models of different
                original revisions share, with different bytes, comes from the oldest of them.
    current   Not materialized: it is the checkout's src/bin/Data.

With --items, "original" is every model of assets-work/Items/catalog.json (each item's own model,
armour class variants, hand and inventory models) as it was before the art rebuild: the BMD at its
catalog "original.revision", checked against "original.sha256", and every texture that original BMD
names, from the folder the catalog lists for it (Item, Player, Effect, Skill, ...) at the same
revision. They go to <repo>/out/ab/original/Data/Item/, Data/Player/, ... with the manifest
out/ab/original/items-manifest.json (per model file: the items using it, revision, SHA-256,
textures). A texture that git does not have at that revision is listed under "missing_textures"
(the catalog lists the same ones) instead of failing the run.

The client reads the variant from there when you pick "Original" in the Assets tab; nothing under
src/bin/Data is touched. The script also writes out/ab/<variant>/manifest.json (which revision and
source every file came from, and the SHA-256 of the catalog.json it was built from, so the editor
can tell when the variant is older than the catalog). out/ is not tracked by git.

The output is deterministic: the same catalog.json and git history give the same bytes (sorted
keys, no timestamps). A run rebuilds its own part of the folder from scratch, so files of models
that left the catalog disappear: a world run owns manifest.json and Data/Object*, an --items run
owns items-manifest.json and the other Data/ folders; each keeps the other's part as it was. Run
it again after catalog.json changes. Exit code 0 on success, 1 on any error (nothing is replaced
then).
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

VARIANTS = ('original',)
CATALOG_SCHEMA = 'mu-world-catalog/1'
MANIFEST_SCHEMA = 'mu-ab-variant/1'
DATA_PREFIX = 'src/bin/Data/'
OUT_DIR = Path('out') / 'ab'
DEFAULT_WORLD = 1
WORLD_MANIFEST = 'manifest.json'
WORLD_FOLDER_PREFIX = 'object'  # Data/Object{N}, matched without regard to case
ITEM_CATALOG = Path('assets-work') / 'Items' / 'catalog.json'
ITEM_CATALOG_SCHEMA = 'mu-item-catalog/1'
ITEMS_MANIFEST = 'items-manifest.json'
ITEMS_DOMAIN = 'items'
DATA_DIR = 'Data'

# BMD container: "BMD", a version byte, then the model. Version 0x0C holds an int32 size and the
# model encrypted with the map-file cipher (MapFileDecrypt in Render/Terrain/ZzzLodTerrain.h);
# version 0x0A holds the model in plain bytes. Record sizes follow Render/Models/ZzzBMD.h.
BMD_MAGIC = b'BMD'
BMD_VERSION_PLAIN = 0x0A
BMD_VERSION_ENCRYPTED = 0x0C
MAP_XOR_KEY = bytes((0xD1, 0x73, 0x52, 0xF6, 0xD2, 0x9A, 0xCB, 0x27,
                     0x3E, 0xAF, 0x59, 0x31, 0x37, 0xB3, 0xE7, 0xA2))
MAP_KEY_START = 0x5E
MAP_KEY_STEP = 0x3D
NAME_BYTES = 32
VERTEX_BYTES = 16      # Vertex_t
NORMAL_BYTES = 20      # Normal_t
TEXCOORD_BYTES = 8     # TexCoord_t
TRIANGLE_BYTES = 64    # Triangle_t2, the on-disk triangle record
KEY_BYTES = 12         # vec3_t per animation key
HIDDEN_TEXTURE_PREFIX = 'hid'
TEXTURE_CONTAINERS = {'.jpg': '.OZJ', '.tga': '.OZT'}


class VariantError(Exception):
    pass


def run_git(repo, *args, check=True):
    result = subprocess.run(['git', '-C', str(repo), *args], capture_output=True)
    if check and result.returncode != 0:
        raise VariantError('git %s failed: %s' % (' '.join(args), result.stderr.decode(errors='replace').strip()))
    return result


def find_repo_root():
    here = Path(__file__).resolve().parent
    result = subprocess.run(['git', '-C', str(here), 'rev-parse', '--show-toplevel'], capture_output=True)
    if result.returncode != 0:
        raise VariantError('not inside a git checkout: %s' % here)
    root = Path(result.stdout.decode().strip())
    if not (root / 'src' / 'bin' / 'Data').is_dir():
        raise VariantError('%s has no src/bin/Data' % root)
    return root


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def git_blob(repo, revision, path):
    """The bytes of `path` at `revision`, or None when git cannot show them."""
    result = run_git(repo, 'cat-file', 'blob', '%s:%s' % (revision, path), check=False)
    return result.stdout if result.returncode == 0 else None


def decrypt_map_file(data):
    out = bytearray(len(data))
    key = MAP_KEY_START
    for index, value in enumerate(data):
        out[index] = ((value ^ MAP_XOR_KEY[index % len(MAP_XOR_KEY)]) - key) & 0xFF
        key = (value + MAP_KEY_STEP) & 0xFF
    return bytes(out)


def plain_model(data, label):
    if data[:len(BMD_MAGIC)] != BMD_MAGIC or len(data) < len(BMD_MAGIC) + 1:
        raise VariantError('%s is not a BMD file' % label)
    version = data[len(BMD_MAGIC)]
    if version == BMD_VERSION_PLAIN:
        return data[len(BMD_MAGIC) + 1:]
    if version != BMD_VERSION_ENCRYPTED:
        raise VariantError('%s has BMD version 0x%02X, the client reads 0x0A and 0x0C' % (label, version))
    (size,) = struct.unpack_from('<i', data, len(BMD_MAGIC) + 1)
    start = len(BMD_MAGIC) + 1 + 4
    if size < 0 or start + size > len(data):
        raise VariantError('%s: encrypted size %d does not fit the file' % (label, size))
    return decrypt_map_file(data[start:start + size])


def bmd_texture_names(data, label):
    """The texture file name of every mesh, in mesh order."""
    model = plain_model(data, label)
    try:
        offset = NAME_BYTES
        mesh_count, _bones, _actions = struct.unpack_from('<hhh', model, offset)
        offset += 6
        names = []
        for _ in range(mesh_count):
            vertices, normals, texcoords, triangles, _texture = struct.unpack_from('<hhhhh', model, offset)
            offset += 10
            offset += vertices * VERTEX_BYTES + normals * NORMAL_BYTES
            offset += texcoords * TEXCOORD_BYTES + triangles * TRIANGLE_BYTES
            raw = model[offset:offset + NAME_BYTES]
            if len(raw) != NAME_BYTES:
                raise struct.error('texture name past the end')
            names.append(raw.split(b'\0', 1)[0].decode('latin-1'))
            offset += NAME_BYTES
        return names
    except struct.error as error:
        raise VariantError('%s: broken mesh table (%s)' % (label, error))


def container_name(texture, label):
    """tree.jpg -> tree.OZJ; None for a hidden-mesh marker."""
    if texture.lower().startswith(HIDDEN_TEXTURE_PREFIX):
        return None
    stem, dot, extension = texture.rpartition('.')
    container = TEXTURE_CONTAINERS.get('.' + extension.lower()) if dot else None
    if container is None:
        raise VariantError('%s names texture %r, which the client cannot load (.jpg or .tga only)' % (label, texture))
    return stem + container


class TreeListing:
    """Case-insensitive file names of one folder at one revision (the client matches names that way)."""

    def __init__(self, repo):
        self.repo = repo
        self.cache = {}

    def find(self, revision, folder, name):
        key = (revision, folder)
        if key not in self.cache:
            listing = run_git(self.repo, 'ls-tree', '--name-only', revision, folder + '/').stdout.decode()
            self.cache[key] = {Path(line).name.lower(): line for line in listing.splitlines() if line}
        return self.cache[key].get(name.lower())


def load_catalog(repo, world):
    path = repo / 'assets-work' / ('World%d' % world) / 'catalog.json'
    if not path.is_file():
        raise VariantError('no catalog for World%d: %s' % (world, path.relative_to(repo)))
    raw = path.read_bytes()
    catalog = json.loads(raw)
    if catalog.get('schema') != CATALOG_SCHEMA:
        raise VariantError('%s is not a %s file' % (path.relative_to(repo), CATALOG_SCHEMA))
    return catalog, path.relative_to(repo).as_posix(), sha256(raw)


def catalog_models(catalog):
    models = list(catalog.get('models', {}).values()) + list(catalog.get('untyped_models', {}).values())
    return sorted(models, key=lambda model: model['name'])


def original_bmd(repo, model):
    original = model.get('original') or {}
    revision = original.get('revision')
    bmd = model.get('bmd', '')
    if not revision or not bmd.startswith(DATA_PREFIX):
        raise VariantError('%s: catalog has no original revision or a BMD outside %s' % (model['name'], DATA_PREFIX))
    data, source = git_blob(repo, revision, bmd), 'git'
    if data is None and original.get('archive'):
        archive = repo / original['archive']
        data, source = (archive.read_bytes(), 'archive') if archive.is_file() else (None, None)
    if data is None:
        raise VariantError('%s: neither git (%s:%s) nor the archive has the original BMD' % (model['name'], revision, bmd))
    expected = original.get('sha256')
    if expected and sha256(data) != expected:
        raise VariantError('%s: original BMD from %s has SHA-256 %s, catalog says %s' %
                           (model['name'], source, sha256(data), expected))
    return {'revision': revision, 'path': bmd, 'data': data, 'source': source}


def collect(repo, catalog):
    """Every output file: models -> BMD + texture names, and texture path -> consumer revisions."""
    listing = TreeListing(repo)
    models, textures = {}, {}
    for model in catalog_models(catalog):
        bmd = original_bmd(repo, model)
        folder = bmd['path'].rsplit('/', 1)[0]
        names = []
        for texture in bmd_texture_names(bmd['data'], '%s (%s)' % (model['name'], bmd['revision'])):
            container = container_name(texture, model['name'])
            if container is None or texture in names:
                continue
            names.append(texture)
            git_path = listing.find(bmd['revision'], folder, container)
            if git_path is None:
                raise VariantError('%s names %s, but %s/%s is not in git at %s' %
                                   (model['name'], texture, folder, container, bmd['revision']))
            textures.setdefault(git_path, {})[model['name']] = bmd['revision']
        models[model['name']] = {'type': model.get('type'), 'bmd': bmd, 'textures': names}
    return models, textures


def oldest_first(repo, revisions):
    """Sorts revisions so that every one comes after its ancestors."""
    ordered = []
    for revision in sorted(revisions):
        position = len(ordered)
        for index, other in enumerate(ordered):
            if run_git(repo, 'merge-base', '--is-ancestor', revision, other, check=False).returncode == 0:
                position = index
                break
        ordered.insert(position, revision)
    return ordered


def commit_of(repo, revision):
    return run_git(repo, 'rev-parse', '--verify', revision + '^{commit}').stdout.decode().strip()


def baseline_copy(repo, catalog, revision, git_path):
    """The texture baseline folder's copy of `git_path`, when that folder holds `revision`'s textures."""
    baseline = catalog.get('texture_baseline') or {}
    if not baseline.get('revision') or not baseline.get('dir'):
        return None
    if commit_of(repo, baseline['revision']) != commit_of(repo, revision):
        return None
    folder = repo / baseline['dir']
    name = Path(git_path).name.lower()
    matches = [entry for entry in folder.iterdir() if entry.name.lower() == name] if folder.is_dir() else []
    return matches[0].read_bytes() if matches else None


def resolve_texture(repo, catalog, git_path, consumers, warnings):
    versions = {}
    for revision in oldest_first(repo, set(consumers.values())):
        from_git = git_blob(repo, revision, git_path)
        from_baseline = baseline_copy(repo, catalog, revision, git_path)
        if from_git is not None and from_baseline is not None and from_git != from_baseline:
            raise VariantError('%s at %s differs from its texture-baseline copy' % (git_path, revision))
        data = from_git if from_git is not None else from_baseline
        if data is None:
            raise VariantError('%s: git cannot show it at %s and the texture baseline has no copy' % (git_path, revision))
        versions[revision] = (data, 'git' if from_git is not None else 'texture-baseline')
    chosen = next(iter(versions))
    differing = sorted(name for name, revision in consumers.items() if versions[revision][0] != versions[chosen][0])
    if differing:
        warnings.append('%s: %s get the %s bytes (their own original revision has other bytes)' %
                        (git_path, ', '.join(differing), chosen))
    data, source = versions[chosen]
    return {'revision': chosen, 'data': data, 'source': source, 'used_by': sorted(consumers)}


def output_path(variant_root, repo_path):
    return variant_root / 'Data' / repo_path[len(DATA_PREFIX):]


def build_manifest(world, variant, catalog_path, catalog_sha, models, textures):
    return {
        'schema': MANIFEST_SCHEMA,
        'variant': variant,
        'world': world,
        'catalog': catalog_path,
        'catalog_sha256': catalog_sha,
        'models': {
            name: {
                'type': entry['type'],
                'bmd': 'Data/' + entry['bmd']['path'][len(DATA_PREFIX):],
                'revision': entry['bmd']['revision'],
                'sha256': sha256(entry['bmd']['data']),
                'source': entry['bmd']['source'],
                'textures': entry['textures'],
            } for name, entry in models.items()
        },
        'textures': {
            'Data/' + path[len(DATA_PREFIX):]: {
                'revision': entry['revision'],
                'sha256': sha256(entry['data']),
                'source': entry['source'],
                'used_by': entry['used_by'],
            } for path, entry in textures.items()
        },
    }


def world_owns(parts):
    """True for the entries of a variant folder that a world run writes: manifest.json, Data/Object{N}."""
    if parts == (WORLD_MANIFEST,):
        return True
    return len(parts) == 2 and parts[0] == DATA_DIR and parts[1].lower().startswith(WORLD_FOLDER_PREFIX)


def items_own(parts):
    """True for the entries an --items run writes: items-manifest.json and every other Data/ folder."""
    if parts == (ITEMS_MANIFEST,):
        return True
    return len(parts) == 2 and parts[0] == DATA_DIR and not world_owns(parts)


def variant_entries(target):
    """The top-level files and the Data/ folders of a variant folder, as relative path parts."""
    if not target.is_dir():
        return []
    entries = []
    for entry in sorted(target.iterdir()):
        if entry.name == DATA_DIR and entry.is_dir():
            entries += [(DATA_DIR, child.name) for child in sorted(entry.iterdir())]
        else:
            entries.append((entry.name,))
    return entries


def keep_other_runs_files(target, staging, other_owns):
    """Moves what the other kind of run wrote into the previous `target` over to `staging`."""
    for parts in variant_entries(target):
        if not other_owns(parts):
            continue
        destination = staging.joinpath(*parts)
        destination.parent.mkdir(parents=True, exist_ok=True)
        target.joinpath(*parts).rename(destination)


def write_variant(target, files, manifest_name, manifest, other_owns):
    """Writes `files` (repo path, bytes) and the manifest, keeping the other kind of run's part."""
    staging = target.with_name(target.name + '.partial')
    shutil.rmtree(staging, ignore_errors=True)
    for repo_path, data in sorted(files):
        path = output_path(staging, repo_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    (staging / manifest_name).write_text(json.dumps(manifest, indent=1, sort_keys=True) + '\n', encoding='utf-8')
    keep_other_runs_files(target, staging, other_owns)
    shutil.rmtree(target, ignore_errors=True)
    staging.rename(target)
    return sum(len(data) for _, data in files)


def materialize(repo, variant, world):
    catalog, catalog_path, catalog_sha = load_catalog(repo, world)
    models, consumers = collect(repo, catalog)
    warnings = []
    textures = {path: resolve_texture(repo, catalog, path, users, warnings) for path, users in sorted(consumers.items())}
    manifest = build_manifest(world, variant, catalog_path, catalog_sha, models, textures)
    target = repo / OUT_DIR / variant
    files = [(entry['bmd']['path'], entry['bmd']['data']) for entry in models.values()]
    files += [(path, entry['data']) for path, entry in textures.items()]
    size = write_variant(target, files, WORLD_MANIFEST, manifest, items_own)
    for warning in warnings:
        print('warning: ' + warning)
    print('%s: %d models and %d textures (%.1f MB) in %s' %
          (variant, len(models), len(textures), size / 1e6, target.relative_to(repo).as_posix()))


# --- Items (--items) --------------------------------------------------------------------------------

def load_item_catalog(repo):
    path = repo / ITEM_CATALOG
    if not path.is_file():
        raise VariantError('no item catalog: %s' % ITEM_CATALOG.as_posix())
    raw = path.read_bytes()
    catalog = json.loads(raw)
    if catalog.get('schema') != ITEM_CATALOG_SCHEMA:
        raise VariantError('%s is not a %s file' % (ITEM_CATALOG.as_posix(), ITEM_CATALOG_SCHEMA))
    return catalog, ITEM_CATALOG.as_posix(), sha256(raw)


def item_sort_key(key):
    group, _, index = key.partition('-')
    return (int(group), int(index))


def item_model_files(catalog):
    """Every model file of the item catalog once: BMD path -> its catalog model and the items using it."""
    files = {}
    items = catalog.get('items', {})
    for key in sorted(items, key=item_sort_key):
        for model in items[key].get('models', []):
            bmd = model.get('bmd', '')
            entry = files.setdefault(bmd, {'model': model, 'items': []})
            if (entry['model'].get('original') or {}) != (model.get('original') or {}):
                raise VariantError('%s: items %s and %s give it different originals' % (bmd, entry['items'][0], key))
            if key not in entry['items']:
                entry['items'].append(key)
    return files


def item_texture_folders(model, texture):
    """Where the client finds `texture` of `model`: the catalog's container for it, then the catalog's
    texture folders of the model, then the model's own folder."""
    folders = []
    for name, container in (model.get('textures') or {}).items():
        if container and name.lower() == texture.lower():
            folders.append(container.rsplit('/', 1)[0])
    folders += list(model.get('texture_dirs') or []) + [model['bmd'].rsplit('/', 1)[0]]
    return list(dict.fromkeys(folders))


def find_item_texture(listing, revision, model, texture, container):
    for folder in item_texture_folders(model, texture):
        found = listing.find(revision, folder, container)
        if found is not None:
            return found
    return None


def collect_items(repo, catalog):
    """Models -> original BMD, texture names and items; texture path -> consumer revisions; missing textures."""
    listing = TreeListing(repo)
    models, consumers, missing = {}, {}, []
    for bmd_path, entry in sorted(item_model_files(catalog).items()):
        model = entry['model']
        bmd = original_bmd(repo, dict(model, name=bmd_path))
        names = []
        for texture in bmd_texture_names(bmd['data'], '%s (%s)' % (bmd_path, bmd['revision'])):
            container = container_name(texture, bmd_path)
            if container is None or any(texture.lower() == name.lower() for name in names):
                continue
            names.append(texture)
            git_path = find_item_texture(listing, bmd['revision'], model, texture, container)
            if git_path is None:
                missing.append('%s: %s' % (data_path(bmd_path), texture))
                continue
            consumers.setdefault(git_path, {})[bmd_path] = bmd['revision']
        models[bmd_path] = {'bmd': bmd, 'textures': names, 'items': entry['items']}
    return models, consumers, missing


def data_path(repo_path):
    return DATA_DIR + '/' + repo_path[len(DATA_PREFIX):]


def build_items_manifest(variant, catalog_path, catalog_sha, models, textures, missing):
    return {
        'schema': MANIFEST_SCHEMA,
        'variant': variant,
        'domain': ITEMS_DOMAIN,
        'catalog': catalog_path,
        'catalog_sha256': catalog_sha,
        'models': {
            data_path(path): {
                'items': entry['items'],
                'revision': entry['bmd']['revision'],
                'sha256': sha256(entry['bmd']['data']),
                'source': entry['bmd']['source'],
                'textures': entry['textures'],
            } for path, entry in models.items()
        },
        'textures': {
            data_path(path): {
                'revision': entry['revision'],
                'sha256': sha256(entry['data']),
                'source': entry['source'],
                'used_by': [data_path(user) for user in entry['used_by']],
            } for path, entry in textures.items()
        },
        'missing_textures': sorted(missing),
    }


def materialize_items(repo, variant):
    catalog, catalog_path, catalog_sha = load_item_catalog(repo)
    models, consumers, missing = collect_items(repo, catalog)
    warnings = []
    textures = {path: resolve_texture(repo, catalog, path, users, warnings) for path, users in sorted(consumers.items())}
    manifest = build_items_manifest(variant, catalog_path, catalog_sha, models, textures, missing)
    target = repo / OUT_DIR / variant
    files = [(path, entry['bmd']['data']) for path, entry in models.items()]
    files += [(path, entry['data']) for path, entry in textures.items()]
    size = write_variant(target, files, ITEMS_MANIFEST, manifest, world_owns)
    for warning in warnings:
        print('warning: ' + warning)
    for texture in sorted(missing):
        print('warning: not in git at its original revision (the client cannot load it either): ' + texture)
    print('%s: %d item models and %d textures (%.1f MB) in %s' %
          (variant, len(models), len(textures), size / 1e6, target.relative_to(repo).as_posix()))


def main(argv):
    parser = argparse.ArgumentParser(description='Materialize an A/B asset variant for the world and item editors.')
    parser.add_argument('variant', choices=VARIANTS)
    parser.add_argument('--world', type=int, default=None, help='world folder number (default 1)')
    parser.add_argument('--items', action='store_true', help='every model of assets-work/Items/catalog.json')
    args = parser.parse_args(argv)
    if args.items and args.world is not None:
        parser.error('--items and --world are separate runs')
    try:
        if args.items:
            materialize_items(find_repo_root(), args.variant)
        else:
            materialize(find_repo_root(), args.variant, args.world if args.world is not None else DEFAULT_WORLD)
    except (VariantError, OSError, ValueError) as error:
        print('error: %s' % error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
