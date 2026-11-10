#!/usr/bin/env python3
"""Generate assets-work/Items/render-facts.json: how the client draws every item, mesh by mesh.

For each item of assets-work/Items/catalog.json and each of its models, render-facts.json records
per mesh how the engine draws it when the item is worn, lying on the ground and in the inventory
(opaque, alpha-test, blended-additive, blended-alpha, blended-subtract or hidden) with the code
lines that decide it, and per item what the engine adds on top (level glow, excellent and ancient
passes, extra mesh passes, UV animation, pulsing, sprites, particles, joints, cloth, sounds).
Artists and the item request contract use it: a blended mesh has to be painted for blending
(assets-work/Items/README.md, "How the game draws items").

How the facts are found:
- per mesh, its texture name (flags _R/_H/_S/_N, .tga alpha, hid/skin/hair) and texture slot from
  the catalog and the BMD, and the meshes its model data draws without chrome (noneBlendMeshes);
- the item's model data (src/bin/Data/Items/Models, docs/item-data.md "Item models"): renderStyle
  (the look of the model), itemEffect (what runs before it is drawn) and glow (the level it glows
  like, the meshes that glow, whether it shines as an excellent item);
- a render style that is data (src/bin/Data/Effects/RenderStyles.json): its layers, each turned
  into the RenderMesh / RenderBody calls Render::Items::Looks::DrawRenderStyle makes of it (the
  light of a layer and the style's itemValuesAfter, which only reach the glow passes drawn after
  it, are not read);
- what is still code, read by render_code.py: ItemObjectAttribute (BlendMesh and HiddenMesh per
  type), the render styles and item effects that are functions with a name
  (Render/Items/ItemRenderStyles.cpp, ItemEffects.cpp), ApplyEventModelEffect (the event models of
  level variants), RenderPartObjectBody and RenderModelWithoutStyle (the drawing without a style),
  SpawnLinkedPartEffects (worn effects) and SpawnHeldWeaponGlow (the light of a held weapon);
- BMD::RenderMesh's rules (render_rules.py) applied to all of those calls;
- render_notes.py: facts read or checked by hand (cloth capes, the look in the client, corrections
  of the automatic reading), each with its code lines.
Code lines are written as file:line and found again from text anchors on every run, so a changed
engine file makes the run (and --check) fail instead of leaving stale line numbers. What the
reading cannot decide (a draw under a runtime condition, a texture or mesh it cannot name) is
listed in the item's "unverified" list and the item's status is "unverified".

Usage (from anywhere):
    python3 tools/item_editor/render_facts.py           # write render-facts.json
    python3 tools/item_editor/render_facts.py --check   # exit 1 when it is out of date
"""

import argparse
from collections import OrderedDict
import copy
from dataclasses import dataclass, field, replace
import json
from pathlib import Path
import re
import struct
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / 'world_editor'))

from cpp_runner import Runner  # noqa: E402
from cpp_source import (SymbolTable, Unresolved, defined_macros, evaluate, function_body, read_source,  # noqa: E402
                        resolve_conditionals, strip_comments, tokenize)
import render_code  # noqa: E402
from render_code import Anchor  # noqa: E402
import render_rules as rules  # noqa: E402
import render_notes  # noqa: E402
from materialize_variant import VariantError, plain_model  # noqa: E402

ROOT = HERE.parents[1]
SOURCE = 'src/source'
CATALOG = 'assets-work/Items/catalog.json'
OUTPUT = 'assets-work/Items/render-facts.json'
SCHEMA = 'mu-item-render-facts/1'
CATALOG_SCHEMA = 'mu-item-catalog/1'
GLOBAL_HEADERS = ('Core/Globals/_define.h', 'Core/Globals/_enum.h', 'Core/Globals/_TextureIndex.h')
OBJECT_FILE = 'Engine/Object/ZzzObject.cpp'
CHARACTER_FILE = 'Engine/Object/ZzzCharacter.cpp'
BMD_FILE = 'Render/Models/ZzzBMD.cpp'
BITMAP_FILE = 'Render/Sprites/GlobalBitmapLoad.cpp'
STYLES_FILE = 'Render/Items/ItemRenderStyles.cpp'
EFFECTS_FILE = 'Render/Items/ItemEffects.cpp'
LOOK_FILE = 'Render/Items/LookDrawing.cpp'
GLOW_FILE = 'Render/Items/ItemGlow.cpp'
LOOKUP_FILE = 'Render/Items/ItemModelLookup.h'
OPEN_DATA_FILE = 'Engine/Object/ZzzOpenData.cpp'
# The item data (docs/item-data.md): the model entries and the render styles that are data.
MODEL_DATA = 'src/bin/Data/Items/Models'
MODEL_FILES = 'Group*.json'
SHARED_MODELS = 'SharedModels.json'
RENDER_STYLES = 'src/bin/Data/Effects/RenderStyles.json'
RENDER_STYLE_KIND = 'renderStyle'
DATA_FORMAT_VERSION = 1
JSON_INDENT = 1
INCLUDE_GUARD = re.compile(r'^\s*#\s*ifndef\s+(\w+)\s*\n\s*#\s*define\s+\1\b', re.M)

WORN, DROPPED, INVENTORY = 'worn', 'dropped', 'inventory'
CONTEXTS = (WORN, DROPPED, INVENTORY)
# The contexts in which the engine uses a model of each catalog role.
ROLE_CONTEXTS = {'item': CONTEXTS, 'class-variant': (WORN,), 'left-hand': (WORN,), 'right-hand': (WORN,),
                 'inventory': (INVENTORY,)}

# How an item is worn (catalog table.item_slot = ITEM_ATTRIBUTE::m_byItemSlot): 0/1 hands (weapons,
# shields, bows), 2..6 the armour parts, 7 wings and capes, 8 pets and mounts, 9 pendants, 10 rings.
HAND_SLOTS = (0, 1)
LAST_HAND_GROUP = 6
ARMOUR_GROUPS = range(7, 12)
ARMOUR_SLOT_OFFSET = 5  # helm (group 7) is slot 2 ... boots (group 11) slot 6
WING_SLOT = 7
PET_SLOT = 8
JEWELLERY_SLOTS = (9, 10)
WORN_HAND, WORN_ARMOUR, WORN_BACK, WORN_PET, WORN_JEWELLERY = 'hand', 'armour', 'back', 'pet', 'jewellery'

# BMD mesh table (Render/Models/ZzzBMD.cpp, BMD::Open2).
MESH_RECORD = struct.Struct('<hhhhh')
NAME_BYTES = 32
MODEL_COUNTS_BYTES = 6
VERTEX_BYTES, NORMAL_BYTES, TEXCOORD_BYTES, TRIANGLE_BYTES = 16, 20, 8, 64

STATUS_VERIFIED, STATUS_CODE, STATUS_UNVERIFIED = 'verified-in-client', 'from-code', 'unverified'
ADDITIVE_LINE = ('Blended meshes of {model} ({meshes}): the game draws them additively - paint on black '
                 '(black is fully transparent, brightness becomes glow); no opaque background, no baked dark outlines')
ALPHA_BLEND_LINE = ('Alpha-blended meshes of {model} ({meshes}): the game draws them see-through at partial opacity '
                    '- keep them light and even; no opaque background, nothing that has to read as solid')
CUT_OUT_LINE = ('Cut-out meshes of {model} ({meshes}): the game discards texels at or below 25% alpha and blends '
                'the rest - keep the 32-bit .tga alpha as the silhouette; no opaque background, no matte fringe')
RULE_PREFIX = 'rule:'
# While an item is put together: the contexts that draw a mesh with another texture than its own
# (not written to render-facts.json; the item's "texture" effects say it).
REPLACED_TEXTURE = '_replaced_texture'
NO_EFFECTS = 'No item-specific effect in the scanned code: only the generic +level, excellent and ancient passes'


# --- sources ------------------------------------------------------------------------------------

class Source:
    """Preprocessed engine files, the symbol table and anchor lookup."""

    def __init__(self, root):
        self.root = Path(root)
        self.source = self.root / SOURCE
        header_texts = [strip_comments(path.read_text(encoding='utf-8', errors='replace'))
                        for path in sorted(self.source.rglob('*.h'))]
        self.macros = defined_macros(header_texts)
        self.consulted = {}
        self.symbols = SymbolTable()
        for header in GLOBAL_HEADERS:
            self.symbols.add_header(self.header_text(header))
        self.texts = {}
        self.model_item = self.symbols.value('MODEL_ITEM')
        self.items_per_group = self.symbols.value('MAX_ITEM_INDEX')

    def header_text(self, relative):
        """A header with its own include guard (#ifndef X / #define X) taken as not yet defined."""
        text = strip_comments((self.source / relative).read_text(encoding='utf-8', errors='replace'))
        guard = INCLUDE_GUARD.search(text)
        macros = self.macros - {guard.group(1)} if guard else self.macros
        return resolve_conditionals(text, macros, self.consulted)

    def text(self, relative):
        if relative not in self.texts:
            self.texts[relative] = read_source(self.source / relative, self.macros, self.consulted)
        return self.texts[relative]

    def where(self, anchor):
        line = render_code.anchor_line(self.text(anchor.file), anchor.needles)
        return f'{SOURCE}/{anchor.file}:{line} {anchor.what}'.rstrip()

    def value(self, expression):
        return evaluate(tokenize(expression), self.symbols.value)

    def type_of_key(self, key):
        group, index = (int(part) for part in key.split('-'))
        return self.model_item + group * self.items_per_group + index


