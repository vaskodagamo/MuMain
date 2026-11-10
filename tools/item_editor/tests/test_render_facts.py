"""render_rules.py (the engine's mesh draw rules), render_code.py (reading the item render code) and
render_facts.py (render-facts.json) on small inputs and on the repository's own data, code and models."""

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

    def test_a_bound_texture_decides_the_alpha_and_shows_hidden_skin(self):
        plain = rules.DrawCall(frozenset(TEXTURE), rules.Value(-1), mesh=0, texture='BITMAP_X', explicit_texture=True)
        drawn = rules.classify(plain, mesh(0, 'skin_arm.jpg'), plain.blend, hide_skin=True)
        self.assertEqual((drawn.mode, drawn.unsure), (rules.OPAQUE, True))  # the file of BITMAP_X is not known
        plain.texture_has_alpha = True
        drawn = rules.classify(plain, mesh(0, 'skin_arm.jpg'), plain.blend, hide_skin=True)
        self.assertEqual((drawn.mode, drawn.unsure), (rules.ALPHA_TEST, False))
        self.assertIsNone(rules.classify(call(TEXTURE, mesh_index=0), mesh(0, 'skin_arm.jpg'), rules.Value(-1), True))

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


class LookFunctions(SymbolsFixture):
    """Functions of named looks: tables of names, rows of ifs, loops over the meshes, switches."""

    def test_a_table_of_names(self):
        text = ('const RenderStyle RenderStyles[] = {\n    {"stormCrow", RenderStormCrow},\n'
                '    {"sealOfWealth", RenderSeal, BITMAP_SEAL, &SealShine},\n};\n')
        rows = render_code.name_table(text, 'const RenderStyle RenderStyles[] = {')
        self.assertEqual(rows, [('stormCrow', ['RenderStormCrow'], 2),
                                ('sealOfWealth', ['RenderSeal', 'BITMAP_SEAL', '&SealShine'], 3)])

    def test_ifs_in_a_row_are_branches(self):
        text = ('static Result Apply(BMD* b, int Type, int Level)\n{\n'
                '    if (Type == MODEL_A && Level == 9)\n    {\n        b->RenderBody(RENDER_TEXTURE, 1.f, -1);\n'
                '        return Result::Drawn;\n    }\n'
                '    if (Type == MODEL_B)\n    {\n        look.blendMesh = 1;\n        return Result::Applied;\n    }\n'
                '    return Result::None;\n}\n')
        branches = render_code.sequence_branches('Apply', text, 1, 'Apply', self.symbols)
        self.assertEqual([(branch.types, branch.qualifiers) for branch in branches], [([1001], ['Level == 9']), ([1002], [])])
        self.assertEqual([(a.target, a.value) for a in branches[1].assignments], [('BlendMesh', '1')])
        self.assertEqual(render_code.RETURN_VALUE.findall(branches[0].body), ['Result::Drawn'])

    def test_guards_decide_conditions_mesh_loops_and_type_switches(self):
        body = ('{ if (b->HideSkin) { b->RenderMesh(0, RENDER_TEXTURE, 1.f, -1);\n'
                '    for (int i = 1; i < b->NumMeshs; ++i) b->RenderMesh(i, RENDER_TEXTURE, 1.f, -1); }\n'
                '  else b->RenderBody(RENDER_TEXTURE, 1.f, -1);\n'
                '  switch (Type) { case MODEL_A: nTexture = 1; break; default: return false; }\n'
                '  for (int j = 0; j < 3; ++j) b->RenderMesh(j, RENDER_TEXTURE, 1.f, -1); }')
        calls, _, _, _ = render_code.body_facts(body, 1)

        def guards(hide_skin, value):
            decide = lambda condition: hide_skin if condition == 'b->HideSkin' else None  # noqa: E731
            found = [render_code.guard(body, call.position, decide, value, self.symbols) for call in calls]
            returned = render_code.guard(body, body.index('return false'), decide, value, self.symbols)
            return found, returned
        self.assertEqual(guards(True, 1001), ([True, True, False, None], False))
        self.assertEqual(guards(False, 1002), ([False, False, True, None], True))
        self.assertEqual(render_code.mesh_loop(body, calls[1].position), ('i', 1))
        self.assertIsNone(render_code.mesh_loop(body, calls[3].position))
        self.assertEqual(render_code.enclosing_condition(body, calls[0].position), 'b->HideSkin')


