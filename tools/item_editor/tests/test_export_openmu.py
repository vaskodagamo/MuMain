"""OpenMU export: a backup-shaped fixture (System.Text.Json $id/$ref, PascalCase scalars, camelCase
collections, as Persistence/BasicModel serializes GameConfiguration) becomes openmu-items.json rows."""

import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import export_openmu_items as export  # noqa: E402

GAME_CONFIGURATION = {
    '$id': 'a0000000-0000-0000-0000-000000000001',
    'Name': 'Season 6',
    'characterClasses': [
        {'$id': 'c0000000-0000-0000-0000-000000000001', 'Number': 16, 'Name': 'Dark Knight'},
        {'$id': 'c0000000-0000-0000-0000-000000000002', 'Number': 48, 'Name': 'Magic Gladiator'},
    ],
    'itemOptions': [
        {'$id': 'o0000000-0000-0000-0000-000000000001', 'Name': 'Excellent Physical Attack Options'},
        {'$id': 'o0000000-0000-0000-0000-000000000002', 'Name': 'Luck'},
    ],
    'itemSetGroups': [
        {'$id': 's0000000-0000-0000-0000-000000000001', 'Name': 'Hyon', 'AlwaysApplies': False},
        {'$id': 's0000000-0000-0000-0000-000000000002', 'Name': 'Dragon Defense Rate Bonus', 'AlwaysApplies': True},
    ],
    'items': [
        {
            '$id': 'i0000000-0000-0000-0000-000000000001',
            'Number': 19, 'Group': 0, 'Name': 'Divine Sword of Archangel', 'DropLevel': 86,
            'MaximumDropLevel': None, 'DropsFromMonsters': False, 'MaximumItemLevel': 15, 'MaximumSockets': 0,
            'Width': 1, 'Height': 4, 'IsAmmunition': False,
            'qualifiedCharacters': [{'$ref': 'c0000000-0000-0000-0000-000000000002'},
                                    {'$ref': 'c0000000-0000-0000-0000-000000000001'}],
            'possibleItemSetGroups': [],
            'possibleItemOptions': [{'$ref': 'o0000000-0000-0000-0000-000000000002'},
                                    {'$ref': 'o0000000-0000-0000-0000-000000000001'}],
        },
        {
            '$id': 'i0000000-0000-0000-0000-000000000002',
            'Number': 1, 'Group': 7, 'Name': 'Dragon Helm', 'DropLevel': 57, 'MaximumDropLevel': None,
            'DropsFromMonsters': True, 'MaximumItemLevel': 15, 'MaximumSockets': 0, 'Width': 2, 'Height': 2,
            'IsAmmunition': False,
            'qualifiedCharacters': [{'$ref': 'c0000000-0000-0000-0000-000000000001'}],
            'possibleItemSetGroups': [{'$ref': 's0000000-0000-0000-0000-000000000002'},
                                      {'$ref': 's0000000-0000-0000-0000-000000000001'}],
            'possibleItemOptions': [{'$ref': 'o0000000-0000-0000-0000-000000000002'}],
        },
    ],
}


class BackupExport(unittest.TestCase):
    def check(self, items):
        self.assertEqual(sorted(items), ['0-19', '7-1'])
        sword = items['0-19']
        self.assertEqual(sword['drop_level'], 86)
        self.assertFalse(sword['drops_from_monsters'])
        self.assertTrue(sword['excellent'])
        self.assertEqual(sword['qualified_classes'], ['Dark Knight', 'Magic Gladiator'])
        self.assertEqual(sword['options'], ['Excellent Physical Attack Options', 'Luck'])
        helm = items['7-1']
        self.assertEqual(helm['ancient_sets'], ['Hyon'])
        self.assertEqual(helm['set_bonus_groups'], ['Dragon Defense Rate Bonus'])
        self.assertFalse(helm['excellent'])

    def test_extracted_game_configuration(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'GameConfiguration_a0000000.json'
            path.write_text(json.dumps(GAME_CONFIGURATION), encoding='utf-8')
            self.check(export.normalise(export.rows_from_backup(export.read_backup_documents(path))))

    def test_backup_zip(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'backup.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('AdminUser_1.json', '{}')
                archive.writestr('GameConfiguration_a0000000.json', json.dumps(GAME_CONFIGURATION))
            self.check(export.normalise(export.rows_from_backup(export.read_backup_documents(path))))

    def test_output_is_deterministic(self):
        rows = export.rows_from_backup([GAME_CONFIGURATION])
        first = export.render(export.document(export.normalise(rows), {'kind': 'backup', 'file': 'x.zip'}))
        second = export.render(export.document(export.normalise(list(reversed(rows))), {'kind': 'backup', 'file': 'x.zip'}))
        self.assertEqual(first, second)

    def test_zip_without_configuration_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'backup.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('Account_1.json', '{}')
            with self.assertRaises(export.ExportError):
                export.read_backup_documents(path)


if __name__ == '__main__':
    unittest.main()