# The generic rules and where they are (render-facts.json "rules").
RULE_ANCHORS = OrderedDict([
    ('flags', Anchor('Render/Sprites/TextureScript.cpp', ('parsingTScriptA',),
                     'texture name flags after the first "_" (at most 4, all valid): R bright, H hidden, S stream, N no chrome')),
    ('flags-applied', Anchor(BMD_FILE, ('void BMD::RenderScriptedMesh(', 'getBright() ? mesh : blendMesh'),
                             'RenderBody: an _R mesh is drawn as its own blend mesh, an _H mesh is skipped')),
    ('hid', Anchor('Data/GameData/ItemData/ItemTextureFiles.cpp', ('HiddenTexturePrefix = "hid"',),
                   'a texture name starting with "hid" is never loaded or drawn')),
    ('skin', Anchor('Data/DataHandler/LoadData.cpp', ('bool isSkin',),
                    'ski*/level*/hair* textures are skin or hair: not drawn where HideSkin is set (ground, inventory)')),
    ('tga', Anchor(BITMAP_FILE, ('bool CGlobalBitmap::LoadTgaFile', 'CreateBitmapStorage(uiBitmapIndex, filename, Width, Height, 4)'),
                   '.tga textures have an alpha channel')),
    ('jpg', Anchor(BITMAP_FILE, ('bool CGlobalBitmap::LoadJpegFile',
                                 'CreateBitmapStorage(uiBitmapIndex, filename, textureWidth, textureHeight, 3)'),
                   '.jpg textures are opaque')),
    ('none-blend', Anchor('Data/DataHandler/ItemData/ItemModelLoader.cpp', ('bmd.Meshs[mesh].NoneBlendMesh = true;',),
                          'the meshes of "noneBlendMeshes" in the model data get no chrome pass, like _N meshes')),
    ('mesh-blend', Anchor(BMD_FILE, ('MeshMaterial ResolveMaterial(', 'blendMeshIndex <= -2 || m->Texture == blendMeshIndex'),
                          'RenderMesh: the blend mesh (texture slot == BlendMesh; every mesh when <= -2) is additive, '
                          'unlit, times BlendMeshLight')),
    ('mesh-texture', Anchor(BMD_FILE, ('void BMD::DrawMesh(', 'else if (alpha < 0.99f || texture->Components == 4)'),
                            'RenderMesh RENDER_TEXTURE: with RENDER_BRIGHT additive, a 4-component texture alpha-tested, else opaque')),
    ('mesh-chrome', Anchor(BMD_FILE, ('MeshMaterial ResolveMaterial(', 'm->NoneBlendMesh'),
                           'RenderMesh chrome/metal pass: skips _N meshes, binds an engine texture')),
    ('additive', Anchor('Render/Textures/ZzzOpenglUtil.cpp', ('void EnableAlphaBlend()', 'BlendMode::Glow'),
                        'EnableAlphaBlend: GL_ONE, GL_ONE (black adds nothing), no depth write')),
    ('alpha-test', Anchor('Render/Textures/ZzzOpenglUtil.cpp', ('void EnableAlphaTest(', 'BlendMode::Alpha'),
                          'EnableAlphaTest: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, alpha test on')),
    ('alpha-threshold', Anchor('Render/Textures/ZzzOpenglUtil.cpp', ('SetAlphaFunc(GL_GREATER, 0.25f)',),
                               'the alpha test keeps texels with alpha > 0.25')),
    ('opaque', Anchor('Render/Textures/ZzzOpenglUtil.cpp', ('void DisableAlphaBlend()',), 'DisableAlphaBlend: opaque')),
    ('item-attributes', Anchor(OBJECT_FILE, ('void ItemObjectAttribute(OBJECT* o)', 'o->BlendMesh = -1;'),
                               'ItemObjectAttribute: BlendMesh -1 unless the type sets one')),
    ('inventory', Anchor('Engine/Object/ZzzInventory.cpp', ('void RenderObjectScreen(', 'ItemObjectAttribute(o);'),
                         'inventory: ItemObjectAttribute, then RenderPartObject with HideSkin')),
    ('dropped', Anchor(OBJECT_FILE, ('void CreateItemDrop(', 'ItemObjectAttribute(o);'),
                       'ground: ItemObjectAttribute; RenderDroppedItem draws with HideSkin')),
    ('hide-skin', Anchor(OBJECT_FILE, ('void RenderPartObject(const OBJECT* o', 'b->HideSkin = HideSkin;'),
                         'RenderPartObject: HideSkin as the drawing asks (inventory and ground: true)')),
    ('render-type', Anchor('Engine/Object/ZzzObject.h', ('void RenderPartObject(const OBJECT* o', 'int RenderType = RENDER_TEXTURE'),
                           'a normal drawing has the render type RENDER_TEXTURE (no doppelganger, no monster hands)')),
    ('worn-link', Anchor(CHARACTER_FILE, ('void SetUpItemObject(', 'ItemObjectAttribute(Object);'),
                         'hands, back and wings: RenderLinkObject uses ItemObjectAttribute')),
    ('worn-armour', Anchor(CHARACTER_FILE, ('void RenderCharacter(const CHARACTER* c',
                                            'RenderPartObject(&c->Object, Type, p, bodyLight, o->Alpha, p->Level'),
                           'armour parts: RenderPartObject on the character object (BlendMesh -1)')),
    ('item-effect', Anchor(OBJECT_FILE, ('Render::Items::ItemEffects::Result ApplyPartObjectEffect(',
                                         'Render::Items::ItemEffects::Apply(b, o, look, Type, Alpha, Level)'),
                           'the item effect of the model data runs before the model is drawn; one that draws the model '
                           'itself ends the drawing')),
    ('style', Anchor(OBJECT_FILE, ('void RenderPartObjectBody(BMD* b', 'Render::Items::Styles::Render(b, o, look, Type, Alpha, RenderType)'),
                     'RenderPartObjectBody: the render style of the model data draws the model')),
    ('style-data', Anchor(LOOK_FILE, ('void DrawRenderStyle(const LookDefinition& style', 'for (const Layer& layer : style.layers)'),
                          'a render style that is data draws its layers in their order (Data/Effects/RenderStyles.json)')),
    ('style-lookup', Anchor(STYLES_FILE, ('const RenderStyle* FindStyle(std::string_view name)', 'FindRenderStyle(name)'),
                            'a render style is looked up in the data first, then in the code')),
    ('body-default', Anchor(OBJECT_FILE, ('static void RenderModelWithoutStyle(', 'b->RenderBody(RenderType, Alpha, look.blendMesh'),
                            'without a render style: RenderBody(RenderType, BlendMesh)')),
    ('level-tiers', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'else if (Level < 3)'),
                           'the +level look')),
    ('glow-level', Anchor(GLOW_FILE, ('int GetLevel(int modelType, int level)', 'Get(modelType).levels'),
                          'glow.level of the model data: the level the item glows like')),
    ('glow-meshes', Anchor(GLOW_FILE, ('void RenderGlow(', 'RenderMeshes(b, o, look, Get(modelType).meshes'),
                           'glow.meshes / glow.hiddenMesh of the model data: the meshes of the +7 glow passes')),
    ('excellent', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'if ((ExcellentFlags & 63) > 0'), 'the excellent shine')),
    ('excellent-data', Anchor(GLOW_FILE, ('bool HasExcellentGlow(int modelType)', 'return Get(modelType).excellent;'),
                              'glow.excellent of the model data: false for items without the excellent shine')),
    ('ancient', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'else if (ancientDiscriminator > 0)'), 'the ancient shimmer')),
    ('render-level', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'if (renderLevel < 4)'),
                            'the effect option caps the +level look')),
])

# Which rule anchor explains a pass (render_rules.Pass.rule).
RULE_OF_PASS = {
    'blend mesh (texture slot == BlendMesh)': ('mesh-blend', 'additive'),
    'textured pass': ('mesh-texture',),
    'chrome/metal pass': ('mesh-chrome',),
    'colour pass (untextured)': ('mesh-texture',),
    'bright pass (untextured)': ('mesh-texture',),
}

MODE_TEXT = OrderedDict([
    (rules.OPAQUE, 'drawn with its texture, no transparency'),
    (rules.ALPHA_TEST, 'texels with alpha at or below 25% are cut out, the rest is alpha-blended with depth writes; '
                       'the .tga alpha is the silhouette'),
    (rules.ADDITIVE, 'added to the picture (GL_ONE, GL_ONE): black is fully transparent, brightness becomes glow; '
                     'unlit, scaled by BlendMeshLight, no depth writes'),
    (rules.ALPHA_BLEND, 'alpha-blended without the 25% cut-out and without depth writes'),
    (rules.SUBTRACT, 'subtracted from the picture (RENDER_DARK)'),
    (rules.HIDDEN, 'not drawn in this context'),
    (rules.UNKNOWN, 'the automatic reading could not decide; see the item\'s "unverified" list'),
])

LEVEL_TIERS = [
    '+0..+2: the model as painted',
    '+3..+4: body light tinted red, pulsing',
    '+5..+6: body light tinted blue, pulsing',
    '+7..+8: plus an additive chrome pass (BITMAP_CHROME)',
    '+9..+10: plus chrome and metal (BITMAP_SHINY) passes',
    '+11..+12: plus chrome2, metal and chrome passes',
    '+13..+15: plus chrome4, metal and chrome passes',
]
EXCELLENT_TEXT = ('Excellent: a second additive pass of the own texture in a pulsing purple/blue light '
                  '(bright texels glow, black stays black)')
NO_EXCELLENT_TEXT = 'No excellent shine (its model data turns it off, as for wings and capes)'
# An item effect that draws the model itself ends the drawing before the passes of RenderPartObjectEffect.
NO_GLOW_DRAWN_TEXT = 'No +level glow: its item effect draws the model itself, without the +level passes'
NO_EXCELLENT_DRAWN_TEXT = 'No excellent shine or ancient shimmer (its item effect draws the model itself)'
ANCIENT_TEXT = 'Ancient: a pulsing additive chrome pass'
ITEM_LEVELS = 16  # glow.level as a list: one level for each item level +0..+15


# --- the code that is read ------------------------------------------------------------------------

@dataclass(frozen=True)
class Region:
    name: str
    file: str
    kind: str        # 'chain', 'switch' or 'sequence'
    needles: tuple
    role: str        # attributes, event-effects, body, no-style, worn-effects, held-effects
    contexts: tuple = CONTEXTS


REGIONS = (
    Region('ItemObjectAttribute', OBJECT_FILE, 'switch', ('void ItemObjectAttribute(OBJECT* o)', 'switch (o->Type)'),
           'attributes'),
    Region('ApplyEventModelEffect', OBJECT_FILE, 'sequence', ('ApplyEventModelEffect(BMD* b',), 'event-effects'),
    Region('RenderPartObjectBody', OBJECT_FILE, 'chain',
           ('void RenderPartObjectBody(BMD* b', 'if (Type == MODEL_ANGEL && b->NumMeshs)'), 'body'),
    Region('RenderModelWithoutStyle', OBJECT_FILE, 'chain',
           ('static void RenderModelWithoutStyle(', 'if (o->Type == MODEL_15GRADE_ARMOR_OBJ_ARMLEFT'), 'no-style'),
    Region('SpawnLinkedPartEffects', CHARACTER_FILE, 'switch',
           ('void SpawnLinkedPartEffects(', 'Luminosity = (float)(Random::Rand() % 30 + 70) * 0.005f;', 'switch (Type)'),
           'worn-effects', (WORN,)),
    Region('SpawnHeldWeaponGlow', CHARACTER_FILE, 'switch',
           ('void SpawnHeldWeaponGlow(', 'bool Success = true;', 'switch (w->Type)'), 'held-effects', (WORN,)),
)
HELD_FUNCTION = 'SpawnHeldWeaponGlow'
HELD_LIGHT = Anchor(CHARACTER_FILE, ('void SpawnHeldWeaponGlow(', 'bool Success = true;', 'if (Success)'),
                    'SpawnHeldWeaponGlow: light sprite at the tip of a held weapon')

# The tables of the looks that are functions with a name.
STYLE_TABLE = 'const RenderStyle RenderStyles[] = {'
STYLE_SPAWN_TABLE = '} StyleSpawns[] = {'
EFFECT_TABLE = 'const ItemEffect ItemEffects[] = {'
SHINE = re.compile(r'constexpr\s+ShineBelowPlus3\s+(\w+)\s*\{(.*?)\}\s*;', re.S)
# RenderSetChrome (stormCrow, thunderHawk, hurricane): every mesh with its texture like RenderBody, and a
# chrome layer under or over it.
SET_CHROME = 'RenderSetChrome'
SET_CHROME_TEXTURE = Anchor(STYLES_FILE, ('void RenderSetChrome(', 'const auto texture = [&](int meshIndex)',
                                          'b->RenderScriptedMesh(meshIndex, RENDER_TEXTURE, look.alpha, look.blendMesh'),
                            'RenderSetChrome: every mesh with its texture, by RenderBody\'s rules')
