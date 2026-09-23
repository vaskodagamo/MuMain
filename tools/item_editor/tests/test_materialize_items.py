"""tools/world_editor/materialize_variant.py original --items: the item editor's A/B originals, built from
git in a small scratch repository (no real catalog or data needed)."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOOLS / 'world_editor'))

import materialize_variant as mv  # noqa: E402

NAME_BYTES = 32
PLAIN_BMD_VERSION = 0x0A


def plain_bmd(name, textures):
    """A BMD the script can read: header, one empty mesh per texture name."""
    body = name.encode().ljust(NAME_BYTES, b'\0') + struct.pack('<hhh', len(textures), 0, 0)
    for texture in textures:
        body += struct.pack('<hhhhh', 0, 0, 0, 0, 0) + texture.encode().ljust(NAME_BYTES, b'\0')
    return b'BMD' + bytes([PLAIN_BMD_VERSION]) + body


def sha(data):
    return hashlib.sha256(data).hexdigest()


class ScratchRepo:
    """A git repository with src/bin/Data and the catalogs the script reads."""

    def __init__(self, root):
        self.root = Path(root)
        self.git('init', '-q')
        self.git('config', 'user.email', 'test@example.com')
        self.git('config', 'user.name', 'test')

    def git(self, *args):
        result = subprocess.run(['git', '-C', str(self.root), *args], capture_output=True, check=True)
        return result.stdout.decode().strip()

    def write(self, path, data):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(data)

    def commit(self, message):
        self.git('add', '-A')
        self.git('commit', '-q', '-m', message)
        return self.git('rev-parse', 'HEAD')

    def read_output(self, path):
        return (self.root / 'out' / 'ab' / 'original' / path).read_bytes()


ORIGINAL_SWORD = plain_bmd('Sword01', ['sword02.jpg'])
CHANGED_SWORD = plain_bmd('Sword01', ['sword02.jpg', 'sword02.jpg'])
ORIGINAL_BLADE_TEXTURE = b'OZJ original blade'
CHANGED_BLADE_TEXTURE = b'OZJ changed blade'
HELM = plain_bmd('HelmMale01', ['skin_barbarian_01.jpg', 'hide.jpg'])
SPHERE = plain_bmd('Sphere', ['flare.tga', 'lost.jpg'])
SKIN_TEXTURE = b'OZJ skin'
FLARE_TEXTURE = b'OZT flare'
TREE = plain_bmd('Tree01', ['tree.jpg'])
TREE_TEXTURE = b'OZJ tree'


def model_entry(bmd, revision, data, textures, texture_dirs=None, role='item'):
    folder = bmd.rsplit('/', 1)[0]
    return {'role': role, 'bmd': bmd, 'folder': folder, 'texture_dirs': texture_dirs or [folder],
            'textures': textures, 'original': {'revision': revision, 'sha256': sha(data)}}


def item_catalog(revision, sword_sha=None):
    sword = model_entry('src/bin/Data/Item/Sword01.bmd', revision, ORIGINAL_SWORD,
                        {'sword02.jpg': 'src/bin/Data/Item/sword02.OZJ'})
    if sword_sha is not None:
        sword['original']['sha256'] = sword_sha
    helm = model_entry('src/bin/Data/Player/HelmMale01.bmd', revision, HELM,
                       {'skin_barbarian_01.jpg': 'src/bin/Data/Player/skin_barbarian_01.OZJ'})
    sphere = model_entry('src/bin/Data/Item/Sphere.bmd', revision, SPHERE, {'flare.tga': 'src/bin/Data/Effect/flare.OZT'},
                         texture_dirs=['src/bin/Data/Effect', 'src/bin/Data/Item'])
    return {
        'schema': mv.ITEM_CATALOG_SCHEMA,
        'items': {
            '0-0': {'key': '0-0', 'models': [sword]},
            '7-0': {'key': '7-0', 'models': [helm]},
            '12-100': {'key': '12-100', 'models': [sphere]},
            '12-101': {'key': '12-101', 'models': [dict(sphere)]},
        },
    }


def world_catalog(revision):
    tree = {'name': 'Tree01', 'type': 130, 'bmd': 'src/bin/Data/Object1/Tree01.bmd',
            'original': {'revision': revision, 'sha256': sha(TREE)}}
    return {'schema': mv.CATALOG_SCHEMA, 'models': {'Tree01': tree}}


class MaterializeItems(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.repo = ScratchRepo(self.scratch.name)
        self.repo.write('src/bin/Data/Item/Sword01.bmd', ORIGINAL_SWORD)
        self.repo.write('src/bin/Data/Item/Sword02.OZJ', ORIGINAL_BLADE_TEXTURE)  # other case than the BMD names
        self.repo.write('src/bin/Data/Player/HelmMale01.bmd', HELM)
        self.repo.write('src/bin/Data/Player/skin_barbarian_01.OZJ', SKIN_TEXTURE)
        self.repo.write('src/bin/Data/Item/Sphere.bmd', SPHERE)
        self.repo.write('src/bin/Data/Effect/flare.OZT', FLARE_TEXTURE)
        self.repo.write('src/bin/Data/Object1/Tree01.bmd', TREE)
        self.repo.write('src/bin/Data/Object1/tree.OZJ', TREE_TEXTURE)
        self.original = self.repo.commit('originals')
        # The art rebuild changes the sword and its texture afterwards.
        self.repo.write('src/bin/Data/Item/Sword01.bmd', CHANGED_SWORD)
        self.repo.write('src/bin/Data/Item/Sword02.OZJ', CHANGED_BLADE_TEXTURE)
        self.write_catalogs(item_catalog(self.original))
        self.repo.commit('rebuild')

    def tearDown(self):
        self.scratch.cleanup()

    def write_catalogs(self, items):
        self.repo.write('assets-work/Items/catalog.json', json.dumps(items).encode())
        self.repo.write('assets-work/World1/catalog.json', json.dumps(world_catalog(self.original)).encode())

    def run_items(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            mv.materialize_items(self.repo.root, 'original')
        return output.getvalue()

    def run_world(self):
        with contextlib.redirect_stdout(io.StringIO()):
            mv.materialize(self.repo.root, 'original', 1)

    def manifest(self):
        return json.loads(self.repo.read_output(mv.ITEMS_MANIFEST))

    def test_writes_each_original_model_and_the_textures_it_names(self):
        self.run_items()
        self.assertEqual(self.repo.read_output('Data/Item/Sword01.bmd'), ORIGINAL_SWORD)
        self.assertEqual(self.repo.read_output('Data/Item/Sword02.OZJ'), ORIGINAL_BLADE_TEXTURE)
        self.assertEqual(self.repo.read_output('Data/Player/HelmMale01.bmd'), HELM)
        self.assertEqual(self.repo.read_output('Data/Player/skin_barbarian_01.OZJ'), SKIN_TEXTURE)
        # A texture the catalog finds in another folder than the model's goes to that folder.
        self.assertEqual(self.repo.read_output('Data/Effect/flare.OZT'), FLARE_TEXTURE)

    def test_manifest_lists_models_items_textures_and_missing_ones(self):
        self.run_items()
        manifest = self.manifest()
        self.assertEqual(manifest['schema'], mv.MANIFEST_SCHEMA)
        self.assertEqual(manifest['domain'], 'items')
        sword = manifest['models']['Data/Item/Sword01.bmd']
        self.assertEqual(sword, {'items': ['0-0'], 'revision': self.original, 'sha256': sha(ORIGINAL_SWORD),
                                 'source': 'git', 'textures': ['sword02.jpg']})
        self.assertEqual(manifest['models']['Data/Item/Sphere.bmd']['items'], ['12-100', '12-101'])
        self.assertEqual(manifest['textures']['Data/Effect/flare.OZT']['used_by'], ['Data/Item/Sphere.bmd'])
        self.assertEqual(manifest['missing_textures'], ['Data/Item/Sphere.bmd: lost.jpg'])
        self.assertNotIn('Data/Player/hide.OZJ', manifest['textures'])  # "hid..." marks a hidden mesh

    def test_the_output_is_deterministic(self):
        self.run_items()
        first = {path: path.read_bytes() for path in sorted((self.repo.root / 'out').rglob('*')) if path.is_file()}
        self.run_items()
        second = {path: path.read_bytes() for path in sorted((self.repo.root / 'out').rglob('*')) if path.is_file()}
        self.assertEqual(first, second)

    def test_a_wrong_original_hash_stops_without_replacing_the_output(self):
        self.run_items()
        before = self.repo.read_output(mv.ITEMS_MANIFEST)
        self.write_catalogs(item_catalog(self.original, sword_sha='0' * 64))
        with self.assertRaisesRegex(mv.VariantError, 'SHA-256'):
            self.run_items()
        self.assertEqual(self.repo.read_output(mv.ITEMS_MANIFEST), before)

    def test_world_and_item_runs_keep_each_others_files(self):
        self.run_world()
        self.run_items()
        self.assertEqual(self.repo.read_output('Data/Object1/Tree01.bmd'), TREE)
        self.assertTrue((self.repo.root / 'out/ab/original' / mv.WORLD_MANIFEST).is_file())
        items_manifest = self.repo.read_output(mv.ITEMS_MANIFEST)
        self.run_world()
        self.assertEqual(self.repo.read_output(mv.ITEMS_MANIFEST), items_manifest)
        self.assertEqual(self.repo.read_output('Data/Item/Sword01.bmd'), ORIGINAL_SWORD)
        self.assertEqual(self.repo.read_output('Data/Object1/tree.OZJ'), TREE_TEXTURE)

    def test_items_and_world_are_separate_runs(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            mv.main(['original', '--items', '--world', '1'])


if __name__ == '__main__':
    unittest.main()
