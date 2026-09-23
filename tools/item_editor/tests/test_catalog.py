"""assets-work/Items/catalog.json: consistent with item_models.json and the tier rules; current when
bmdconv is available ($MU_BMDCONV or a local tools build)."""

import json
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import build_item_catalog  # noqa: E402
import tiers  # noqa: E402

CATALOG = ROOT / 'assets-work' / 'Items' / 'catalog.json'


def bmdconv_available():
    try:
        return build_item_catalog.find_bmdconv(None) is not None
    except build_item_catalog.CatalogError:
        return False


@unittest.skipUnless(CATALOG.exists(), 'catalog.json not built')
class CommittedCatalog(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
        cls.models = json.loads((TOOLS / 'item_models.json').read_text(encoding='utf-8'))

    def test_every_item_with_a_model_is_listed(self):
        self.assertEqual(self.catalog['schema'], build_item_catalog.SCHEMA)
        self.assertEqual(set(self.catalog['items']), set(self.models['items']))

    def test_entries_have_the_documented_fields(self):
        fields = {'key', 'group', 'index', 'name', 'in_table', 'family', 'table', 'openmu', 'client', 'badges',
                  'models', 'tier', 'armour_set', 'shared_with', 'original', 'requests', 'client_review'}
        for key, entry in self.catalog['items'].items():
            self.assertEqual(set(entry), fields, key)
            self.assertEqual(entry['models'][0]['role'], 'item', key)
            self.assertIn(entry['tier']['value'], range(1, tiers.TIER_COUNT + 1), key)
            self.assertEqual(entry['family'], tiers.family_of(entry['group'], entry['index']), key)

    def test_family_ranks_are_a_permutation(self):
        ranks = {}
        for entry in self.catalog['items'].values():
            ranks.setdefault(entry['family'], []).append(entry['tier']['family_rank'])
        for family, values in ranks.items():
            self.assertEqual(sorted(values), list(range(1, len(values) + 1)), family)

    def test_swords_run_basic_to_rare(self):
        swords = sorted((e for e in self.catalog['items'].values() if e['family'] == 'swords'),
                        key=lambda e: e['tier']['family_rank'])
        dropping = [e for e in swords if e['tier']['drops_from_monsters'] is not False]
        scores = [e['tier']['score'] for e in dropping]
        self.assertEqual(scores, sorted(scores))
        self.assertEqual(dropping[0]['tier']['value'], 1)
        self.assertEqual(dropping[-1]['tier']['value'], tiers.TIER_COUNT)

    def test_shared_textures_are_symmetric(self):
        items = self.catalog['items']
        for key, entry in items.items():
            for container, users in entry['shared_with'].items():
                for user in users:
                    if user in items:
                        self.assertIn(key, items[user]['shared_with'].get(container, []), f'{key} {container} {user}')

    @unittest.skipUnless(bmdconv_available(), 'bmdconv not available (set MU_BMDCONV)')
    def test_catalog_is_current(self):
        self.assertEqual(build_item_catalog.main(['--check']), 0,
                         'catalog.json is out of date; run tools/item_editor/build_item_catalog.py')


    def test_check_ignores_request_status(self):
        with_request = json.loads(json.dumps(self.catalog))
        key = next(iter(with_request['items']))
        with_request['items'][key]['requests'] = [{'id': 'x', 'status': 'open', 'assigned_to': None}]
        with_request['requests'] = [{'id': 'x', 'status': 'open', 'items': [key]}]
        with_request.setdefault('counts', {})['requests'] = 1
        with_request.setdefault('generated_from', []).append(build_item_catalog.REQUEST_SOURCE)
        self.assertEqual(build_item_catalog.without_requests(with_request),
                         build_item_catalog.without_requests(self.catalog))
        changed = json.loads(json.dumps(with_request))
        changed['items'][key]['name'] = 'Changed'
        self.assertNotEqual(build_item_catalog.without_requests(changed),
                            build_item_catalog.without_requests(self.catalog))


if __name__ == '__main__':
    unittest.main()
