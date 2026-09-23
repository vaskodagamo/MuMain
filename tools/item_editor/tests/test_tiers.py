"""Tier rules (tiers.py): families, percentile bands, drop order, badge tie-breakers, owner overrides."""

from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import tiers  # noqa: E402

NO_BADGES = {'socket': False, 'set': False, 'option380': False, 'excellent': False, 'max_item_level': None}


def item(index, level, family='swords', drop_level=None, drops=None, **badges):
    return {'group': 0, 'index': index, 'family': family, 'client_level': level,
            'openmu_drop_level': drop_level, 'drops_from_monsters': drops, 'badges': dict(NO_BADGES, **badges)}


class Families(unittest.TestCase):
    def test_every_key_has_one_family(self):
        seen = {}
        for family, keys in tiers.FAMILY_KEYS.items():
            for key in keys:
                self.assertNotIn(key, seen, f'{key} is in {seen.get(key)} and {family}')
                seen[key] = family

    def test_group_defaults_and_exceptions(self):
        self.assertEqual(tiers.family_of(0, 0), 'swords')
        self.assertEqual(tiers.family_of(4, 7), 'ammunition')
        self.assertEqual(tiers.family_of(4, 0), 'bows')
        self.assertEqual(tiers.family_of(8, 5), 'armours')
        self.assertEqual(tiers.family_of(12, 0), 'wings-1')
        self.assertEqual(tiers.family_of(12, 36), 'wings-3')
        self.assertEqual(tiers.family_of(13, 30), 'wings-2')
        self.assertEqual(tiers.family_of(14, 13), 'jewels')
        self.assertEqual(tiers.family_of(13, 0), 'pets')
        self.assertEqual(tiers.family_of(14, 1), 'potions')


class Quantiles(unittest.TestCase):
    def test_seven_distinct_scores_give_seven_tiers(self):
        items = {f'0-{i}': item(i, level=(i + 1) * 10) for i in range(7)}
        result = tiers.compute_tiers(items)
        self.assertEqual([result[f'0-{i}']['value'] for i in range(7)], [1, 2, 3, 4, 5, 6, 7])
        self.assertEqual([result[f'0-{i}']['family_rank'] for i in range(7)], [1, 2, 3, 4, 5, 6, 7])

    def test_equal_scores_share_a_tier(self):
        items = {'0-0': item(0, 5), '0-1': item(1, 5), '0-2': item(2, 5), '0-3': item(3, 90)}
        result = tiers.compute_tiers(items)
        # percentile rank with half of the ties: 1 + 7 * (0 + 3/2) // 4 and 1 + 7 * (3 + 1/2) // 4
        self.assertEqual({result[k]['value'] for k in ('0-0', '0-1', '0-2')}, {3})
        self.assertEqual(result['0-3']['value'], 7)

    def test_a_family_with_one_score_sits_in_the_middle(self):
        items = {f'12-{i}': dict(item(i, 150, family='wings-3'), group=12) for i in range(7)}
        self.assertEqual({t['value'] for t in tiers.compute_tiers(items).values()}, {4})

    def test_openmu_drop_level_wins_over_client_level(self):
        items = {'0-0': item(0, level=50, drop_level=3), '0-1': item(1, level=10)}
        result = tiers.compute_tiers(items)
        self.assertEqual(result['0-0']['score'], 3)
        self.assertEqual(result['0-0']['score_source'], tiers.SCORE_OPENMU)
        self.assertEqual(result['0-1']['score_source'], tiers.SCORE_CLIENT)
        self.assertEqual(result['0-0']['family_rank'], 1)

    def test_families_are_ranked_separately(self):
        items = {'0-0': item(0, 10), '1-0': dict(item(0, 100, family='axes'), group=1)}
        result = tiers.compute_tiers(items)
        self.assertEqual((result['0-0']['score'], result['1-0']['score']), (10, 100))
        # each is alone in its family, so both sit in the middle
        self.assertEqual((result['0-0']['value'], result['1-0']['value']), (4, 4))


class DropOrder(unittest.TestCase):
    def test_non_dropping_items_sort_after_dropping_ones(self):
        items = {
            '0-0': item(0, 0, drop_level=6, drops=True),
            '0-1': item(1, 0, drop_level=140, drops=True),
            '0-19': item(19, 0, drop_level=86, drops=False),
        }
        result = tiers.compute_tiers(items)
        self.assertEqual(result['0-19']['family_rank'], 3)
        # its tier is where its score falls among the dropping items: 1 + 7 * (1 + 0) // 2
        self.assertEqual(result['0-19']['value'], 4)

    def test_family_without_dropping_items_uses_all_scores(self):
        items = {'14-0': dict(item(0, 0, family='potions', drops=False), group=14),
                 '14-1': dict(item(1, 40, family='potions', drops=False), group=14)}
        result = tiers.compute_tiers(items)
        self.assertEqual((result['14-0']['value'], result['14-1']['value']), (2, 6))


class Badges(unittest.TestCase):
    def test_badges_break_ties_in_order(self):
        items = {
            '0-0': item(0, 50, max_item_level=15),
            '0-1': item(1, 50, excellent=True),
            '0-2': item(2, 50, option380=True),
            '0-3': item(3, 50, set=True),
            '0-4': item(4, 50, socket=True),
            '0-5': item(5, 50),
        }
        result = tiers.compute_tiers(items)
        ranked = sorted(items, key=lambda k: result[k]['family_rank'])
        self.assertEqual(ranked, ['0-5', '0-0', '0-1', '0-2', '0-3', '0-4'])


class Overrides(unittest.TestCase):
    def test_owner_override_wins_and_is_marked(self):
        items = {'0-0': item(0, 10), '0-1': item(1, 20)}
        result = tiers.compute_tiers(items, {'0-0': {'tier': 7, 'note': 'iconic'}})
        self.assertEqual(result['0-0']['value'], 7)
        self.assertEqual(result['0-0']['source'], tiers.SOURCE_OWNER)
        self.assertEqual(result['0-0']['computed_value'], 2)
        self.assertEqual(result['0-0']['family_rank'], 1)
        self.assertEqual(result['0-1']['source'], tiers.SOURCE_COMPUTED)

    def test_invalid_overrides_are_rejected(self):
        for bad in ({'0-0': {'tier': 8}}, {'0-0': {'tier': 3, 'colour': 'red'}}, {'sword': {'tier': 1}},
                    {'0-0': {'note': 'no tier'}}, [], {'0-0': {'tier': True}}):
            with self.subTest(bad=bad), self.assertRaises(tiers.TierError):
                tiers.validate_overrides(bad)
        with self.assertRaises(tiers.TierError):
            tiers.validate_overrides({'9-9': {'tier': 2}}, known_keys={'0-0'})

    def test_missing_file_means_no_overrides(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(tiers.load_overrides(Path(folder) / 'tiers.json'), {})
            path = Path(folder) / 'tiers.json'
            path.write_text('{"0-19": {"tier": 7, "note": "quest reward"}}', encoding='utf-8')
            self.assertEqual(tiers.load_overrides(path), {'0-19': {'tier': 7, 'note': 'quest reward'}})


if __name__ == '__main__':
    unittest.main()
