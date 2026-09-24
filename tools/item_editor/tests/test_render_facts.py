"""render_rules.py (the engine's mesh draw rules), render_code.py (reading the item render code) and
render_facts.py (render-facts.json) on small inputs and on the repository's own code and models."""

from pathlib import Path
import json
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import render_code  # noqa: E402
import render_facts  # noqa: E402
import render_rules as rules  # noqa: E402
from cpp_source import SymbolTable  # noqa: E402


def mesh(index, texture):
    return rules.mesh_facts(index, texture)


def call(flags, blend=-1, mesh_index=None, alpha=1.0, hidden=-1, texture=''):
    return rules.DrawCall(frozenset(flags), rules.Value(blend), mesh=mesh_index, alpha=rules.Value(alpha),
                          hidden=rules.Value(hidden), texture=texture)


TEXTURE = {rules.RENDER_TEXTURE}


class TextureFlags(unittest.TestCase):
    """TextureScriptParsing::parsingTScriptA."""

    def test_flags_after_the_first_underscore(self):
        self.assertTrue(rules.texture_flags('NEWW_R.jpg').bright)
        flags = rules.texture_flags('glow_RS.jpg')
        self.assertTrue(flags.bright and flags.stream)
        self.assertTrue(rules.texture_flags('x_H.tga').hidden)
        self.assertTrue(rules.texture_flags('x_N.jpg').none_blend)
        self.assertEqual(rules.texture_flags('x_DT.jpg').shadow, 'texture')

    def test_one_unknown_character_cancels_every_flag(self):
        self.assertEqual(rules.texture_flags('elfin_wing.jpg'), rules.NO_FLAGS)
        self.assertEqual(rules.texture_flags('glow_Rock.jpg'), rules.NO_FLAGS)
        self.assertEqual(rules.texture_flags('x_D.jpg'), rules.NO_FLAGS)
        # only the first "_" counts, so a second one with a flag behind it is not read
        self.assertEqual(rules.texture_flags('wing_3_R.jpg'), rules.NO_FLAGS)
        # lower case is not a flag
        self.assertEqual(rules.texture_flags('sword_r.jpg'), rules.NO_FLAGS)

    def test_at_most_four_flag_characters_are_read(self):
        self.assertTrue(rules.texture_flags('a_RHSNx.jpg').none_blend)

    def test_the_panda_ring_name(self):
        self.assertTrue(rules.texture_flags('mu_rgb_lights.jpg').bright)

    def test_file_kinds(self):
        self.assertTrue(mesh(0, 'hide.jpg').never_drawn)
        self.assertFalse(mesh(0, 'Hide.jpg').never_drawn)
        self.assertTrue(mesh(0, 'skin_arm.jpg').skin_or_hair)
        self.assertTrue(mesh(0, 'LevelArmor.jpg').skin_or_hair)
        self.assertTrue(mesh(0, 'hair_r.tga').skin_or_hair)
        self.assertTrue(mesh(0, 'angel_wing.tga').has_alpha)
        self.assertFalse(mesh(0, 'elfin_wing.jpg').has_alpha)


