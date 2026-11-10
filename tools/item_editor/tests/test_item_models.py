"""item_models.json: the C++ interpreter, the checked-in file being current, and every file on disk."""

import json
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

from cpp_runner import Runner  # noqa: E402
from cpp_source import SymbolTable, Unresolved, resolve_conditionals, strip_comments  # noqa: E402
import gen_item_models  # noqa: E402

HEADER = '''
#define MAX_ITEM_INDEX 512
constexpr int MODEL_BASE = 1000;
enum { MODEL_SWORD = MODEL_BASE, MODEL_AXE = (MODEL_SWORD + 1 * MAX_ITEM_INDEX), MODEL_NEXT, MODEL_LAST };
'''


def run(body, extra_header=''):
    symbols = SymbolTable()
    symbols.add_header(HEADER + extra_header)
    runner = Runner(symbols, ('AccessModel', 'OpenTexture'))
    runner.run('Test', strip_comments(body), 1)
    return runner


def loads(runner):
    return [(call.args[0], *call.args[1:]) for call in runner.calls]


class Interpreter(unittest.TestCase):
    def test_symbols_follow_enum_rules(self):
        symbols = SymbolTable()
        symbols.add_header(HEADER)
        self.assertEqual(symbols.value('MODEL_AXE'), 1512)
        self.assertEqual(symbols.value('MODEL_LAST'), 1514)
        with self.assertRaises(Unresolved):
            symbols.value('MODEL_UNKNOWN')

    def test_loops_ifs_and_continue(self):
        runner = run(r'''
            for (int i = 0; i < 4; i++)   // skips 2
            {
                if (i == 2)
                    continue;
                gLoadData.AccessModel(MODEL_SWORD + i, L"Data\\Item\\", L"Sword", i + 1);
            }
            for (int i = 8; i <= 9; ++i) ::gLoadData.AccessModel(MODEL_AXE + i, L"Data\\Item\\", L"Axe", i + 1);
        ''')
        self.assertEqual([c.args[0] for c in runner.calls], [1000, 1001, 1003, 1520, 1521])
        self.assertEqual(runner.calls[-1].args[1:], ['Data\\Item\\', 'Axe', 10])

    def test_string_buffers_arrays_and_counters(self):
        runner = run(r'''
            const wchar_t names[][50] = { L"new_Helm", L"new_Armor" };
            const wchar_t* base = { L"Data\\Player\\LuckyItem\\" };
            wchar_t path[50] = { L"" };
            int nIndex = 62;
            for (int i = 0; i < 2; i++)
            {
                mu_swprintf(path, L"%ls%d\\", base, nIndex);
                if (nIndex != 63) gLoadData.AccessModel(MODEL_SWORD + nIndex, path, names[0], i + 1);
                gLoadData.AccessModel(MODEL_AXE + nIndex, path, names[1], i + 1);
                nIndex++;
            }
        ''')
        self.assertEqual(loads(runner), [
            (1062, 'Data\\Player\\LuckyItem\\62\\', 'new_Helm', 1),
            (1574, 'Data\\Player\\LuckyItem\\62\\', 'new_Armor', 1),
            (1575, 'Data\\Player\\LuckyItem\\63\\', 'new_Armor', 2),
        ])

    def test_unknown_statements_are_skipped_and_reported(self):
        runner = run(r'''
            Models[MODEL_SWORD].Meshs[1].NoneBlendMesh = true;
            LoadBitmap(L"Item\\x.jpg", BITMAP_X, GL_LINEAR);
            gLoadData.AccessModel(MODEL_UNKNOWN, L"Data\\Item\\", L"Nope");
            for (int i = 0; i < UNKNOWN_COUNT; i++) gLoadData.AccessModel(MODEL_SWORD + i, L"Data\\Item\\", L"Loop");
            gLoadData.OpenTexture(MODEL_SWORD, L"Item\\");
        ''')
        self.assertEqual(loads(runner), [(1000, 'Item\\')])
        self.assertEqual(len(runner.skipped), 2)

    def test_plain_calls_are_followed_and_string_arrays_are_read(self):
        symbols = SymbolTable()
        symbols.add_header(HEADER)
        bodies = {'OpenMore': r'gLoadData.AccessModel(MODEL_AXE, L"Data\\Item\\", L"More");'}
        runner = Runner(symbols, ('AccessModel', 'OpenTexture'),
                        resolve_function=lambda name: (name, bodies[name], 1) if name in bodies else None)
        runner.run('Test', strip_comments(r"""
            OpenMore();
            Unknown();
            const std::wstring folders[] = {L"Item\\", L"Player\\"};
            gLoadData.OpenTexture(MODEL_SWORD, folders);
        """), 1)
        self.assertEqual(loads(runner), [(1512, 'Data\\Item\\', 'More'), (1000, ['Item\\', 'Player\\'])])
        self.assertEqual(runner.calls[0].function, 'OpenMore')
        self.assertEqual(runner.skipped, [])

    def test_conditionals_follow_defined_macros(self):
        consulted = {}
        text = resolve_conditionals('#ifdef ON\nA();\n#else\nB();\n#endif\n#if defined(OFF) || ON\nC();\n#endif',
                                    {'ON'}, consulted)
        self.assertEqual(text.split(), ['A();', 'C();'])
        self.assertEqual(consulted, {'ON': True, 'OFF': False})

    def test_file_names_follow_access_model(self):
        generator = gen_item_models.Generator.__new__(gen_item_models.Generator)

        class Call:
            function = 'OpenItems'

            def __init__(self, *args):
                self.args = list(args)
        self.assertEqual(generator.model_file(Call(0, 'Data\\Item\\', 'Sword', 1))['referenced'], 'Data/Item/Sword01.bmd')
        self.assertEqual(generator.model_file(Call(0, 'Data\\Item\\', 'Sword', 32))['referenced'], 'Data/Item/Sword32.bmd')
        self.assertEqual(generator.model_file(Call(0, 'Data\\Item\\', 'Saint'))['referenced'], 'Data/Item/Saint.bmd')
        self.assertEqual(generator.model_file(Call(0, 'Data\\Item\\', 'Devil', 0))['referenced'], 'Data/Item/Devil00.bmd')