SET_CHROME_HIDDEN = Anchor(STYLES_FILE, ('void RenderSetChrome(', 'if (b->IsMeshHidden(i, look.hiddenMesh))'),
                           'RenderSetChrome: without the hidden mesh')
SET_CHROME_LAYER = Anchor(STYLES_FILE, ('void RenderSetChrome(', 'TextureHasHoles(b, i)'),
                          'RenderSetChrome: the chrome layer (BITMAP_CHROME + 1)')
SET_CHROME_TEXT = ('A chrome layer (BITMAP_CHROME + 1) on every mesh: solid under the texture where the texture has '
                   'holes (it fills them), added over the texture otherwise')

# The conditions of the looks in code that a normal drawing of a player's item decides.
RENDER_TYPE_HAS = re.compile(r'RenderType & (RENDER_[A-Z0-9_]+)')
RENDER_TYPE_LACKS = re.compile(r'!\(RenderType & (RENDER_[A-Z0-9_]+)\)')
HIDE_SKIN_CONDITIONS = {'b->HideSkin': True, 'b->HideSkin == true': True, '!b->HideSkin': False,
                        'b->HideSkin == false': False}
# Only the helper NPCs have the flag (docs/item-data.md, "Some styles only apply to some drawings").
NOT_HELPER_NPC = 'o->m_bpcroom != TRUE'
HELPER_NPC_ONLY = Anchor(STYLES_FILE, ('bool RenderHelperNpcPlate(', 'if (o->m_bpcroom != TRUE)'),
                         'only on the helper NPCs')
OUTSIDE_RANGE = re.compile(rf'Type < ({render_code.OPERAND}) \|\| Type > ({render_code.OPERAND})')
NOT_IN_SET = re.compile(rf'!IsViolentWindToEternalWingSet\(Type, ({render_code.OPERAND})\)')
SET_COUNT = re.compile(r'constexpr\s+int\s+ViolentWindToEternalWingSetCount\s*=\s*(\d+)\s*;')
SET_RULE = Anchor(STYLES_FILE, ('bool IsViolentWindToEternalWingSet(int Type, int firstSet)',
                                'return Type >= firstSet && Type < firstSet + ViolentWindToEternalWingSetCount;'))
# The second models of the Rage Fighter gloves are drawn like their glove (GetItemTypeOfModel).
HAND_MODELS = Anchor(LOOKUP_FILE, ('inline int GetItemTypeOfModel(int modelType)',
                                   'if (modelType >= MODEL_SWORD_32_LEFT && modelType <= MODEL_SWORD_35_RIGHT)',
                                   'return ITEM_SACRED_GLOVE + (modelType - MODEL_SWORD_32_LEFT) / 2;'))
# The files of the engine's own textures (BITMAP_...): every LoadBitmap of the load code, run like
# gen_item_models.py runs the model loads.
LOAD_BITMAP = 'LoadBitmap'
LOAD_FUNCTION = re.compile(r'^(?:static\s+)?void\s+(\w+)\s*\(\s*\)\s*\{', re.M)
CODE_NONE_BLEND = re.compile(r'Models\[([^\]]+)\]\.Meshs\[(\d+)\]\.NoneBlendMesh\s*=\s*true\s*;')
RESULT_DRAWN = 'Result::Drawn'
STYLE_NOT_APPLIED = 'false'
# The int and bool variables of a look's function: what they hold decides its ifs, its meshes and its
# textures (the mesh of a lucky helm, the inventory texture of an armour).
LOCAL_DECLARATION = re.compile(r'\b(?:const\s+)?(?:bool|int)\s+(\w+)\s*(\[\d*\])?\s*=\s*([^;]+);')
LOCAL_ASSIGNMENT = re.compile(r'(?<![\w.>])(\w+)\s*=(?!=)\s*([^;]+);')
FOR_HEAD = re.compile(r'\bfor\s*\(\s*$')
IDENTIFIER = re.compile(r'[A-Za-z_]\w*')
CONSTANT_NAME = re.compile(r'[A-Z][A-Z0-9_]*')
BOOLEANS = {'true': 1, 'false': 0}

SPAWN_KINDS = {'CreateSprite': 'sprite', 'CreateSpriteFpsChecked': 'sprite', 'RenderBrightEffect': 'sprite',
               'SpawnBrightSprite': 'sprite', 'RenderLight': 'sprite', 'CreateParticle': 'particle',
               'CreateParticleFpsChecked': 'particle', 'CreateJoint': 'joint', 'CreateEffect': 'effect-model',
               'PlayBuffer': 'sound'}
# The argument that names what is spawned, where it is not the first one.
SPAWN_ARGUMENT = {'RenderBrightEffect': 1, 'SpawnBrightSprite': 2}
SPAWN_TEXT = {'sprite': 'Sprites', 'particle': 'Particles', 'joint': 'Joints (beams and trails)',
              'effect-model': 'Effect models', 'sound': 'Sounds'}

# Render styles that are data (docs/item-data.md, "Render styles and item effects").
LAYER_FIELDS = {'kind', 'motion', 'blend', 'meshes', 'texture', 'light', 'opacity', 'glowMesh', 'glowStrength',
                'scrollU', 'scrollV', 'scrollingMesh'}
LAYER_TEXTURE, LAYER_REFLECTION = 'texture', 'reflection'
MOTION_FLAGS = {'slowDrift': 'RENDER_CHROME', 'fastSweep': 'RENDER_CHROME2', 'followsLight': 'RENDER_CHROME3',
                'swirl': 'RENDER_CHROME4', 'strongSwirl': 'RENDER_CHROME5', 'stripes': 'RENDER_CHROME6',
                'verySlowDrift': 'RENDER_CHROME7', 'oilSheen': 'RENDER_OIL', 'stillShine': 'RENDER_METAL'}
DEFAULT_MOTION = 'slowDrift'
BLEND_NORMAL, BLEND_GLOW, BLEND_AS_DRAWING = 'normal', 'glow', 'asDrawing'
ALL_EVEN_HIDDEN = 'allEvenHidden'
ALL_BUT = 'allBut'
OPACITY_OF_DRAWING = 'drawing'
GLOW_NONE, GLOW_ALL = 'none', 'all'
NO_MESH, EVERY_MESH = -1, -2  # BlendMesh: none, every mesh (ResolveMaterial)
ALPHA_TEXTURE_SUFFIX = '.tga'


class DataError(render_code.CodeError):
    """The item data no longer has the form this tool reads."""


@dataclass
class ModelEntry:
    """One item's entry of Data/Items/Models."""
    key: str
    data: dict
    where: str            # file:line of the entry
    none_blend: tuple     # noneBlendMeshes, the shared model's for an entry that names one

    @property
    def style(self):
        return self.data.get('renderStyle', '')

    @property
    def effect(self):
        return self.data.get('itemEffect', '')

    @property
    def glow(self):
        return self.data.get('glow') or {}


@dataclass
class StyleCode:
    """A render style that is a function (a row of RenderStyles[])."""
    function: str
    line: int
    texture: str = ''     # the texture of a recipe that styles share
    glow: str = ''        # its own glow pass of items +7 and up
    shine: str = ''       # the shine below +3


class Looks:
    """The item model data, the render styles that are data, and the tables that name the render
    styles and item effects that are code."""

    def __init__(self, source):
        self.source = source
        self.entries = self.read_entries()
        self.styles = self.read_styles()
        text = source.text(STYLES_FILE)
        self.style_code = {}
        for name, values, line in render_code.name_table(text, STYLE_TABLE):
            style = StyleCode(values[0], line)
            for value in values[1:]:
                if value.startswith('&'):
                    style.shine = value[1:]
                elif value.startswith('BITMAP_'):
                    style.texture = value
                else:
                    style.glow = value
            self.style_code[name] = style
        self.style_spawns = {name: values[0] for name, values, _ in render_code.name_table(text, STYLE_SPAWN_TABLE)}
        self.shines = {name: unique(FLAG_NAME.findall(body)) for name, body in SHINE.findall(text)}
        effects = render_code.name_table(source.text(EFFECTS_FILE), EFFECT_TABLE)
        self.effects = {name: (values[0], values[1] if len(values) > 1 else '', line) for name, values, line in effects}
        count = SET_COUNT.search(text)
        if count is None:
            raise render_code.CodeError('ViolentWindToEternalWingSetCount not found')
        self.set_count = int(count.group(1))
        for anchor in (SET_RULE, HAND_MODELS):
            source.where(anchor)  # the rules below are theirs; a changed line stops the run
        self.hand_first, self.hand_last = source.value('MODEL_SWORD_32_LEFT'), source.value('MODEL_SWORD_35_RIGHT')
        self.first_glove = source.value('ITEM_SACRED_GLOVE')
        self.max_item = source.symbols.value('MAX_ITEM')
        self.code_none_blend = {}
        for model, mesh in CODE_NONE_BLEND.findall(source.text(OPEN_DATA_FILE)):
            self.code_none_blend.setdefault(source.value(model), []).append(int(mesh))
        self.bitmaps = self.read_bitmaps()

    def read_bitmaps(self):
        """Texture index -> the file LoadBitmap loads into it (below Data, with '/'); None for an
        index that two different files are loaded into."""
        text = self.source.text(OPEN_DATA_FILE)
        runner = Runner(self.source.symbols, (LOAD_BITMAP,))
        for name in LOAD_FUNCTION.findall(text):
            body, line = function_body(text, name)
            if body is not None and LOAD_BITMAP in body:
                runner.run(name, body, line)
        files = {}
        for call in runner.calls:
            if len(call.args) < 2 or not isinstance(call.args[0], str) or not isinstance(call.args[1], int):
                continue
            file = call.args[0].replace('\\', '/')
            index = call.args[1]
            files[index] = file if files.get(index, file) == file else None
        return files

    def bitmap_file(self, texture):
        """The file of an engine texture named like the code names it (BITMAP_INVEN_ARMOR + 6), or ''."""
        try:
            return self.bitmaps.get(self.source.value(texture)) or ''
        except Unresolved:
            return ''

    def read_entries(self):
        """Item type -> ModelEntry, from every Group*.json; the line is the entry's "number" line."""
        folder = self.source.root / MODEL_DATA
        shared = {entry['name']: entry for entry in read_data(folder / SHARED_MODELS)['models']}
        entries = {}
        for path in sorted(folder.glob(MODEL_FILES)):
            text = path.read_text(encoding='utf-8')
            document = json.loads(text)
            if document.get('formatVersion') != DATA_FORMAT_VERSION:
                raise DataError(f'{MODEL_DATA}/{path.name}: formatVersion is not {DATA_FORMAT_VERSION}')
            lines = number_lines(text)
            for entry in document['models']:
                files = entry
                if 'model' in entry:
                    if entry['model'] not in shared:
                        raise DataError(f'{MODEL_DATA}/{path.name}: unknown shared model {entry["model"]!r}')
                    files = shared[entry['model']]
                key = f'{document["group"]}-{entry["number"]}'
                item_type = document['group'] * self.source.items_per_group + entry['number']
                where = f'{MODEL_DATA}/{path.name}:{lines[entry["number"]]}'
                entries[item_type] = ModelEntry(key, entry, where, tuple(files.get('noneBlendMeshes', ())))
        return entries

    def read_styles(self):
        """Render style name -> (look, line of its name)."""
        path = self.source.root / RENDER_STYLES
        text = path.read_text(encoding='utf-8')
        document = json.loads(text)
        if document.get('formatVersion') != DATA_FORMAT_VERSION or document.get('kind') != RENDER_STYLE_KIND:
            raise DataError(f'{RENDER_STYLES}: not formatVersion {DATA_FORMAT_VERSION} of kind {RENDER_STYLE_KIND}')
        styles = {}
        for look in document['looks']:
            found = re.search(r'"name"\s*:\s*"' + re.escape(look['name']) + '"', text)
            line = text.count('\n', 0, found.start()) + 1 if found else 0
            for layer in look.get('layers', []):
                unknown = set(layer) - LAYER_FIELDS
                if unknown:
                    raise DataError(f'{RENDER_STYLES}:{line} {look["name"]}: layer fields this tool does not read: '
                                    f'{sorted(unknown)}')
            styles[look['name']] = (look, line)
        return styles

    def item_type_of(self, value):
        """GetItemTypeOfModel: the item a model slot is drawn like, or None."""
        if self.hand_first <= value <= self.hand_last:
            return self.first_glove + (value - self.hand_first) // 2
        item_type = value - self.source.model_item
        return item_type if 0 <= item_type < self.max_item else None

    def entry_of(self, value):
        item_type = self.item_type_of(value)
        return self.entries.get(item_type) if item_type is not None else None

    def none_blend_of(self, value):
        """The meshes of a model that get no chrome pass by its data or by the load code."""
        own_type = value - self.source.model_item
        entry = self.entries.get(own_type) if 0 <= own_type < self.max_item else None
        return set(entry.none_blend if entry else ()) | set(self.code_none_blend.get(value, ()))