class MeshRules(unittest.TestCase):
    """BMD::RenderMesh and BMD::RenderBody."""

    def test_the_blend_mesh_is_additive(self):
        drawn = rules.classify(call(TEXTURE, blend=0), mesh(0, 'elfin_wing.jpg'), rules.Value(0), hide_skin=False)
        self.assertEqual(drawn.mode, rules.ADDITIVE)
        self.assertEqual(drawn.texture, 'own')

    def test_blend_mesh_minus_two_blends_every_mesh(self):
        passes = rules.passes_of(call(TEXTURE, blend=-2), [mesh(0, 'a.jpg'), mesh(1, 'b.tga')], hide_skin=False)
        self.assertEqual([drawn.mode for drawn in passes], [rules.ADDITIVE, rules.ADDITIVE])

    def test_textured_passes(self):
        cases = [(call(TEXTURE), 'a.jpg', rules.OPAQUE), (call(TEXTURE), 'a.tga', rules.ALPHA_TEST),
                 (call(TEXTURE, alpha=0.5), 'a.jpg', rules.ALPHA_BLEND),
                 (call(TEXTURE | {rules.RENDER_BRIGHT}), 'a.jpg', rules.ADDITIVE),
                 (call(TEXTURE | {rules.RENDER_DARK}), 'a.jpg', rules.SUBTRACT)]
        for draw_call, texture, mode in cases:
            with self.subTest(texture=texture, mode=mode):
                self.assertEqual(rules.classify(draw_call, mesh(0, texture), draw_call.blend, False).mode, mode)

    def test_render_body_honours_the_name_flags(self):
        meshes = [mesh(0, 'body.jpg'), mesh(1, 'glow_R.jpg'), mesh(2, 'cut_H.jpg')]
        passes = rules.passes_of(call(TEXTURE), meshes, hide_skin=False)
        self.assertEqual({drawn.mesh: drawn.mode for drawn in passes}, {0: rules.OPAQUE, 1: rules.ADDITIVE})

    def test_render_mesh_ignores_the_bright_flag(self):
        passes = rules.passes_of(call(TEXTURE, mesh_index=1), [mesh(0, 'a.jpg'), mesh(1, 'glow_R.jpg')], False)
        self.assertEqual(passes[0].mode, rules.OPAQUE)

    def test_hidden_mesh_argument_and_hidden_files(self):
        meshes = [mesh(0, 'a.jpg'), mesh(1, 'b.jpg'), mesh(2, 'hidden.jpg'), mesh(3, 'skin.jpg')]
        worn = rules.passes_of(call(TEXTURE, hidden=1), meshes, hide_skin=False)
        self.assertEqual([drawn.mesh for drawn in worn], [0, 3])
        carried = rules.passes_of(call(TEXTURE, hidden=1), meshes, hide_skin=True)
        self.assertEqual([drawn.mesh for drawn in carried], [0])

    def test_chrome_passes_skip_no_chrome_meshes_and_bind_an_engine_texture(self):
        chrome = call({'RENDER_CHROME', rules.RENDER_BRIGHT})
        self.assertIsNone(rules.classify(chrome, mesh(0, 'x_N.jpg'), chrome.blend, False))
        drawn = rules.classify(chrome, mesh(0, 'x.jpg'), chrome.blend, False)
        self.assertEqual((drawn.mode, drawn.texture), (rules.ADDITIVE, 'engine:BITMAP_CHROME'))

    def test_the_base_look_is_the_first_pass_with_the_own_texture(self):
        passes = rules.passes_of(call({'RENDER_CHROME', rules.RENDER_BRIGHT}), [mesh(0, 'wing.tga')], False)
        passes += rules.passes_of(call(TEXTURE), [mesh(0, 'wing.tga')], False)
        base, overlays = rules.base_and_overlays(passes)
        self.assertEqual(base[0].mode, rules.ALPHA_TEST)
        self.assertEqual([drawn.texture for drawn in overlays[0]], ['engine:BITMAP_CHROME'])


class SymbolsFixture(unittest.TestCase):
    def setUp(self):
        self.symbols = SymbolTable()
        self.symbols.add_header('enum { MODEL_ITEM = 1000, MODEL_A = MODEL_ITEM + 1, MODEL_B, MODEL_C, MODEL_D };')