class CheckedInTable(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generated = gen_item_models.Generator().build()
        cls.checked_in = json.loads(gen_item_models.OUTPUT.read_text(encoding='utf-8'))

    def test_checked_in_json_is_current(self):
        self.assertEqual(gen_item_models.render(self.generated), gen_item_models.OUTPUT.read_text(encoding='utf-8'),
                         'item_models.json is out of date; run python3 tools/item_editor/gen_item_models.py')

    def test_every_referenced_file_exists_or_is_reported(self):
        items = self.checked_in['items']
        entries = [(key, item['file']) for key, item in items.items()]
        entries += [(key, extra['file']) for key, item in items.items() for extra in item['extra_models']]
        missing = sorted({(key, entry['path']) for key, entry in entries if not (ROOT / entry['path']).is_file()})
        reported = sorted((entry['item'], entry['path']) for entry in self.checked_in['missing_files'])
        self.assertEqual(missing, reported)
        for key, entry in entries:
            if entry['exists']:
                # the recorded spelling is the on-disk spelling (macOS matches case-insensitively)
                parent = (ROOT / entry['path']).parent
                self.assertIn(Path(entry['path']).name, {p.name for p in parent.iterdir()}, key)

    def test_loops_and_armour_are_expanded(self):
        items = self.checked_in['items']
        for index in range(17):
            self.assertEqual(items[f'0-{index}']['file']['path'], f'src/bin/Data/Item/Sword{index + 1:02d}.bmd')
        self.assertEqual(items['7-0']['file']['path'], 'src/bin/Data/Player/HelmMale01.bmd')
        self.assertEqual(items['8-10']['file']['path'], 'src/bin/Data/Player/ArmorElf01.bmd')
        self.assertTrue(items['7-62']['file']['path'].endswith('LuckyItem/62/new_Helm01.bmd'))
        roles = {(extra['role'], extra.get('class')) for extra in items['7-10']['extra_models']}
        self.assertIn(('class-variant', 'sum'), roles)
        self.assertIn('0-26', self.checked_in['socket_items'])
        self.assertEqual(self.checked_in['skipped_statements'], [])

    def extras(self, key, role):
        return [(extra['model'], Path(extra['file']['path']).name, extra.get('condition'))
                for extra in self.checked_in['items'][key]['extra_models'] if extra['role'] == role]

    def test_inventory_models_come_from_the_display_code(self):
        # The Rage Fighter armour's inventory model, and level variants drawn with event models.
        self.assertEqual(self.extras('8-59', 'inventory'),
                         [('MODEL_ARMORINVEN_60', 'Armor_inventory60.bmd', 'Type == ITEM_SACRED_ARMOR')])
        luck = self.extras('14-11', 'inventory')
        self.assertIn(('MODEL_EVENT + 10', 'MagicBox08.bmd', 'Type == ITEM_BOX_OF_LUCK && 8 <= Level && Level <= 12'), luck)
        self.assertIn(('MODEL_EVENT + 5', 'MagicBox03.bmd', 'Type == ITEM_BOX_OF_LUCK && Level == 2'), luck)
        self.assertIn(('MODEL_EVENT + 5', 'MagicBox03.bmd', 'Type == ITEM_BOX_OF_LUCK && 14 <= Level && Level <= 15'), luck)
        self.assertEqual(len(luck), 8)
        self.assertEqual(self.extras('14-47', 'inventory'),
                         [('MODEL_JACK_OLANTERN_BLESSINGS', 'hellowinscroll.bmd',
                           'Type >= ITEM_JACK_OLANTERN_BLESSINGS && Type <= ITEM_JACK_OLANTERN_CRY')])
        self.assertEqual(self.extras('14-46', 'inventory'), [])  # its own model

    def test_hand_models_and_body_parts_are_loaded(self):
        self.assertEqual(self.extras('0-32', 'left-hand'), [('MODEL_SWORD_32_LEFT', 'SwordL33.bmd', None)])
        self.assertEqual(self.extras('0-35', 'right-hand'), [('MODEL_SWORD_35_RIGHT', 'Sword36R.bmd', None)])
        others = {other['model']: other['file']['path'] for other in self.checked_in['other_models']}
        self.assertEqual(others['MODEL_BODY_HELM'], 'src/bin/Data/Player/HelmClass01.bmd')
        self.assertEqual(others['MODEL_BODY_ARMOR + 7'], 'src/bin/Data/Player/ArmorClass201.bmd')
        self.assertEqual(others['MODEL_BODY_BOOTS + 20'], 'src/bin/Data/Player/BootClass307.bmd')
        self.assertNotIn('MODEL_BODY_HELM + 10', others)  # the Magic Gladiator has no second form

    def test_inventory_statements_are_read_or_refused(self):
        statements = gen_item_models.inventory_statements(
            'if (level == 1)\n    return drawn(MODEL_A);\nreturn level == 0 ? levelMinusOne(MODEL_B) : nothing;\nbreak;')
        self.assertEqual(statements, [('level == 1', ' (MODEL_A)'), (None, ' level == 0 ? (MODEL_B) : nothing'), (None, None)])
        with self.assertRaises(Unresolved):
            gen_item_models.inventory_statements('model = MODEL_A;')
        self.assertEqual(gen_item_models.level_runs([2, 8, 9, 10, 14, 15]), [(2, 2), (8, 10), (14, 15)])


if __name__ == '__main__':
    unittest.main()
