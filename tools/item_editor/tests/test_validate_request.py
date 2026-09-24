"""validate_request.py on the README's complete example (written to a scratch tree) and on broken copies."""

import copy
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
REQUESTS = ROOT / 'assets-work' / 'Items' / 'requests'
sys.path.insert(0, str(REQUESTS))

import validate_request as validator  # noqa: E402

EXAMPLE_HEADING = '## Complete example'


def readme_example():
    text = (REQUESTS / 'README.md').read_text(encoding='utf-8')
    block = re.search(r'```json\n(.*?)\n```', text.split(EXAMPLE_HEADING, 1)[1], re.S)
    return json.loads(block.group(1))


def tiny_jpeg(width, height):
    """SOI, a baseline start-of-frame with the size, EOI: all the validator reads."""
    frame = struct.pack('>HBHHB', 11, 8, height, width, 1) + bytes((1, 0x11, 0))
    return b'\xff\xd8' + b'\xff\xc0' + frame + b'\xff\xd9'


@unittest.skipUnless((ROOT / 'assets-work/Items/catalog.json').exists(), 'catalog.json not built')
class ReadmeExample(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp(prefix='mu-item-request-'))
        (self.scratch / 'src').symlink_to(ROOT / 'src')
        items = self.scratch / 'assets-work' / 'Items'
        items.mkdir(parents=True)
        for name in ('catalog.json', 'tiers.json', 'assignments.json', 'render-facts.json'):
            shutil.copy(ROOT / 'assets-work' / 'Items' / name, items / name)
        self.request = readme_example()
        self.folder = items / 'requests' / self.request['id']
        (self.folder / 'captures').mkdir(parents=True)
        (self.folder / 'brief.md').write_text('# Kris: sharper blade\n', encoding='utf-8')
        for capture in self.request['evidence']['captures']:
            (self.scratch / capture['file']).write_bytes(tiny_jpeg(*capture['resolution']))
        for image in self.request['change']['reference_images']:
            (self.scratch / image).write_bytes(tiny_jpeg(800, 600))
        self.reference = validator.load_reference(self.scratch, items, git_root=ROOT)

    def tearDown(self):
        shutil.rmtree(self.scratch)

    def validate(self, request):
        (self.folder / 'request.json').write_text(json.dumps(request, indent=2), encoding='utf-8')
        return validator.validate(self.folder, self.reference)

    def assert_error(self, request, fragment):
        report = self.validate(request)
        self.assertTrue(any(fragment in message for message in report.errors),
                        f'expected an error containing {fragment!r}, got {report.errors}')

    def test_readme_example_is_valid(self):
        report = self.validate(self.request)
        self.assertEqual(report.errors, [])

    def test_a_reference_mesh_in_the_folder_is_refused(self):
        (self.folder / 'delivery').mkdir()
        (self.folder / 'delivery' / 'sword.glb').write_bytes(b'glTF')
        self.assert_error(self.request, 'reference meshes stay outside the repository')

    def test_an_oversized_delivery_file_is_refused(self):
        (self.folder / 'delivery').mkdir()
        with open(self.folder / 'delivery' / 'source.blend', 'wb') as handle:
            handle.truncate((validator.MAX_DELIVERY_FILE_MB + 1) * validator.BYTES_PER_MB)
        self.assert_error(self.request, 'over 8 MB')

    def test_render_block_is_required(self):
        request = copy.deepcopy(self.request)
        del request['constraints']['render']
        self.assert_error(request, 'constraints: missing "render"')

    def test_render_block_must_equal_render_facts(self):
        request = copy.deepcopy(self.request)
        request['constraints']['render']['0-0']['models'][0]['meshes'][0]['worn'] = 'blended-additive'
        self.assert_error(request, 'constraints.render.0-0: differs from render-facts.json')

    def test_render_block_keys_are_the_targets(self):
        request = copy.deepcopy(self.request)
        request['constraints']['render']['12-0'] = request['constraints']['render']['0-0']
        self.assert_error(request, "constraints.render: keys ['0-0', '12-0']")

    def test_a_blended_wing_needs_the_blended_line(self):
        facts = json.loads((ROOT / 'assets-work' / 'Items' / 'render-facts.json').read_text(encoding='utf-8'))
        wing = facts['items']['12-0']
        request = copy.deepcopy(self.request)
        request['targets'][0]['key'] = '12-0'
        request['constraints']['render'] = {'12-0': validator.render_block(wing)}
        line = ('Blended meshes of Wing01.bmd (mesh 0): the game draws them additively - paint on black '
                '(black is fully transparent, brightness becomes glow); no opaque background, no baked dark outlines')
        self.assertEqual(wing['request']['must_keep'], [line])
        self.assert_error(request, 'missing the render lines of render-facts.json')
        request['constraints']['must_keep'].append(line)
        errors = [error for error in self.validate(request).errors if 'render' in error]
        self.assertEqual(errors, [])

    def test_a_finished_request_keeps_its_render_facts(self):
        request = copy.deepcopy(self.request)
        request['constraints']['render']['0-0']['effects'] = ['an older reading']
        request['status'] = 'withdrawn'
        request['status_history'].append({'status': 'withdrawn', 'at': '2026-09-24T09:00:00+02:00', 'by': 'owner'})
        request['decision'] = {'status': 'withdrawn', 'at': '2026-09-24T09:00:00+02:00', 'by': 'owner',
                               'reason': 'test', 'ledger_entry': None}
        errors = [error for error in self.validate(request).errors if 'render' in error]
        self.assertEqual(errors, [])

    def test_upscale_may_not_own_a_bmd(self):
        request = copy.deepcopy(self.request)
        request['constraints']['owned_files'].append('src/bin/Data/Item/Sword01.bmd')
        self.assert_error(request, 'may not replace BMDs')

    def test_redesign_may_own_the_bmd(self):
        request = copy.deepcopy(self.request)
        request['kind'] = 'redesign'
        request['constraints']['owned_files'].append('src/bin/Data/Item/Sword01.bmd')
        request['constraints']['must_keep'][-1] = validator.KIND_MUST_KEEP['redesign'][0]
        self.assertEqual(self.validate(request).errors, [])

    def test_must_keep_needs_the_kind_line(self):
        request = copy.deepcopy(self.request)
        request['kind'] = 'repaint'
        self.assert_error(request, 'constraints.must_keep')

    def test_catalog_facts_are_compared(self):
        request = copy.deepcopy(self.request)
        request['targets'][0]['tier'] = 7
        self.assert_error(request, '0-0.tier')

    def test_branch_must_follow_the_id(self):
        request = copy.deepcopy(self.request)
        request['handoff']['branch'] = 'codex/item-req-sword01-sharper-blade'
        self.assert_error(request, 'handoff.branch')

    def test_id_names_the_first_target(self):
        request = copy.deepcopy(self.request)
        request['targets'][0]['key'] = '0-1'
        self.assert_error(request, 'item part "0-0"')

    def test_world_field_is_rejected(self):
        request = copy.deepcopy(self.request)
        request['world'] = 1
        self.assert_error(request, 'world')

    def test_set_needs_armour_parts(self):
        request = copy.deepcopy(self.request)
        request['kind'] = 'set'
        request['change']['set_kind'] = 'upscale'
        self.assert_error(request, 'not an armour part')

    def retarget(self, entry):
        """The example request moved to another catalog item, owning all of its textures."""
        request = copy.deepcopy(self.request)
        slug = request['id'][len('2026-09-23-0-0-'):]
        request['id'] = f'2026-09-23-{entry["key"]}-{slug}'
        name = request['id'][len('2026-09-23-'):]
        request['handoff'].update(branch=f'codex/item-req-{name}', worktree=f'../MuMain-item-req-{name}',
                                  deliver_to=f'assets-work/Items/requests/{request["id"]}/delivery/')
        target = validator.catalog_target(entry)
        for model, known in zip(target['models'], entry['models']):
            model['current_sha256'] = known['original']['sha256']
        request['targets'] = [target]
        request['change'].pop('reference_images')
        request['evidence']['captures'] = []
        request['constraints']['owned_files'] = sorted({c for m in entry['models'] for c in m['textures'].values() if c})
        facts = json.loads((ROOT / 'assets-work/Items/render-facts.json').read_text(encoding='utf-8'))['items'][entry['key']]
        request['constraints']['render'] = {entry['key']: validator.render_block(facts)}
        request['constraints']['must_keep'] += facts['request']['must_keep']
        self.folder = self.folder.parent / request['id']
        (self.folder / 'captures').mkdir(parents=True)
        (self.folder / 'brief.md').write_text('# retargeted\n', encoding='utf-8')
        return request

    def test_shared_textures_must_be_listed_and_frozen(self):
        catalog = json.loads((ROOT / 'assets-work/Items/catalog.json').read_text(encoding='utf-8'))
        entry = next(e for key, e in sorted(catalog['items'].items())
                     if e['shared_with'] and all(c.startswith(validator.OWNABLE_FOLDERS) for c in e['shared_with']))
        request = self.retarget(entry)
        self.assert_error(request, 'must be frozen')
        self.assert_error(request, 'constraints.shared_textures')
        shared = {c: sorted({entry['key'], *users}, key=validator.consumer_sort_key)
                  for c, users in entry['shared_with'].items()}
        request['constraints']['shared_textures'] = shared
        request['constraints']['frozen_textures'] = sorted(shared)
        request['constraints']['owned_files'] = [f for f in request['constraints']['owned_files'] if f not in shared]
        report = self.validate(request)
        self.assertEqual(report.errors, [])

    def test_capture_size_must_match(self):
        request = copy.deepcopy(self.request)
        request['evidence']['captures'][0]['resolution'] = [800, 600]
        self.assert_error(request, 'resolution')

    def test_claimed_request_needs_an_assignment(self):
        request = copy.deepcopy(self.request)
        request['status'] = 'claimed'
        request['status_history'].append({'status': 'claimed', 'at': '2026-09-23T11:00:00+02:00', 'by': 'codex / items'})
        request['handoff'].update(claimed_by='codex / items', claimed_at='2026-09-23T11:00:00+02:00',
                                  start_commit='0' * 40)
        self.assert_error(request, 'assignments.json')

    def test_owner_decision_shape(self):
        (self.folder / 'owner-decision.json').write_text('{"verdict": "maybe", "notes": "", "date": "2026-09-24"}')
        self.assert_error(self.request, 'owner-decision.json.verdict')


if __name__ == '__main__':
    unittest.main()
