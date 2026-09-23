"""Item table decoder: layouts, BuxConvert XOR, GenerateCheckSum2 and the shipped Item_Eng.bmd."""

from pathlib import Path
import re
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import item_table  # noqa: E402

SHIPPED_TABLE = ROOT / 'src/bin/Data/Local/Eng/item_eng.bmd'
FIELD_DEFS = ROOT / 'src/source/Data/GameData/ItemData/ItemFieldDefs.h'
SHIPPED_NAMED_ITEMS = 946  # the client log: "Loaded 946 items" (ITEM_EDITOR_PLAN.md, I0 status)


def sample_items():
    items = [item_table.empty_item() for _ in range(item_table.MAX_ITEM)]
    kris = items[0]
    kris.update(Name='Kris', Level=6, Width=1, Height=2, DamageMin=6, DamageMax=11, iZen=-5, TwoHand=False)
    kris['RequireClass'] = [1, 1, 1, 1, 1, 1, 1]
    helm = items[7 * item_table.MAX_ITEM_INDEX + 1]
    helm.update(Name='Dragon Helm', Level=57, Width=2, Height=2, RequireLevel=0, RequireStrength=120)
    helm['RequireClass'] = [0, 1, 0, 1, 0, 0, 0]
    helm['Resistance'] = [1, 2, 3, 4, 5, 6, 7, 8]
    return items


class ChecksumAndXor(unittest.TestCase):
    def test_bux_convert_is_symmetric_and_restarts_per_call(self):
        data = bytes(range(10))
        encoded = item_table.bux_convert(data)
        self.assertEqual(encoded[:3], bytes((0x00 ^ 0xFC, 0x01 ^ 0xCF, 0x02 ^ 0xAB)))
        self.assertEqual(item_table.bux_convert(encoded), data)

    def test_checksum_matches_a_hand_computed_value(self):
        # Two words, key 0: word 0 is XOR-ed (and mixed, offset 0 is a multiple of 16), word 1 added.
        data = struct.pack('<II', 0x11111111, 0x22222222)
        result = 0
        result ^= 0x11111111
        result ^= (0 + result) >> 1
        result = (result + 0x22222222) & 0xFFFFFFFF
        self.assertEqual(item_table.checksum2(data, 0), result)

    def test_checksum_uses_the_key(self):
        data = bytes(range(64))
        self.assertNotEqual(item_table.checksum2(data, 0xE2F1), item_table.checksum2(data, 0xE5F1))


class Layouts(unittest.TestCase):
    def test_record_sizes(self):
        self.assertEqual(item_table.RECORDS[item_table.FORMAT_LEGACY].size, 84)
        self.assertEqual(item_table.RECORDS[item_table.FORMAT_CURRENT].size, 104)

    def test_fields_follow_item_field_defs(self):
        text = FIELD_DEFS.read_text(encoding='utf-8')
        simple = text.split('#define ITEM_FIELDS_SIMPLE(X)')[1].split('#define')[0]
        declared = re.findall(r'^\s*X\((\w+),\s*(\w+),', simple, re.M)
        codes = {'Bool': '?', 'Byte': 'B', 'Word': 'H', 'Int': 'i'}
        self.assertEqual([(name, codes[kind]) for name, kind in declared], list(item_table.SIMPLE_FIELDS))

    def test_round_trip_in_both_layouts(self):
        items = sample_items()
        for layout in (item_table.FORMAT_LEGACY, item_table.FORMAT_CURRENT):
            with self.subTest(layout=layout):
                data = item_table.encode_item_table(items, layout)
                table = item_table.read_item_table(data)
                self.assertEqual(table['format'], layout)
                self.assertEqual(table['items'][0], items[0])
                self.assertEqual(table['items'][7 * 512 + 1], items[7 * 512 + 1])
                self.assertEqual(len(item_table.named_items(table)), 2)

    def test_corrupted_file_fails_the_checksum(self):
        data = bytearray(item_table.encode_item_table(sample_items()))
        data[100] ^= 0x01
        with self.assertRaisesRegex(item_table.ItemTableError, 'checksum'):
            item_table.read_item_table(bytes(data))

    def test_unknown_size_is_rejected(self):
        with self.assertRaisesRegex(item_table.ItemTableError, 'neither'):
            item_table.read_item_table(b'\0' * 1000)


class SideTables(unittest.TestCase):
    def test_set_types(self):
        size = item_table.SET_TYPE_RECORD.size
        plain = bytearray(size * item_table.MAX_ITEM)
        plain[size:2 * size] = bytes((0, 12, 3, 0))  # item 1: set numbers 0 and 12; others all zero
        body = b''.join(item_table.bux_convert(bytes(plain[i:i + size])) for i in range(0, len(plain), size))
        data = body + struct.pack('<I', item_table.checksum2(body, item_table.SET_TYPE_CHECKSUM_KEY))
        self.assertEqual(item_table.read_set_types(data), {1: [0, 12]})

    def test_add_options_xor_runs_across_records(self):
        plain = bytearray(item_table.ADD_OPTION_RECORD.size * item_table.MAX_ITEM)
        item_table.ADD_OPTION_RECORD.pack_into(plain, 22 * item_table.ADD_OPTION_RECORD.size, 2, 200, 1, 10, 22, 0)
        options = item_table.read_add_options(item_table.bux_convert(bytes(plain)))
        self.assertEqual(options, {22: {'option1': 2, 'value1': 200, 'option2': 1, 'value2': 10, 'type': 22, 'time': 0}})


@unittest.skipUnless(SHIPPED_TABLE.exists(), 'shipped item table not in this checkout')
class ShippedTable(unittest.TestCase):
    def test_shipped_table_is_legacy_with_valid_checksum(self):
        table = item_table.read_item_table(SHIPPED_TABLE.read_bytes())
        self.assertEqual(table['format'], item_table.FORMAT_LEGACY)
        self.assertEqual(len(item_table.named_items(table)), SHIPPED_NAMED_ITEMS)
        kris = table['items'][0]
        self.assertEqual((kris['Name'], kris['Level'], kris['Width'], kris['Height']), ('Kris', 6, 1, 2))
        self.assertEqual(table['items'][7 * 512]['Name'], 'Bronze Helm')


if __name__ == '__main__':
    unittest.main()