def read_data(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


NUMBER_LINE = re.compile(r'"number"\s*:\s*(\d+)')


def number_lines(text):
    """Item number -> line of its "number" in a model data file."""
    lines = {}
    for number, line in enumerate(text.split('\n'), 1):
        found = NUMBER_LINE.search(line)
        if found:
            lines.setdefault(int(found.group(1)), number)
    return lines


def read_regions(source):
    """Region role -> its branches; every branch knows its file and contexts."""
    by_role = {}
    for region in REGIONS:
        text = source.text(region.file)
        if region.kind == 'sequence':
            branches = render_code.sequence_branches(region.name, text, 1, region.name, source.symbols)
        elif region.kind == 'chain':
            branches = render_code.chain_branches(region.name, text, 1, region.needles, source.symbols)
        else:
            branches = render_code.switch_branches(region.name, text, 1, region.needles, source.symbols)
        for branch in branches:
            branch.file = region.file
            branch.contexts = region.contexts
            by_role.setdefault(region.role, []).append(branch)
    return by_role


# --- BMD meshes ---------------------------------------------------------------------------------

def mesh_slots(path):
    """Mesh_t::Texture of every mesh in file order, or None when the file cannot be read."""
    try:
        model = plain_model(Path(path).read_bytes(), str(path))
        count = struct.unpack_from('<h', model, NAME_BYTES)[0]
        offset = NAME_BYTES + MODEL_COUNTS_BYTES
        slots = []
        for _ in range(count):
            vertices, normals, texcoords, triangles, slot = MESH_RECORD.unpack_from(model, offset)
            offset += MESH_RECORD.size + vertices * VERTEX_BYTES + normals * NORMAL_BYTES
            offset += texcoords * TEXCOORD_BYTES + triangles * TRIANGLE_BYTES + NAME_BYTES
            slots.append(slot)
        return slots
    except (OSError, VariantError, struct.error):
        return None


def model_meshes(root, model, none_blend=()):
    """The meshes of a catalog model; `none_blend`: the meshes its data draws without chrome."""
    structure = model.get('structure') or {}
    slots = mesh_slots(Path(root) / model['bmd']) or []
    missing = {name for name, status in (model.get('texture_status') or {}).items() if status == 'missing'}
    meshes = [rules.mesh_facts(index, texture, slots[index] if index < len(slots) else -1, texture in missing)
              for index, texture in enumerate(structure.get('mesh_textures', []))]
    return [replace(mesh, flags=replace(mesh.flags, none_blend=True)) if mesh.index in none_blend else mesh
            for mesh in meshes]


# --- draw-call evaluation -----------------------------------------------------------------------

NUMBER = re.compile(r'^-?\d+(\.\d*)?f?$')
NO_TEXTURE = -1  # RenderMesh / RenderBody: draw with the mesh's own texture
FLAG_NAME = re.compile(r'RENDER_[A-Z0-9_]+')
BITMAP = re.compile(r'^(BITMAP_[A-Z0-9_]+(?:\s*\+\s*\d+)?)$')
BLEND_FIELDS = ('o->BlendMesh', 'look.blendMesh')
HIDDEN_FIELDS = ('o->HiddenMesh', 'look.hiddenMesh')
ALPHA_FIELDS = ('o->Alpha', 'look.alpha', 'Alpha', 'alpha')
U_FIELDS = ('o->BlendMeshTexCoordU', 'look.blendMeshU')
V_FIELDS = ('o->BlendMeshTexCoordV', 'look.blendMeshV')
TEXTURE_PARAMETER = 'Texture'  # the texture a recipe gets from its row of RenderStyles[]


@dataclass
class ObjectState:
    """The OBJECT fields the body code reads, as ItemObjectAttribute (or the character) set them and
    as the draw's copy of them (DrawLook) has them."""
    blend: rules.Value
    hidden: rules.Value
    u: str = '0'
    v: str = '0'


def number_of(text):
    text = text.strip()
    return float(text.rstrip('f')) if NUMBER.match(text) else None


def int_value(text, state):
    text = text.strip()
    if text in BLEND_FIELDS:
        return state.blend
    if text in HIDDEN_FIELDS:
        return state.hidden
    number = number_of(text)
    return rules.Value(number) if number is not None else rules.Value(text=text)


def alpha_value(text):
    text = text.strip()
    if text in ALPHA_FIELDS:
        return rules.Value(1.0)
    number = number_of(text)
    return rules.Value(number) if number is not None else rules.Value(text=text)


def flags_of(text, caller_flags):
    names = set()
    for part in text.split('|'):
        part = part.strip()
        if part.strip('()') == 'RenderType':
            names |= caller_flags
        elif part.lstrip('(').startswith('RenderType'):
            continue  # (RenderType & RENDER_EXTRA): only for a few monsters
        else:
            names |= set(FLAG_NAME.findall(part))
    return frozenset(names)


def is_animated(text, state_text):
    text = text.strip()
    if text in U_FIELDS + V_FIELDS:
        return is_animated(state_text, '0')
    number = number_of(text)
    return number != 0 if number is not None else bool(text)


def draw_call(call, state, caller_flags):
    """A RenderMesh / RenderBody call as a render_rules.DrawCall, or None for other calls."""
    args = call.args
    if call.name == 'RenderMesh' and len(args) >= 4:
        mesh = number_of(args[0])
        result = rules.DrawCall(flags_of(args[1], caller_flags), int_value(args[3], state),
                                mesh=int(mesh) if mesh is not None else -1, alpha=alpha_value(args[2]), line=call.line)
        u_text, v_text = (args[5] if len(args) > 5 else '0'), (args[6] if len(args) > 6 else '0')
        texture = args[7] if len(args) > 7 else '-1'
        if mesh is None:
            result.unknown += (f'mesh {args[0]}',)
    elif call.name == 'RenderBody' and len(args) >= 3:
        hidden = int_value(args[6], state) if len(args) > 6 else rules.Value(-1)
        result = rules.DrawCall(flags_of(args[0], caller_flags), int_value(args[2], state), alpha=alpha_value(args[1]),
                                hidden=hidden, line=call.line)
        u_text, v_text = (args[4] if len(args) > 4 else '0'), (args[5] if len(args) > 5 else '0')
        texture = args[7] if len(args) > 7 else '-1'
    else:
        return None
    result.uv_animated = is_animated(u_text, state.u) or is_animated(v_text, state.v)
    texture = texture.strip()
    bitmap = BITMAP.match(texture)
    explicit = int_value(texture, state)  # Wing of Storm passes look.hiddenMesh (-1) here
    if bitmap:
        result.texture = ' '.join(bitmap.group(1).split())
        result.explicit_texture = True
    elif not (explicit.known and explicit.number == NO_TEXTURE):
        result.unknown += (f'texture {texture}',)
        result.explicit_texture = True
    if not result.blend.known:
        result.unknown += (f'BlendMesh {result.blend.text}',)
    return result


def apply_assignment(assignment, state):
    if assignment.target == 'BlendMesh':
        state.blend = int_value(assignment.value, state)
    elif assignment.target == 'HiddenMesh':
        state.hidden = int_value(assignment.value, state)
    elif assignment.target == 'BlendMeshTexCoordU':
        state.u = assignment.value
    elif assignment.target == 'BlendMeshTexCoordV':
        state.v = assignment.value


def statements(branch):
    """Calls and assignments of a branch in source order."""
    entries = [(call.position, 0, call) for call in branch.calls]
    entries += [(assignment.position, 1, assignment) for assignment in branch.assignments]
    return [entry for _, _, entry in sorted(entries, key=lambda item: (item[0], item[1]))]


# --- render styles that are data ------------------------------------------------------------------

def shape_text(value):
    """A number or a shape of a look as a short text."""
    if not isinstance(value, dict):
        return f'{value:g}'
    kind = value.get('shape')
    if kind in ('wave', 'bounce', 'ramp'):
        return f'{kind} from {value["from"]:g} to {value["to"]:g}, {value["seconds"]:g} s'
    if kind == 'scroll':
        return f'scrolls {value["perSecond"]:g} per second'
    if kind == 'frames':
        return f'{value["count"]} frames of {value["step"]:g}, {value["secondsEach"]:g} s each'
    if kind == 'random':
        return f'random from {value["from"]:g} to {value["to"]:g} in steps of {value["step"]:g}'
    raise DataError(f'{RENDER_STYLES}: unknown shape {kind!r}')


def changes(value):
    """Whether a number or shape of a look is not a constant 0."""
    return isinstance(value, dict) or value != 0


def layer_flags(layer, caller_flags):
    """LayerFlags (LookDrawing.cpp)."""
    kind, blend = layer.get('kind', LAYER_TEXTURE), layer.get('blend', BLEND_NORMAL)
    if kind not in (LAYER_TEXTURE, LAYER_REFLECTION) or blend not in (BLEND_NORMAL, BLEND_GLOW, BLEND_AS_DRAWING):
        raise DataError(f'{RENDER_STYLES}: unknown layer kind {kind!r} or blend {blend!r}')
    if blend == BLEND_AS_DRAWING:
        return frozenset(caller_flags)
    if kind == LAYER_TEXTURE:
        flags = {rules.RENDER_TEXTURE}
    else:
        motion = layer.get('motion', DEFAULT_MOTION)
        if motion not in MOTION_FLAGS:
            raise DataError(f'{RENDER_STYLES}: unknown motion {motion!r}')
        flags = {MOTION_FLAGS[motion]}
    if blend == BLEND_GLOW:
        flags.add(rules.RENDER_BRIGHT)
    return frozenset(flags)


def layer_calls(layer, state, caller_flags):
    """One layer of a render style as the calls DrawLayer / DrawMeshes make (LookDrawing.cpp), and
    the meshes a layer of 'every mesh but several' leaves out."""
    flags = layer_flags(layer, caller_flags)
    opacity = layer.get('opacity')
    if opacity is None or opacity == OPACITY_OF_DRAWING:
        alpha = rules.Value(1.0)  # the item's value and the drawing's: both 1 in a normal drawing
    elif isinstance(opacity, dict):
        alpha = rules.Value(text=f'opacity {shape_text(opacity)}')
    else:
        alpha = rules.Value(float(opacity))
    glow = layer.get('glowMesh')
    if glow is None:
        blend = state.blend
    elif glow == GLOW_NONE:
        blend = rules.Value(NO_MESH)
    elif glow == GLOW_ALL:
        blend = rules.Value(EVERY_MESH)
    elif isinstance(glow, int):
        blend = rules.Value(glow)
    else:
        raise DataError(f'{RENDER_STYLES}: unknown glowMesh {glow!r}')
    scroll_u, scroll_v = layer.get('scrollU'), layer.get('scrollV')
    animated = (is_animated(state.u, '0') if scroll_u is None else changes(scroll_u)) or \
               (is_animated(state.v, '0') if scroll_v is None else changes(scroll_v))
    texture = layer.get('texture', '')

    def call(mesh=None, hidden=rules.Value(NO_MESH)):
        made = rules.DrawCall(flags, blend, mesh=mesh, alpha=alpha, hidden=hidden, texture=texture,
                              uv_animated=animated, explicit_texture=bool(texture))
        if texture:
            made.texture_has_alpha = texture.lower().endswith(ALPHA_TEXTURE_SUFFIX)
        if not blend.known:
            made.unknown += (f'BlendMesh {blend.text}',)
        return made

    meshes = layer.get('meshes')
    if meshes is None:
        return [call(hidden=state.hidden)], ()
    if meshes == ALL_EVEN_HIDDEN:
        return [call()], ()
    if isinstance(meshes, list):
        return [call(mesh=mesh) for mesh in meshes], ()
    if isinstance(meshes, dict) and list(meshes) == [ALL_BUT] and meshes[ALL_BUT]:
        left_out = meshes[ALL_BUT]
        if len(left_out) == 1:
            return [call(hidden=rules.Value(left_out[0]))], ()
        return [call()], tuple(left_out)
    raise DataError(f'{RENDER_STYLES}: unknown meshes {meshes!r}')


# --- items --------------------------------------------------------------------------------------

@dataclass
class Draw:
    """The passes of one model in one context."""
    passes: list = field(default_factory=list)
    problems: list = field(default_factory=list)
    evidence: list = field(default_factory=list)
    uv: dict = field(default_factory=dict)   # mesh -> line of a call that animates its texture coordinates
    conditional: list = field(default_factory=list)  # draw calls under a runtime condition
    effects: list = field(default_factory=list)      # what the draw's helpers add (RenderSetChrome)
    unknown_meshes: list = field(default_factory=list)  # calls that draw a mesh the reading cannot name


@dataclass
class Scene:
    """What one drawing of one model fixes: its type, its meshes, and whether the skin is hidden."""
    value: int
    meshes: list
    hide_skin: bool
    caller_flags: frozenset = frozenset({rules.RENDER_TEXTURE})


@dataclass
class Local:
    """What a local variable of a look's function holds in one drawing: a number, a list of numbers
    or nothing known, and its text for a texture argument (BITMAP_INVEN_ARMOR + 6)."""
    number: object = None
    text: str = ''

    @property
    def known(self):
        return self.number is not None


# Branch conditions that make a branch run only for special monsters or modes, never for an item
# a player wears, drops or carries.
SPECIAL_QUALIFIERS = ('RENDER_EXTRA', 'm_bpcroom', 'KIND_PLAYER', 'SubType')


def is_special(branch):
    return any(word in qualifier for qualifier in branch.qualifiers for word in SPECIAL_QUALIFIERS)


def base_modes(passes):
    base, _ = rules.base_and_overlays(passes)
    return {mesh: (drawn.mode, drawn.texture) for mesh, drawn in base.items()}


def worn_kind(item):
    slot = (item.get('table') or {}).get('item_slot')
    group = item['group']
    if group <= LAST_HAND_GROUP and slot in HAND_SLOTS:
        return WORN_HAND
    if group in ARMOUR_GROUPS and slot == group - ARMOUR_SLOT_OFFSET:
        return WORN_ARMOUR
    if slot == WING_SLOT:
        return WORN_BACK
    if slot == PET_SLOT:
        return WORN_PET
    if slot in JEWELLERY_SLOTS:
        return WORN_JEWELLERY
    return None


class ItemReader:
    """Reads the render facts of one catalog item at a time."""

    def __init__(self, root, source, regions, looks):
        self.root = Path(root)
        self.source = source
        self.regions = regions
        self.looks = looks
        self.rules = {name: source.where(anchor) for name, anchor in RULE_ANCHORS.items()}
        # Evidence lists name a generic rule as "rule:<name>"; render-facts.json "rules" has its line.
        self.anchors = {name: f'{RULE_PREFIX}{name}' for name in RULE_ANCHORS}
        self.functions = {}
        self.set_chrome = [source.where(anchor) for anchor in (SET_CHROME_TEXTURE, SET_CHROME_HIDDEN, SET_CHROME_LAYER)]
        source.where(HELPER_NPC_ONLY)  # decide() takes this condition as true: no player is a helper NPC

    @staticmethod
    def where(branch, line=None):
        return f'{SOURCE}/{branch.file}:{line or branch.line}'

    def first(self, role, value):
        """The branch of a region that handles `value` in a normal draw: the first match, passing over
        branches that only run for special monsters or modes (SPECIAL_QUALIFIERS)."""
        for branch in self.regions.get(role, []):
            if value in branch.types and not is_special(branch):
                return branch
        return None

    def function(self, file, name):
        """The body of a named look's function as a branch."""
        if (file, name) not in self.functions:
            branch = render_code.function_branch(name, self.source.text(file), self.source.symbols)
            branch.file = file
            branch.contexts = CONTEXTS
            self.functions[(file, name)] = branch
        return self.functions[(file, name)]

    # -- local variables and conditions --

    def lookup(self, scene, local_values):
        """Names of an expression of the render code: the model type, a local variable, a constant."""
        def value_of(name):
            if name == 'Type':
                return scene.value
            if name in BOOLEANS:
                return BOOLEANS[name]
            if name in local_values:
                if not local_values[name].known:
                    raise Unresolved(f'{name} is not known')
                return local_values[name].number
            if not CONSTANT_NAME.fullmatch(name):
                raise Unresolved(f'{name} is not a constant')
            return self.source.symbols.value(name)
        return value_of

    def number(self, expression, scene, local_values):
        """An expression of the render code as a number, or None when it is not one that the model
        type, the local variables and the constants decide."""
        text = expression.replace('o->Type', 'Type')
        try:
            found = evaluate(tokenize(text), self.lookup(scene, local_values))
        except Unresolved:
            return None
        return found if isinstance(found, (int, list)) else None

    def local(self, expression, is_array, scene, local_values):
        text = ' '.join(expression.split())
        if is_array:
            parts = [self.number(part, scene, local_values) for part in text.strip('{}').split(',')]
            return Local(parts if all(isinstance(part, int) for part in parts) else None)
        shown = self.with_locals(text, local_values)
        number = self.number(text, scene, local_values)
        if BITMAP.match(shown):
            return Local(number, shown)
        return Local(number, str(number) if number is not None else '')

    @staticmethod
    def with_locals(text, local_values):
        """`text` with every local variable that has a text replaced by it."""
        def replaced(found):
            held = local_values.get(found.group(0))
            return held.text if held is not None and held.text else found.group(0)
        return ' '.join(IDENTIFIER.sub(replaced, text).split())

    def locals_at(self, branch, scene, position):
        """What the int and bool variables of the branch hold when the statement at `position` runs:
        their declarations and assignments before it, in order; one under a runtime condition
        leaves its variable unknown."""
        body = branch.body
        declared = {}
        events = []
        for found in LOCAL_DECLARATION.finditer(body):
            if FOR_HEAD.search(body[:found.start()]):
                continue  # a loop counter
            declared[found.group(1)] = found.span()
            events.append((found.start(), found.group(1), found.group(3), bool(found.group(2))))
        for found in LOCAL_ASSIGNMENT.finditer(body):
            name = found.group(1)
            if name in declared and not declared[name][0] <= found.start() < declared[name][1]:
                events.append((found.start(), name, found.group(2), False))
        local_values = {}
        for start, name, expression, is_array in sorted(events):
            if start >= position:
                break
            runs = render_code.guard(body, start, lambda condition: self.decide(condition, scene, local_values),
                                     scene.value, self.source.symbols)
            if runs:
                local_values[name] = self.local(expression, is_array, scene, local_values)
            elif runs is None:
                local_values[name] = Local()
        return local_values

    def decide(self, condition, scene, local_values=None):
        """Whether a condition of the render code holds in a normal drawing of a player's item:
        True, False, or None when it depends on something else (a buff, the light, the level)."""
        local_values = local_values or {}
        text = ' '.join(condition.split())
        if text in HIDE_SKIN_CONDITIONS:
            return scene.hide_skin == HIDE_SKIN_CONDITIONS[text]
        found = RENDER_TYPE_HAS.fullmatch(text)
        if found:
            return found.group(1) in scene.caller_flags
        found = RENDER_TYPE_LACKS.fullmatch(text)
        if found:
            return found.group(1) not in scene.caller_flags
        if text == NOT_HELPER_NPC:
            return True
        try:
            found = OUTSIDE_RANGE.fullmatch(text)
            if found:
                low, high = (render_code.operand_value(found.group(index), self.source.symbols) for index in (1, 2))
                return scene.value < low or scene.value > high
            found = NOT_IN_SET.fullmatch(text)
            if found:
                first = render_code.operand_value(found.group(1), self.source.symbols)
                return not first <= scene.value < first + self.looks.set_count
        except Unresolved:
            return None
        types, qualifiers, unresolved = render_code.condition_types(text, self.source.symbols)
        if types and not qualifiers and not unresolved:
            return scene.value in types
        # What is left: a condition on the local variables, the type and constants (nIndex > 0, bHide).
        found = self.number(text, scene, local_values)
        return bool(found) if isinstance(found, int) else None

    def guard(self, branch, position, scene):
        local_values = self.locals_at(branch, scene, position)
        return render_code.guard(branch.body, position, lambda condition: self.decide(condition, scene, local_values),
                                 scene.value, self.source.symbols)

    def result(self, branch, scene, draw, what):
        """What a function of a look returns for this drawing: the value of the first `return` that
        runs. A return under a runtime condition is listed as a problem and passed over."""
        for found in render_code.RETURN_VALUE.finditer(branch.body):
            runs = self.guard(branch, found.start(), scene)
            if runs:
                return ' '.join(found.group(1).split())
            if runs is None:
                line = render_code.line_of(branch.body, found.start(), branch.body_line)
                draw.problems.append(f'{self.where(branch, line)}: {what} depends on a runtime condition')
        return ''

    # -- one model in one context --

    def initial_state(self, value, context, group, draw):
        state = ObjectState(rules.Value(-1), rules.Value(-1))
        if context == WORN and group in ARMOUR_GROUPS:
            draw.evidence.append(self.anchors['worn-armour'])
            return state
        draw.evidence.append(self.anchors['item-attributes'])
        branch = self.first('attributes', value)
        if branch is None:
            return state
        for assignment in branch.assignments:
            apply_assignment(assignment, state)
            if assignment.target in ('BlendMesh', 'HiddenMesh'):
                draw.evidence.append(f'{self.where(branch, assignment.line)} ItemObjectAttribute: '
                                     f'{assignment.target} = {assignment.value}')
        return state

    def effect_branch(self, value):
        """What runs before the model is drawn (ApplyPartObjectEffect): the item effect its model data
        names, else the event model's own. (branch, evidence line) or (None, problem or '')."""
        entry = self.looks.entry_of(value)
        if entry is not None and entry.effect:
            if entry.effect not in self.looks.effects:
                return None, f'{entry.where}: the item effect "{entry.effect}" does not exist; drawn without it'
            branch = self.function(EFFECTS_FILE, self.looks.effects[entry.effect][0])
            return branch, f'{entry.where} itemEffect "{entry.effect}": {self.where(branch)} {branch.function}'
        branch = self.first('event-effects', value)
        if branch is None:
            return None, ''
        return branch, f'{self.where(branch)} {branch.function}: branch for this model'

    def draw(self, value, meshes, context, group):
        """The passes of one model in one context. Draw calls that only run under a runtime condition
        (a buff, the light colour, the level) count as passes when they leave every mesh's base look as
        the unconditional calls make it; otherwise the draw is marked for checking by hand."""
        draw = self.draw_calls(value, meshes, context, group, with_conditional=True)
        steady = self.draw_calls(value, meshes, context, group, with_conditional=False)
        if steady.conditional and base_modes(draw.passes) != base_modes(steady.passes):
            draw.problems += steady.conditional
        elif steady.conditional and not steady.passes:
            draw.problems += steady.conditional
        return draw

    def draw_calls(self, value, meshes, context, group, with_conditional):
        draw = Draw()
        state = self.initial_state(value, context, group, draw)
        scene = Scene(value, meshes, context in (DROPPED, INVENTORY))
        # ApplyPartObjectEffect: the item effect; one that returns Drawn has drawn the model.
        effect, line = self.effect_branch(value)
        if effect is None and line:
            draw.problems.append(line)
        if effect is not None:
            draw.evidence.append(line)
            if effect.qualifiers:
                draw.problems.append(f'{self.where(effect)}: the branch also needs "{effect.qualifiers[0]}"')
            self.run_branch(effect, scene, state, draw, with_conditional)
            if self.result(effect, scene, draw, 'whether the effect draws the model itself').endswith(RESULT_DRAWN):
                return draw
        # RenderPartObjectBody: a few models of its own, then the render style, then the plain drawing.
        body = self.first('body', value)
        if body is not None:
            draw.evidence.append(f'{self.where(body)} {body.function}: branch for this type')
            self.run_branch(body, scene, state, draw, with_conditional)
            return draw
        if self.run_style(scene, state, draw, with_conditional):
            return draw
        plain = self.first('no-style', value)
        if plain is not None:
            draw.evidence.append(f'{self.where(plain)} {plain.function}: branch for this type')
            self.run_branch(plain, scene, state, draw, with_conditional)
            return draw
        draw.passes += rules.passes_of(rules.DrawCall(scene.caller_flags, state.blend), meshes, scene.hide_skin)
        draw.evidence.append(self.anchors['body-default'])
        return draw

    def style_of(self, value):
        """(model entry, render style name) of a model, or (None, '')."""
        entry = self.looks.entry_of(value)
        return (entry, entry.style) if entry is not None and entry.style else (None, '')

    def style_applies(self, name, scene, draw):
        """The function of a render style in code when the style is for this drawing, else None."""
        style = self.looks.style_code[name]
        branch = self.function(STYLES_FILE, style.function)
        returned = self.result(branch, scene, draw, 'whether the render style applies')
        return None if returned == STYLE_NOT_APPLIED else branch

    def run_style(self, scene, state, draw, with_conditional):
        """Draws the model with the render style of its model data; False when it has none or the
        style is not for this drawing (the model is then drawn plainly)."""
        entry, name = self.style_of(scene.value)
        if not name:
            return False
        if name in self.looks.styles:  # the data first (FindStyle)
            look, line = self.looks.styles[name]
            where = f'{RENDER_STYLES}:{line} render style "{name}"'
            draw.evidence += [f'{entry.where} renderStyle "{name}"', where, self.anchors['style-data']]
            self.run_look(look, where, scene, state, draw)
            return True
        if name not in self.looks.style_code:
            draw.problems.append(f'{entry.where}: the render style "{name}" does not exist; drawn plainly')
            return False
        branch = self.style_applies(name, scene, draw)
        if branch is None:
            function = self.function(STYLES_FILE, self.looks.style_code[name].function)
            draw.evidence.append(f'{entry.where} renderStyle "{name}" is not for this drawing '
                                 f'({self.where(function)} {function.function}): drawn plainly')
            return False
        draw.evidence += [f'{entry.where} renderStyle "{name}"', f'{self.where(branch)} {branch.function}']
        self.run_branch(branch, scene, state, draw, with_conditional, self.looks.style_code[name].texture)
        return True

    def run_look(self, look, where, scene, state, draw):
        for number, layer in enumerate(look.get('layers', []), 1):
            note = f'{where}, layer {number}'
            calls, left_out = layer_calls(layer, state, scene.caller_flags)
            for call in calls:
                if call.unknown:
                    draw.problems.append(f'{note}: cannot read {", ".join(call.unknown)}')
                for drawn in rules.passes_of(call, scene.meshes, scene.hide_skin):
                    if drawn.mesh in left_out:
                        continue
                    drawn.note = note
                    draw.passes.append(drawn)
                    if call.uv_animated:
                        draw.uv.setdefault(drawn.mesh, note)

    def run_branch(self, branch, scene, state, draw, with_conditional, texture=''):
        for entry in statements(branch):
            runs = True if not entry.conditional else self.guard(branch, entry.position, scene)
            if runs is False:
                continue
            if isinstance(entry, render_code.Assignment):
                if runs is None and entry.target in ('BlendMesh', 'HiddenMesh'):
                    draw.problems.append(f'{self.where(branch, entry.line)}: {entry.target} is set under a runtime condition')
                if runs or with_conditional:
                    apply_assignment(entry, state)
                continue
            calls = self.with_texture_files(self.calls_of(entry, branch, scene, state, texture, draw))
            if not calls:
                continue
            if runs is None:
                draw.conditional.append(f'{self.where(branch, entry.line)}: a draw call under a runtime condition '
                                        f'changes the look')
                if not with_conditional:
                    continue
            for call in calls:
                if call.unknown:
                    draw.problems.append(f'{self.where(branch, entry.line)}: cannot read {", ".join(call.unknown)}')
                if call.mesh == NO_MESH:
                    draw.unknown_meshes.append(f'{self.where(branch, entry.line)} {branch.function}')
                for drawn in rules.passes_of(call, scene.meshes, scene.hide_skin):
                    drawn.note = f'{self.where(branch, entry.line)} {branch.function}'
                    drawn.conditional = runs is None
                    draw.passes.append(drawn)
                    if call.uv_animated:
                        draw.uv.setdefault(drawn.mesh, drawn.note)

    def calls_of(self, entry, branch, scene, state, texture, draw):
        """The RenderMesh / RenderBody calls one call of the render code makes: itself, one per mesh in
        a loop over the model's meshes, or what RenderSetChrome draws with each mesh's own texture."""
        if entry.name == SET_CHROME:
            where = f'{self.where(branch, entry.line)} {branch.function}'
            found = effect('mesh-pass', list(CONTEXTS), SET_CHROME_TEXT, [where] + self.set_chrome)
            if found not in draw.effects:
                draw.effects.append(found)
            return [rules.DrawCall(frozenset({rules.RENDER_TEXTURE}), state.blend, hidden=state.hidden, line=entry.line)]
        local_values = self.locals_at(branch, scene, entry.position)
        if texture or local_values:
            entry = copy.copy(entry)
            entry.args = [texture if texture and argument.strip() == TEXTURE_PARAMETER else argument
                          for argument in entry.args]
            if entry.name == 'RenderMesh' and entry.args and number_of(entry.args[0]) is None:
                mesh = self.number(entry.args[0], scene, local_values)  # nIndex, anMesh[Type - MODEL_...]
                if isinstance(mesh, int):
                    entry.args[0] = str(mesh)
            if entry.name in ('RenderMesh', 'RenderBody') and len(entry.args) > 7:
                entry.args[7] = self.with_locals(entry.args[7], local_values)
        loop = render_code.mesh_loop(branch.body, entry.position)
        if loop and entry.name == 'RenderMesh' and entry.args and entry.args[0].strip() == loop[0]:
            calls = []
            for mesh in range(loop[1], len(scene.meshes)):
                one = copy.copy(entry)
                one.args = [str(mesh)] + entry.args[1:]
                calls.append(draw_call(one, state, scene.caller_flags))
            return calls
        call = draw_call(entry, state, scene.caller_flags)
        return [call] if call is not None else []

    def with_texture_files(self, calls):
        """Says of every call that binds an engine texture whether its file has an alpha channel."""
        for call in calls:
            file = self.looks.bitmap_file(call.texture) if call.texture and call.texture_has_alpha is None else ''
            if file:
                call.texture_has_alpha = file.lower().endswith(ALPHA_TEXTURE_SUFFIX)
        return calls

    # -- level, excellent and per-type effects --

    def drawn_by_effect(self, value):
        """The line of the item effect when it draws the model itself (Result::Drawn): the drawing
        then ends before the +level, excellent and ancient passes. '' otherwise."""
        branch, line = self.effect_branch(value)
        if branch is None or branch.qualifiers:
            return ''
        returned = self.result(branch, Scene(value, [], False), Draw(), '')
        return line if returned.endswith(RESULT_DRAWN) else ''

    def level_glow(self, value):
        forced, evidence_lines = None, []
        drawn = self.drawn_by_effect(value)
        if drawn:
            return {'kind': 'none', 'level': None, 'summary': NO_GLOW_DRAWN_TEXT,
                    'evidence': [drawn, self.anchors['item-effect']]}
        entry = self.looks.entry_of(value)
        levels = entry.glow.get('level') if entry is not None else None
        if isinstance(levels, list):
            if len(levels) != ITEM_LEVELS:
                raise DataError(f'{entry.where}: glow.level is not a level or a list of {ITEM_LEVELS}')
            levels = levels[0] if len(set(levels)) == 1 else levels
        if levels is not None:
            shown = levels if isinstance(levels, int) else ', '.join(str(level) for level in levels)
            evidence_lines += [f'{entry.where} glow.level: {shown}', self.anchors['glow-level']]
            forced = levels if isinstance(levels, int) else f'levels +0..+{ITEM_LEVELS - 1} glow like {shown}'
        branch, _ = self.effect_branch(value)
        for operator, expression, line, position in branch.level if branch is not None else ():
            condition = render_code.enclosing_condition(branch.body, position)
            when = f' when {condition}' if condition else ''
            evidence_lines.append(f'{self.where(branch, line)} {branch.function}: Level {operator} {expression}{when}'.rstrip())
            number = number_of(expression) if operator == '=' and not condition else None
            forced = int(number) if number is not None else f'Level {operator} {expression}{when}'.strip()
        evidence_lines.append(self.anchors['level-tiers'])
        if forced == 0:
            summary, kind = 'No +level glow: the engine draws it as +0 whatever its level', 'none'
        elif isinstance(forced, int):
            summary, kind = f'Always drawn with the +{forced} look whatever its level', 'forced'
        elif forced:
            summary, kind = f'The +level look uses a changed level ({forced})', 'changed'
        else:
            summary, kind = ('+level look: +3..+6 tinted light, +7 and up extra chrome/metal passes '
                             'over the whole model'), 'generic'
        restricted = self.glow_meshes(entry)
        if restricted:
            summary += f'; {restricted}'
            evidence_lines += [f'{entry.where} glow', self.anchors['glow-meshes']]
        return {'kind': kind, 'level': forced if isinstance(forced, int) else None, 'summary': summary,
                'evidence': evidence_lines}

    @staticmethod
    def glow_meshes(entry):
        """The meshes of the +7 glow passes when the model data narrows them."""
        glow = entry.glow if entry is not None else {}
        if glow.get('meshes'):
            return f'the chrome/metal level passes draw only mesh {", ".join(map(str, sorted(glow["meshes"])))}'
        if 'hiddenMesh' in glow:
            return f'the chrome/metal level passes skip mesh {glow["hiddenMesh"]}'
        return ''

    def excellent(self, value):
        drawn = self.drawn_by_effect(value)
        if drawn:
            return {'summary': NO_EXCELLENT_DRAWN_TEXT, 'evidence': [drawn, self.anchors['item-effect']]}
        entry = self.looks.entry_of(value)
        glow = entry.glow if entry is not None else {}
        if glow.get('excellent', True) is False:
            return {'summary': NO_EXCELLENT_TEXT,
                    'evidence': [f'{entry.where} glow.excellent: false', self.anchors['excellent-data']]}
        text, evidence_lines = EXCELLENT_TEXT, [self.anchors['excellent']]
        if 'excellentMesh' in glow:
            text += f'; only on mesh {glow["excellentMesh"]}'
            evidence_lines.insert(0, f'{entry.where} glow.excellentMesh: {glow["excellentMesh"]}')
        return {'summary': text, 'evidence': evidence_lines}

    def branch_effects(self, branch, contexts=None):
        contexts = list(branch.contexts if contexts is None else contexts)
        # Sprites, particles and joints live in the world: an inventory draw does not show them.
        world_contexts = [context for context in contexts if context != INVENTORY]
        found = []
        spawned = OrderedDict()
        for call in branch.calls:
            kind = SPAWN_KINDS.get(call.name)
            if kind is None:
                continue
            index = SPAWN_ARGUMENT.get(call.name, 0)
            what = ' '.join(call.args[index].split()) if len(call.args) > index else '?'
            spawned.setdefault(kind, OrderedDict()).setdefault(what, []).append(call.line)
        for kind, things in spawned.items():
            lines = sorted({line for found_lines in things.values() for line in found_lines})
            found.append(effect(kind, world_contexts, f'{SPAWN_TEXT[kind]}: {", ".join(things)}',
                                [f'{self.where(branch, line)} {branch.function}' for line in lines]))
        for assignment in branch.assignments:
            where = [f'{self.where(branch, assignment.line)} {branch.function}']
            if assignment.target == 'BlendMeshLight' and number_of(assignment.value) is None:
                word = 'flickers' if 'rand' in assignment.value.lower() else 'pulses'
                found.append(effect('pulse', contexts, f'The blend mesh brightness {word} ({assignment.value})', where))
            elif assignment.target in ('BlendMeshTexCoordU', 'BlendMeshTexCoordV') and number_of(assignment.value) is None:
                word = 'jumps to a random offset' if 'rand' in assignment.value.lower() else 'scrolls'
                found.append(effect('uv', contexts, f'The blend/stream mesh texture {word} '
                                    f'along {assignment.target[-1]} ({assignment.value})', where))
            elif assignment.target == 'StreamMesh' and number_of(assignment.value) not in (None, -1.0):
                found.append(effect('uv', contexts, f'Mesh {assignment.value} is the stream mesh (unlit, UV offset)', where))
        if branch.function == HELD_FUNCTION and 'Success = false' not in branch.body:
            found.append(effect('sprite', contexts, 'A light sprite at the blade tip while held, coloured by +level',
                                [f'{self.where(branch)} {branch.function}', self.source.where(HELD_LIGHT)]))
        return found

    def look_effects(self, look, where):
        """What a render style that is data animates: a glow strength that changes, a scrolling mesh."""
        found = []
        for number, layer in enumerate(look.get('layers', []), 1):
            note = [f'{where}, layer {number}']
            meshes = layer.get('meshes')
            which = f'Mesh {", ".join(map(str, unique(meshes)))}' if isinstance(meshes, list) else 'The model'
            strength = layer.get('glowStrength')
            if isinstance(strength, dict):
                found.append(effect('pulse', list(CONTEXTS), f'{which}: the glow strength changes ({shape_text(strength)})',
                                    note))
            if 'scrollingMesh' in layer:
                found.append(effect('uv', list(CONTEXTS), f'Mesh {layer["scrollingMesh"]} is the stream mesh '
                                    f'(unlit, UV offset)', note))
        return found

    def style_effects(self, value):
        """What the render style of a model adds besides its passes."""
        entry, name = self.style_of(value)
        if not name:
            return []
        scene, found = Scene(value, [], False), []
        if name in self.looks.styles:
            look, line = self.looks.styles[name]
            return self.look_effects(look, f'{RENDER_STYLES}:{line} render style "{name}"')
        if name not in self.looks.style_code:
            return []
        style = self.looks.style_code[name]
        branch = self.style_applies(name, scene, Draw())
        if branch is not None:
            found += self.branch_effects(branch)
        if name in self.looks.style_spawns:
            spawn = self.function(STYLES_FILE, self.looks.style_spawns[name])
            found += self.branch_effects(spawn)
        table = f'{SOURCE}/{STYLES_FILE}:{style.line} render style "{name}"'
        if style.shine:
            flags = ', '.join(flag[len('RENDER_'):].lower() for flag in self.looks.shines.get(style.shine, ())
                              if flag != rules.RENDER_BRIGHT)
            found.append(effect('mesh-pass', list(CONTEXTS), f'Shines below +3 too: two more additive passes over the '
                                f'model ({flags})', [table]))
        if style.glow:
            found.append(effect('mesh-pass', list(CONTEXTS), 'The +7 glow pass is drawn by the render style\'s own '
                                f'code ({style.glow})', [table]))
        return found

    def code_effects(self, value, draws):
        effects = []
        attributes = self.first('attributes', value)
        if attributes is not None:
            effects += self.branch_effects(attributes)
        branch, _ = self.effect_branch(value)
        if branch is not None:
            effects += self.branch_effects(branch)
        entry = self.looks.entry_of(value)
        spawn = self.looks.effects.get(entry.effect, ('', ''))[1] if entry is not None else ''
        if spawn:
            effects += self.branch_effects(self.function(EFFECTS_FILE, spawn))
        for role in ('worn-effects', 'held-effects'):
            found = self.first(role, value)
            if found is not None:
                effects += self.branch_effects(found)
        effects += self.style_effects(value)
        for _, draw in draws:
            effects += [found for found in draw.effects if found not in effects]
        return effects + self.pass_effects(draws)

    def texture_name(self, texture):
        """A pass's texture for a sentence: the engine's name with its file where the load code names it."""
        if not texture.startswith(rules.ENGINE_TEXTURE_PREFIX):
            return 'its own texture'
        name = texture[len(rules.ENGINE_TEXTURE_PREFIX):]
        file = self.looks.bitmap_file(name) if BITMAP.match(name) else ''
        return f'{name} ({file})' if file else name

    def pass_effects(self, draws):
        """Overlay passes (a mesh drawn again), a mesh drawn with another texture than its own, and
        animated texture coordinates, from the draws."""
        found, seen = [], set()
        replaced = OrderedDict()  # (mesh, texture) -> contexts and the lines of the calls
        for context, draw in draws:
            base, overlays = rules.base_and_overlays(draw.passes)
            for mesh, drawn in base.items():
                if drawn.texture.startswith(rules.ENGINE_TEXTURE_PREFIX):
                    contexts, lines = replaced.setdefault((mesh, drawn.texture), ([], []))
                    contexts.append(context)
                    lines.append(drawn.note)
            for mesh, extra in overlays.items():
                for drawn in extra:
                    key = (mesh, drawn.mode, drawn.texture, drawn.note)
                    same_as_base = (drawn.mode, drawn.texture) == (base[mesh].mode, base[mesh].texture)
                    if key in seen or (drawn.conditional and same_as_base):
                        continue  # another arm of the same condition, not an extra layer
                    seen.add(key)
                    when = ' (only under a runtime condition)' if drawn.conditional else ''
                    found.append(effect('mesh-pass', list(CONTEXTS),
                                        f'Mesh {mesh} is drawn again, {drawn.mode}, with {self.texture_name(drawn.texture)}{when}',
                                        [drawn.note]))
            for mesh, where in draw.uv.items():
                if ('uv', mesh, where) in seen:
                    continue
                seen.add(('uv', mesh, where))
                found.append(effect('uv', list(CONTEXTS), f'Mesh {mesh}: texture coordinates animated by the code', [where]))
        for (mesh, texture), (contexts, lines) in replaced.items():
            found.append(effect('texture', unique(contexts), f'Mesh {mesh} is drawn with {self.texture_name(texture)} '
                                f'instead of its own texture ({", ".join(unique(contexts))})', unique(lines)))
        return found


def effect(kind, contexts, summary, evidence_lines, source='code'):
    return {'kind': kind, 'contexts': contexts, 'summary': summary, 'evidence': evidence_lines, 'source': source}


# --- one item -----------------------------------------------------------------------------------

def mesh_entry(mesh, draws_by_context, anchors):
    """render-facts.json entry of one mesh: its mode per context and the evidence."""
    entry = OrderedDict([('mesh', mesh.index), ('texture', mesh.texture), ('flags', mesh_flags(mesh))])
    evidence_lines, problems, replaced = [], [], []
    for context in CONTEXTS:
        draw = draws_by_context.get(context)
        if draw is None:
            entry[context] = None
            continue
        base, _ = rules.base_and_overlays(draw.passes)
        drawn = base.get(mesh.index)
        if drawn is None and draw.unknown_meshes and not hidden_evidence(mesh, context, anchors):
            entry[context] = rules.UNKNOWN  # a call draws a mesh the reading cannot name: maybe this one
            problems.append(f'mesh {mesh.index} ({context}): may be the mesh of {draw.unknown_meshes[0]}')
            continue
        if drawn is None:
            entry[context] = rules.HIDDEN
            evidence_lines += hidden_evidence(mesh, context, anchors)
            continue
        entry[context] = drawn.mode
        if drawn.texture != rules.OWN_TEXTURE:
            replaced.append(context)
        if drawn.unsure:
            entry[context] = rules.UNKNOWN
            problems.append(f'mesh {mesh.index} ({context}): drawn with a texture other than its own whose file is not '
                            f'known ({drawn.texture[len(rules.ENGINE_TEXTURE_PREFIX):] or "not named"}): opaque or cut '
                            f'out by that texture\'s alpha')
        elif drawn.mode == rules.UNKNOWN:
            problems.append(f'mesh {mesh.index} ({context}): {drawn.note or drawn.rule}')
        evidence_lines += draw.evidence
        if drawn.note:
            evidence_lines.append(drawn.note)
        for name in RULE_OF_PASS.get(drawn.rule, ()):
            evidence_lines.append(anchors[name])
        if mesh.flags.bright:
            evidence_lines.append(anchors['flags-applied'])
        if drawn.mode == rules.ALPHA_TEST and mesh.has_alpha:
            evidence_lines += [anchors['tga'], anchors['alpha-test'], anchors['alpha-threshold']]
    entry['evidence'] = unique(evidence_lines)
    if replaced:
        entry[REPLACED_TEXTURE] = replaced
    return entry, problems


def mesh_flags(mesh):
    flags = mesh.flags.names()
    if mesh.has_alpha:
        flags.append('alpha-channel')
    if mesh.never_drawn:
        flags.append('hid')
    if mesh.skin_or_hair:
        flags.append('skin-or-hair')
    if mesh.missing:
        flags.append('missing-file')
    if mesh.slot >= 0 and mesh.slot != mesh.index:
        flags.append(f'texture-slot-{mesh.slot}')
    return flags


def hidden_evidence(mesh, context, anchors):
    if mesh.never_drawn:
        return [anchors['hid']]
    if mesh.flags.hidden:
        return [anchors['flags-applied']]
    if mesh.skin_or_hair and context in (DROPPED, INVENTORY):
        return [anchors['skin']]
    return []


def unique(values):
    return list(OrderedDict.fromkeys(values))


def model_value(source, model, item_value):
    constant = model.get('model')
    if model.get('role') == 'item' or not constant:
        return item_value
    try:
        return source.value(constant)
    except Unresolved:
        return item_value


class Builder:
    def __init__(self, root=ROOT):
        self.root = Path(root)
        self.source = Source(self.root)
        self.regions = read_regions(self.source)
        self.looks = Looks(self.source)
        self.reader = ItemReader(self.root, self.source, self.regions, self.looks)
        self.notes = render_notes.resolved(self.source)

    def item(self, key, item):
        value = self.source.type_of_key(key)
        worn = worn_kind(item)
        models, draws, problems = [], [], []
        for model in item.get('models', []):
            entry, model_draws, model_problems = self.model(item, model, value, worn)
            models.append(entry)
            draws += model_draws
            problems += model_problems
        note = self.notes.get(key, {})
        render_notes.apply_mesh_notes(models, note)
        effects = self.reader.code_effects(value, draws)
        effects += [dict(entry) for entry in render_notes.generic_effects(self.notes, item, worn)]
        effects += [dict(entry) for entry in note.get('effects', [])]
        level = self.reader.level_glow(value)
        excellent = self.reader.excellent(value)
        problems = [problem for problem in problems if not render_notes.is_resolved(note, problem)]
        unverified = unique(problems + list(note.get('unverified', [])))
        status = STATUS_UNVERIFIED if unverified else STATUS_VERIFIED if note.get('verified') else STATUS_CODE
        entry = OrderedDict([
            ('key', key), ('name', item.get('name')), ('worn', worn or 'not-worn'), ('status', status),
            ('summary', note.get('summary', '')), ('verified', note.get('verified', '')),
            ('models', models), ('level_glow', level), ('excellent', excellent),
            ('effects', effects), ('unverified', unverified),
        ])
        entry['request'] = for_request(entry)
        entry['drawn'] = drawn_line(entry)
        for model in models:
            for mesh in model['meshes']:
                mesh.pop(REPLACED_TEXTURE, None)
        return entry

    def model(self, item, model, item_value, worn):
        value = model_value(self.source, model, item_value)
        meshes = model_meshes(self.root, model, self.looks.none_blend_of(value))
        draws, by_context, problems = [], {}, []
        for context in ROLE_CONTEXTS.get(model.get('role'), CONTEXTS):
            if context == WORN and worn in (None, WORN_JEWELLERY):
                continue
            draw = self.reader.draw(value, meshes, context, item['group'])
            if context == WORN and worn == WORN_PET:
                draw.problems.append('pets and mounts are drawn by their own code when worn; not analysed')
            by_context[context] = draw
            draws.append((context, draw))
            problems += draw.problems
        mesh_entries = []
        for mesh in meshes:
            mesh_json, mesh_problems = mesh_entry(mesh, by_context, self.reader.anchors)
            mesh_entries.append(mesh_json)
            problems += mesh_problems
        entry = OrderedDict([('role', model.get('role')), ('bmd', model.get('bmd')), ('meshes', mesh_entries)])
        if model.get('class'):
            entry['class'] = model['class']
        return entry, draws, unique(problems)

    def document(self, catalog):
        items = catalog.get('items', {})
        ordered = sorted(items, key=lambda key: tuple(int(part) for part in key.split('-')))
        out_items = OrderedDict((key, self.item(key, items[key])) for key in ordered)
        render_notes.check_keys(self.notes, out_items)
        return OrderedDict([
            ('schema', SCHEMA),
            ('generated_by', 'tools/item_editor/render_facts.py'),
            ('catalog', CATALOG),
            ('modes', MODE_TEXT),
            ('rules', OrderedDict((name, self.reader.rules[name]) for name in RULE_ANCHORS)),
            ('regions', [self.region_line(region) for region in REGIONS] + self.table_lines()),
            ('level_tiers', LEVEL_TIERS),
            ('excellent', EXCELLENT_TEXT),
            ('ancient', ANCIENT_TEXT),
            ('counts', counts(out_items)),
            ('items', out_items),
        ])

    def region_line(self, region):
        where = self.source.where(Anchor(region.file, region.needles))
        return f'{where} {region.name} ({region.role})'

    def table_lines(self):
        """Where the looks with a name are: the tables of the code and the data files."""
        tables = ((STYLES_FILE, STYLE_TABLE, 'render styles in code'), (STYLES_FILE, STYLE_SPAWN_TABLE, 'render style spawns'),
                  (EFFECTS_FILE, EFFECT_TABLE, 'item effects'))
        lines = [f'{self.source.where(Anchor(file, (needle,)))} ({what})' for file, needle, what in tables]
        return lines + [f'{RENDER_STYLES} (render styles that are data)',
                        f'{MODEL_DATA} (model data: renderStyle, itemEffect, glow, noneBlendMeshes)']


# --- what requests and the editor use ------------------------------------------------------------

def mesh_modes(mesh):
    """The modes a mesh is drawn in with its own texture: what its texture has to be painted for. A
    context that draws it with another texture (an inventory texture, a reflection) does not count."""
    return {mesh[context] for context in CONTEXTS if mesh.get(context) and context not in mesh.get(REPLACED_TEXTURE, ())}


def must_keep_lines(models):
    """The must_keep lines of an item's models (README: 'How the game draws the item')."""
    lines = []
    for model in models:
        name = Path(model['bmd'] or '').name
        for mode, template in ((rules.ADDITIVE, ADDITIVE_LINE), (rules.ALPHA_BLEND, ALPHA_BLEND_LINE),
                               (rules.ALPHA_TEST, CUT_OUT_LINE)):
            meshes = [str(mesh['mesh']) for mesh in model['meshes'] if mode in mesh_modes(mesh)]
            if meshes:
                lines.append(template.format(model=name, meshes=mesh_list(meshes)))
    return unique(lines)


def mesh_list(meshes):
    return ('mesh ' if len(meshes) == 1 else 'meshes ') + ', '.join(meshes)


def for_request(entry):
    """What an item request copies (README 'How the game draws the item'): the effect sentences and
    the must_keep lines. The request's per-mesh table is request_model() of every model."""
    effects = [entry['level_glow']['summary'], entry['excellent']['summary']]
    effects += [item['summary'] for item in entry['effects']]
    if not entry['effects']:
        effects.append(NO_EFFECTS)
    return OrderedDict([('effects', unique(effects)), ('must_keep', must_keep_lines(entry['models']))])


def drawn_line(entry):
    """The Item Editor's one-line summary: which meshes blend, and the effect kinds."""
    several = len(entry['models']) > 1
    parts = []
    for label, modes in (('blended meshes', {rules.ADDITIVE}), ('alpha-blended meshes', {rules.ALPHA_BLEND}),
                         ('cut-out meshes', {rules.ALPHA_TEST})):
        found = []
        for model in entry['models']:
            meshes = [str(mesh['mesh']) for mesh in model['meshes'] if mesh_modes(mesh) & modes]
            if meshes:
                prefix = f'{Path(model["bmd"] or "").name} ' if several else ''
                found.append(prefix + ','.join(meshes))
        if found:
            parts.append(f'{label} {"; ".join(found)}')
    kinds = unique(item['kind'] for item in entry['effects'])
    if entry['level_glow']['kind'] == 'none':
        kinds.insert(0, 'no level glow')
    return f'{" / ".join(parts) or "opaque"} / effects: {", ".join(kinds) or "none"}'


def counts(items):
    result = OrderedDict([('items', len(items))])
    for status in (STATUS_VERIFIED, STATUS_CODE, STATUS_UNVERIFIED):
        result[status] = sum(1 for entry in items.values() if entry['status'] == status)
    result['with_blended_meshes'] = sum(1 for entry in items.values()
                                        if any(line.startswith('Blended') for line in entry['request']['must_keep']))
    result['with_effects'] = sum(1 for entry in items.values() if entry['effects'])
    return result


def render(document):
    return json.dumps(document, indent=JSON_INDENT, ensure_ascii=False) + '\n'


def load_catalog(root):
    catalog = json.loads((Path(root) / CATALOG).read_text(encoding='utf-8'))
    if catalog.get('schema') != CATALOG_SCHEMA:
        raise SystemExit(f'{CATALOG} is not {CATALOG_SCHEMA}')
    return catalog


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    parser.add_argument('--check', action='store_true', help='exit 1 when render-facts.json is out of date')
    parser.add_argument('--root', default=str(ROOT), help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    root = Path(args.root)
    text = render(Builder(root).document(load_catalog(root)))
    output = root / OUTPUT
    if args.check:
        current = output.read_text(encoding='utf-8') if output.exists() else ''
        if current != text:
            print(f'{OUTPUT} is out of date; run python3 tools/item_editor/render_facts.py', file=sys.stderr)
            return 1
        print(f'{OUTPUT} is up to date')
        return 0
    output.write_text(text, encoding='utf-8')
    print(f'wrote {OUTPUT}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