class RenderStyleLayers(unittest.TestCase):
    """A render style that is data: its layers as the calls DrawRenderStyle makes of them."""

    def state(self, blend=-1, hidden=-1):
        return render_facts.ObjectState(rules.Value(blend), rules.Value(hidden))

    def layer(self, layer, state=None):
        calls, left_out = render_facts.layer_calls(layer, state or self.state(), frozenset(TEXTURE))
        return calls, left_out

    def test_every_mesh_keeps_the_items_values(self):
        (found,), _ = self.layer({'kind': 'texture'}, self.state(blend=0, hidden=2))
        self.assertEqual((found.mesh, found.flags, found.blend.number, found.hidden.number, found.alpha.number),
                         (None, frozenset(TEXTURE), 0, 2, 1.0))
        (found,), _ = self.layer({'kind': 'texture', 'meshes': 'allEvenHidden', 'opacity': 'drawing'}, self.state(hidden=2))
        self.assertEqual(found.hidden.number, -1)

    def test_listed_meshes_glow_and_reflections(self):
        calls, _ = self.layer({'kind': 'reflection', 'blend': 'glow', 'meshes': [1, 2], 'glowMesh': 'none',
                               'texture': 'Effect/Chrome01.jpg', 'scrollV': {'shape': 'scroll', 'perSecond': 0.3}})
        self.assertEqual([found.mesh for found in calls], [1, 2])
        self.assertEqual(calls[0].flags, frozenset({'RENDER_CHROME', rules.RENDER_BRIGHT}))
        self.assertEqual((calls[0].blend.number, calls[0].texture, calls[0].texture_has_alpha, calls[0].uv_animated),
                         (-1, 'Effect/Chrome01.jpg', False, True))
        (found,), _ = self.layer({'kind': 'reflection', 'motion': 'swirl', 'glowMesh': 'all', 'opacity': 0.5})
        self.assertEqual((found.flags, found.blend.number, found.alpha.number), (frozenset({'RENDER_CHROME4'}), -2, 0.5))

    def test_all_but_and_the_drawings_own_blend(self):
        (found,), left_out = self.layer({'kind': 'texture', 'meshes': {'allBut': [2]}}, self.state(hidden=0))
        self.assertEqual((found.hidden.number, left_out), (2, ()))
        (found,), left_out = self.layer({'kind': 'texture', 'meshes': {'allBut': [1, 2]}})
        self.assertEqual((found.hidden.number, left_out), (-1, (1, 2)))
        (found,), _ = self.layer({'kind': 'reflection', 'blend': 'asDrawing'})
        self.assertEqual(found.flags, frozenset(TEXTURE))

    def test_a_changing_opacity_is_see_through_and_unknown_words_stop(self):
        (found,), _ = self.layer({'kind': 'texture', 'opacity': {'shape': 'wave', 'from': 0, 'to': 1, 'seconds': 2}})
        self.assertFalse(found.alpha.known)
        self.assertEqual(rules.classify(found, mesh(0, 'a.jpg'), found.blend, False).mode, rules.ALPHA_BLEND)
        for layer in ({'kind': 'paint'}, {'kind': 'reflection', 'motion': 'wobble'}, {'kind': 'texture', 'meshes': 'some'},
                      {'kind': 'texture', 'glowMesh': 'most'}):
            with self.subTest(layer=layer), self.assertRaises(render_facts.DataError):
                self.layer(layer)


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

    def test_first_wings_and_shields(self):
        # Wings of Spirits like the Wings of Elf; Heaven and Satan cut out; the plain shields opaque.
        self.assertEqual(self.modes('12-3'), [(rules.ADDITIVE,) * 3])
        self.assertEqual(self.modes('12-2'), [(rules.ALPHA_TEST,) * 3])
        for key in ('6-0', '6-1', '6-4'):
            with self.subTest(key=key):
                self.assertEqual(set(self.modes(key)), {(rules.OPAQUE,) * 3})
                self.assertEqual(self.item(key)['request']['must_keep'], [])

    def test_a_render_style_that_is_data(self):
        # Wings of Darkness: a glowing reflection with Effect/bab2.jpg under the cut-out wing.
        item = self.item('12-6')
        self.assertEqual(self.modes('12-6'), [(rules.ALPHA_TEST,) * 3])
        self.assertIn('Mesh 0 is drawn again, blended-additive, with Effect/bab2.jpg',
                      [effect['summary'] for effect in item['effects']])
        self.assertEqual(item['level_glow']['kind'], 'none')
        evidence = item['models'][0]['meshes'][0]['evidence']
        self.assertTrue(any('renderStyle "wingsOfDarkness"' in line for line in evidence), evidence)

    def test_a_render_style_in_code_follows_its_conditions(self):
        # The Violent Wind Helm draws only its helmet mesh where the skin is hidden (anMesh[Type - ...]).
        self.assertEqual(self.modes('7-39'), [(rules.OPAQUE, rules.HIDDEN, rules.HIDDEN),
                                              (rules.ALPHA_TEST, rules.HIDDEN, rules.HIDDEN), (rules.OPAQUE,) * 3])
        self.assertEqual(self.item('7-39')['status'], render_facts.STATUS_CODE)
        # The lucky Violent Wind Armor: in the inventory and on the ground mesh 0 has a texture of its own.
        armor = self.item('8-65')
        self.assertEqual(self.modes('8-65'), [(rules.OPAQUE, rules.ALPHA_TEST, rules.ALPHA_TEST)])
        self.assertEqual(armor['request']['must_keep'], [])
        self.assertIn('Mesh 0 is drawn with BITMAP_INVEN_ARMOR + 6 (Player/LuckyItem/65/InvenArmorMale40_luck.tga) '
                      'instead of its own texture (dropped, inventory)', armor['request']['effects'])

    def test_what_the_reading_cannot_decide_is_unverified(self):
        # Ammunition is drawn differently under the Infinity Arrow buff; a pet is drawn by its own code.
        for key in ('4-7', '13-0'):
            with self.subTest(key=key):
                item = self.item(key)
                self.assertEqual(item['status'], render_facts.STATUS_UNVERIFIED)
                self.assertTrue(item['unverified'])

    def test_every_render_style_and_item_effect_of_the_data_is_known(self):
        looks = self.builder.looks
        styles = {entry.style for entry in looks.entries.values() if entry.style}
        effects = {entry.effect for entry in looks.entries.values() if entry.effect}
        self.assertEqual(styles - set(looks.styles) - set(looks.style_code), set())
        self.assertEqual(effects - set(looks.effects), set())

    def test_render_facts_json_is_current(self):
        self.assertEqual(render_facts.main(['--check']), 0,
                         'render-facts.json is out of date; run python3 tools/item_editor/render_facts.py')


if __name__ == '__main__':
    unittest.main()