class CodeReading(SymbolsFixture):
    def test_conditions(self):
        types, qualifiers, _ = render_code.condition_types('Type == MODEL_A || o->Type == MODEL_C', self.symbols)
        self.assertEqual((types, qualifiers), ([1001, 1003], []))
        types, _, _ = render_code.condition_types('Type >= MODEL_A && Type <= MODEL_C', self.symbols)
        self.assertEqual(types, [1001, 1002, 1003])
        types, _, _ = render_code.condition_types('MODEL_B <= Type && MODEL_D >= Type', self.symbols)
        self.assertEqual(types, [1002, 1003, 1004])
        types, qualifiers, _ = render_code.condition_types('Type == MODEL_B && (RenderType & RENDER_EXTRA)', self.symbols)
        self.assertEqual((types, qualifiers), ([1002], ['RenderType RENDER_EXTRA']))
        types, _, _ = render_code.condition_types('o->SubType == MODEL_A', self.symbols)
        self.assertEqual(types, [])

    def test_if_chain_and_switch(self):
        text = ('if (Type == MODEL_A) { b->RenderMesh(0, RENDER_TEXTURE, 1.f, 0); }\n'
                'else if (Type == MODEL_B)\n    b->RenderBody(RENDER_TEXTURE, 1.f, -1);\n'
                'else { return; }\n')
        arms, _ = render_code.parse_if_chain(text, 0, 1)
        self.assertEqual([arm[0] for arm in arms], ['Type == MODEL_A', 'Type == MODEL_B', ''])
        self.assertEqual(arms[1][1], 2)
        switch = 'switch (o->Type)\n{\ncase MODEL_A:\ncase MODEL_B:\n    o->BlendMesh = 0;\n    break;\ndefault: break;\n}'
        groups, _ = render_code.parse_switch(switch, 0, 1)
        self.assertEqual(groups[0][0], ['MODEL_A', 'MODEL_B'])
        branch = render_code.make_branch('f', '', groups[0][1], groups[0][2], groups[0][3], self.symbols, groups[0][0])
        self.assertEqual(branch.types, [1001, 1002])
        self.assertEqual([(a.target, a.value) for a in branch.assignments], [('BlendMesh', '0')])

    def test_nested_type_conditions_are_decided(self):
        body = ('{ b->RenderMesh(0, RENDER_TEXTURE, 1.f, -1);\n'
                '  if (Type == MODEL_A) b->RenderMesh(1, RENDER_TEXTURE, 1.f, -1);\n'
                '  else if (Type == MODEL_B) b->RenderMesh(2, RENDER_TEXTURE, 1.f, -1);\n'
                '  else b->RenderMesh(3, RENDER_TEXTURE, 1.f, -1);\n'
                '  if (g_isCharacterBuff(o, eBuff_X)) b->RenderMesh(4, RENDER_TEXTURE, 1.f, -1); }')
        calls, _, _, _ = render_code.body_facts(body, 1)
        guards = [render_code.type_guard(body, found.position, 1002, self.symbols) for found in calls]
        self.assertEqual(guards, [True, False, True, False, None])


class RepositoryFacts(unittest.TestCase):
    """render-facts.json of the repository: the facts checked in the client stay what they were."""

    @classmethod
    def setUpClass(cls):
        cls.builder = render_facts.Builder(ROOT)
        cls.catalog = render_facts.load_catalog(ROOT)

    def item(self, key):
        return self.builder.item(key, self.catalog['items'][key])

    def modes(self, key, model=0):
        return [(mesh['worn'], mesh['dropped'], mesh['inventory']) for mesh in self.item(key)['models'][model]['meshes']]

    def test_wings_of_elf_are_additive_everywhere(self):
        item = self.item('12-0')
        self.assertEqual(self.modes('12-0'), [(rules.ADDITIVE,) * 3])
        self.assertEqual(item['level_glow']['kind'], 'none')
        self.assertEqual(item['request']['must_keep'], [render_facts.ADDITIVE_LINE.format(model='Wing01.bmd', meshes='mesh 0')])

    def test_cut_out_and_opaque_wings(self):
        self.assertEqual(self.modes('12-1'), [(rules.ALPHA_TEST,) * 3])
        self.assertEqual(self.modes('12-41'), [(rules.OPAQUE,) * 3])
        self.assertEqual(self.modes('12-4'), [(rules.OPAQUE,) * 3, (rules.ADDITIVE,) * 3])

    def test_capes_are_cloth_when_worn(self):
        self.assertEqual(self.modes('13-30'), [(rules.HIDDEN, rules.ALPHA_TEST, rules.ALPHA_TEST)])
        kinds = [effect['kind'] for effect in self.item('13-30')['effects']]
        self.assertIn('cloth', kinds)

    def test_a_plain_sword(self):
        item = self.item('0-0')
        self.assertEqual(self.modes('0-0'), [(rules.OPAQUE,) * 3])
        self.assertEqual(item['request']['must_keep'], [])
        self.assertEqual(item['level_glow']['kind'], 'generic')
        self.assertIn('trail', [effect['kind'] for effect in item['effects']])

    def test_a_potion_is_not_worn(self):
        self.assertEqual(self.item('14-0')['models'][0]['meshes'][0]['worn'], None)

    def test_render_facts_json_is_current(self):
        self.assertEqual(render_facts.main(['--check']), 0,
                         'render-facts.json is out of date; run python3 tools/item_editor/render_facts.py')


if __name__ == '__main__':
    unittest.main()
