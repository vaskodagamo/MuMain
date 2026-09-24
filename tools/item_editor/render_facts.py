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
  the catalog and the BMD;
- the per-type branches of the item render code, read by render_code.py, with BMD::RenderMesh's
  rules (render_rules.py) applied to their RenderMesh / RenderBody calls: ItemObjectAttribute,
  RenderPartObjectEffect (level switch and per-type branches), RenderPartObjectBody,
  RenderPartObjectBodyColor(2) (which meshes get the level passes), RenderLinkObject (worn effects)
  and RenderCharacter (effects of a weapon held in the hand);
- render_notes.py: facts read or checked by hand (cloth capes, the look in the client, corrections
  of the automatic reading), each with its code lines.
Code lines are written as file:line and found again from text anchors on every run, so a changed
engine file makes the run (and --check) fail instead of leaving stale line numbers.

Usage (from anywhere):
    python3 tools/item_editor/render_facts.py           # write render-facts.json
    python3 tools/item_editor/render_facts.py --check   # exit 1 when it is out of date
"""

import argparse
from collections import OrderedDict
from dataclasses import dataclass, field
import json
from pathlib import Path
import re
import struct
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / 'world_editor'))

from cpp_source import SymbolTable, Unresolved, defined_macros, evaluate, read_source, strip_comments, tokenize  # noqa: E402
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
GLOBAL_HEADERS = ('Core/Globals/_define.h', 'Core/Globals/_enum.h')
OBJECT_FILE = 'Engine/Object/ZzzObject.cpp'
CHARACTER_FILE = 'Engine/Object/ZzzCharacter.cpp'
JSON_INDENT = 1

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
            self.symbols.add_header(read_source(self.source / header, self.macros, self.consulted))
        self.texts = {}
        self.model_item = self.symbols.value('MODEL_ITEM')
        self.items_per_group = self.symbols.value('MAX_ITEM_INDEX')

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
    ('flags-applied', Anchor('Render/Models/ZzzBMD.cpp', ('void BMD::RenderBody(', 'getBright()'),
                             'RenderBody: an _R mesh is drawn as its own blend mesh, an _H mesh is skipped')),
    ('hid', Anchor('Data/DataHandler/LoadData.cpp', ("pTexture->FileName[0] == 'h' && pTexture->FileName[1] == 'i'",),
                   'a texture name starting with "hid" is never loaded or drawn')),
    ('skin', Anchor('Data/DataHandler/LoadData.cpp', ('bool isSkin',),
                    'ski*/level*/hair* textures are skin or hair: not drawn where HideSkin is set (ground, inventory)')),
    ('tga', Anchor('Render/Sprites/GlobalBitmap.cpp', ('bool CGlobalBitmap::OpenTga', 'Components = 4'),
                   '.tga textures have an alpha channel')),
    ('jpg', Anchor('Render/Sprites/GlobalBitmap.cpp', ('bool CGlobalBitmap::OpenJpegTurbo', 'Components = 3'),
                   '.jpg textures are opaque')),
    ('mesh-blend', Anchor('Render/Models/ZzzBMD.cpp', ('void BMD::RenderMesh(', 'blendMeshIndex <= -2 || m->Texture == blendMeshIndex'),
                          'RenderMesh: the blend mesh (texture slot == BlendMesh; every mesh when <= -2) is additive, '
                          'unlit, times BlendMeshLight')),
    ('mesh-texture', Anchor('Render/Models/ZzzBMD.cpp', ('void BMD::RenderMesh(', 'else if (alpha < 0.99f || texture->Components == 4)'),
                            'RenderMesh RENDER_TEXTURE: with RENDER_BRIGHT additive, a 4-component texture alpha-tested, else opaque')),
    ('mesh-chrome', Anchor('Render/Models/ZzzBMD.cpp', ('void BMD::RenderMesh(', 'if (m->NoneBlendMesh)'),
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
    ('worn-link', Anchor(CHARACTER_FILE, ('void RenderLinkObject(', 'ItemObjectAttribute(Object);'),
                         'hands, back and wings: RenderLinkObject uses ItemObjectAttribute')),
    ('worn-armour', Anchor(CHARACTER_FILE, ('void RenderCharacter(', 'RenderPartObject(&c->Object, Type, p, c->Light, o->Alpha, p->Level'),
                           'armour parts: RenderPartObject on the character object (BlendMesh -1)')),
    ('body-default', Anchor(OBJECT_FILE, ('void RenderPartObjectBody(BMD* b', 'Check_LuckyItem(Type, -MODEL_ITEM)',
                                          'b->RenderBody(RenderType, Alpha, o->BlendMesh'),
                            'RenderPartObjectBody default: RenderBody(RENDER_TEXTURE, BlendMesh)')),
    ('level-tiers', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'else if (Level < 3 || o->Type == MODEL_ZEN)'),
                           'the +level look')),
    ('excellent', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'if ((ExcellentFlags & 63) > 0'), 'the excellent shine')),
    ('ancient', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'else if (ancientDiscriminator > 0)'), 'the ancient shimmer')),
    ('render-level', Anchor(OBJECT_FILE, ('void RenderPartObjectEffect(', 'if (g_pOption->GetRenderLevel() < 4)'),
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
NO_EXCELLENT_TEXT = 'No excellent shine (the engine leaves wings and capes out)'
ANCIENT_TEXT = 'Ancient: a pulsing additive chrome pass'

# RenderPartObjectEffect's excellent condition leaves these types out (wings and capes).
NO_EXCELLENT_RANGES = (('MODEL_WING', 'MODEL_WINGS_OF_DARKNESS'), ('MODEL_CAPE_OF_LORD', 'MODEL_CAPE_OF_LORD'),
                       ('MODEL_WING_OF_STORM', 'MODEL_WING_OF_DIMENSION'), ('MODEL_WING + 130', 'MODEL_WING + 134'),
                       ('MODEL_CAPE_OF_FIGHTER', 'MODEL_CAPE_OF_OVERRULE'), ('MODEL_WING + 135', 'MODEL_WING + 135'))


# --- regions of the item render code ------------------------------------------------------------

@dataclass(frozen=True)
class Region:
    name: str
    file: str
    kind: str        # 'chain' or 'switch'
    needles: tuple
    role: str        # attributes, level, effect, body, glow-meshes, worn-effects, held-effects
    contexts: tuple = CONTEXTS


REGIONS = (
    Region('ItemObjectAttribute', OBJECT_FILE, 'switch', ('void ItemObjectAttribute(OBJECT* o)', 'switch (o->Type)'),
           'attributes'),
    Region('RenderPartObjectEffect', OBJECT_FILE, 'switch', ('void RenderPartObjectEffect(', 'return;', 'switch (Type)'),
           'level'),
    Region('RenderPartObjectEffect', OBJECT_FILE, 'chain',
           ('void RenderPartObjectEffect(', 'if (o->Type == MODEL_BILL_OF_BALROG)'), 'effect'),
    Region('RenderPartObjectBody', OBJECT_FILE, 'chain',
           ('void RenderPartObjectBody(BMD* b', 'if ((Type == MODEL_STORM_CROW_ARMOR'), 'body'),
    Region('RenderPartObjectBody', OBJECT_FILE, 'chain',
           ('void RenderPartObjectBody(BMD* b', 'if (bIsNotRendered == FALSE);'), 'body-2'),
    Region('RenderPartObjectBodyColor', OBJECT_FILE, 'chain',
           ('void RenderPartObjectBodyColor(BMD* b', 'if (Type == MODEL_LEGENDARY_STAFF)'), 'glow-meshes'),
    Region('RenderLinkObject', CHARACTER_FILE, 'switch',
           ('void RenderLinkObject(', 'Luminosity = (float)(rand() % 30 + 70) * 0.005f;', 'switch (Type)'),
           'worn-effects', (WORN,)),
    Region('RenderCharacter', CHARACTER_FILE, 'switch',
           ('void RenderCharacter(CHARACTER* c', 'bool Success = true;', 'switch (w->Type)'), 'held-effects', (WORN,)),
)
HELD_LIGHT = Anchor(CHARACTER_FILE, ('void RenderCharacter(CHARACTER* c', 'bool Success = true;', 'if (Success)'),
                    'RenderCharacter: light sprite at the tip of a held weapon')

SPAWN_KINDS = {'CreateSprite': 'sprite', 'CreateSpriteFpsChecked': 'sprite', 'RenderBrightEffect': 'sprite',
               'RenderLight': 'sprite', 'CreateParticle': 'particle', 'CreateParticleFpsChecked': 'particle',
               'CreateJoint': 'joint', 'CreateEffect': 'effect-model', 'PlayBuffer': 'sound'}
SPAWN_TEXT = {'sprite': 'Sprites', 'particle': 'Particles', 'joint': 'Joints (beams and trails)',
              'effect-model': 'Effect models', 'sound': 'Sounds'}


def read_regions(source):
    """Region role -> its branches; every branch knows its file and contexts."""
    by_role = {}
    for region in REGIONS:
        text = source.text(region.file)
        reader = render_code.chain_branches if region.kind == 'chain' else render_code.switch_branches
        for branch in reader(region.name, text, 1, region.needles, source.symbols):
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


def model_meshes(root, model):
    structure = model.get('structure') or {}
    slots = mesh_slots(Path(root) / model['bmd']) or []
    missing = {name for name, status in (model.get('texture_status') or {}).items() if status == 'missing'}
    return [rules.mesh_facts(index, texture, slots[index] if index < len(slots) else -1, texture in missing)
            for index, texture in enumerate(structure.get('mesh_textures', []))]


# --- draw-call evaluation -----------------------------------------------------------------------

NUMBER = re.compile(r'^-?\d+(\.\d*)?f?$')
NO_TEXTURE = -1  # RenderMesh / RenderBody: draw with the mesh's own texture
FLAG_NAME = re.compile(r'RENDER_[A-Z0-9_]+')
BITMAP = re.compile(r'^(BITMAP_[A-Z0-9_]+(?:\s*\+\s*\d+)?)$')


@dataclass
class ObjectState:
    """The OBJECT fields the body code reads, as ItemObjectAttribute (or the character) set them."""
    blend: rules.Value
    hidden: rules.Value
    u: str = '0'
    v: str = '0'


def number_of(text):
    text = text.strip()
    return float(text.rstrip('f')) if NUMBER.match(text) else None


def int_value(text, state):
    text = text.strip()
    if text == 'o->BlendMesh':
        return state.blend
    if text == 'o->HiddenMesh':
        return state.hidden
    number = number_of(text)
    return rules.Value(number) if number is not None else rules.Value(text=text)


def alpha_value(text):
    text = text.strip()
    if text in ('o->Alpha', 'Alpha', 'alpha'):
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
    if text in ('o->BlendMeshTexCoordU', 'o->BlendMeshTexCoordV'):
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
    explicit = int_value(texture, state)  # Wing of Storm passes o->HiddenMesh (-1) here
    if bitmap:
        result.texture = ' '.join(bitmap.group(1).split())
    elif not (explicit.known and explicit.number == NO_TEXTURE):
        result.unknown += (f'texture {texture}',)
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
    entries = [(call.line, 0, call) for call in branch.calls]
    entries += [(assignment.line, 1, assignment) for assignment in branch.assignments]
    return [entry for _, _, entry in sorted(entries, key=lambda item: (item[0], item[1]))]


def is_draw(call):
    return call.name in ('RenderMesh', 'RenderBody')


# --- items --------------------------------------------------------------------------------------

@dataclass
class Draw:
    """The passes of one model in one context."""
    passes: list = field(default_factory=list)
    problems: list = field(default_factory=list)
    evidence: list = field(default_factory=list)
    uv: dict = field(default_factory=dict)   # mesh -> line of a call that animates its texture coordinates
    conditional: list = field(default_factory=list)  # draw calls under a runtime condition


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

    def __init__(self, root, source, regions):
        self.root = Path(root)
        self.source = source
        self.regions = regions
        self.rules = {name: source.where(anchor) for name, anchor in RULE_ANCHORS.items()}
        # Evidence lists name a generic rule as "rule:<name>"; render-facts.json "rules" has its line.
        self.anchors = {name: f'{RULE_PREFIX}{name}' for name in RULE_ANCHORS}
        self.no_excellent = set()
        for low, high in NO_EXCELLENT_RANGES:
            self.no_excellent.update(range(source.value(low), source.value(high) + 1))

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

    def body_branch(self, value):
        """The RenderPartObjectBody branch that draws `value`; None for the default RenderBody."""
        found = self.first('body', value)
        if found is not None:
            return found
        return self.first('body-2', value)

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

    def steps(self, value):
        """The branches that draw `value`, in order: RenderPartObjectEffect's, then the body's
        (skipped when the effect branch draws and returns). None stands for the default body."""
        effect = self.first('effect', value)
        steps = [effect] if effect is not None else []
        if effect is not None and effect.returns and any(is_draw(call) for call in effect.calls):
            return steps
        return steps + [self.body_branch(value)]

    def draw(self, value, meshes, context, group):
        """The passes of one model in one context. Draw calls that only run under a runtime condition
        (a buff, the light colour, a loop) count as passes when they leave every mesh's base look as the
        unconditional calls make it; otherwise the draw is marked for checking by hand."""
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
        hide_skin = context in (DROPPED, INVENTORY)
        caller_flags = frozenset({rules.RENDER_TEXTURE})
        for branch in self.steps(value):
            if branch is None:
                draw.passes += rules.passes_of(rules.DrawCall(caller_flags, state.blend), meshes, hide_skin)
                draw.evidence.append(self.anchors['body-default'])
                continue
            draw.evidence.append(f'{self.where(branch)} {branch.function}: branch for this type')
            if branch.qualifiers:
                draw.problems.append(f'{self.where(branch)}: the branch also needs "{branch.qualifiers[0]}"')
            self.run_branch(branch, value, state, meshes, hide_skin, caller_flags, draw, with_conditional)
        return draw

    def run_branch(self, branch, value, state, meshes, hide_skin, caller_flags, draw, with_conditional):
        for entry in statements(branch):
            runs = True if not entry.conditional else render_code.type_guard(
                branch.body, entry.position, value, self.source.symbols)
            if runs is False:
                continue
            if isinstance(entry, render_code.Assignment):
                if runs is None and entry.target in ('BlendMesh', 'HiddenMesh'):
                    draw.problems.append(f'{self.where(branch, entry.line)}: {entry.target} is set under a runtime condition')
                if runs or with_conditional:
                    apply_assignment(entry, state)
                continue
            call = draw_call(entry, state, caller_flags)
            if call is None:
                continue
            if runs is None:
                draw.conditional.append(f'{self.where(branch, entry.line)}: a draw call under a runtime condition '
                                        f'changes the look')
                if not with_conditional:
                    continue
            if call.unknown:
                draw.problems.append(f'{self.where(branch, entry.line)}: cannot read {", ".join(call.unknown)}')
            for drawn in rules.passes_of(call, meshes, hide_skin):
                drawn.note = f'{self.where(branch, entry.line)} {branch.function}'
                drawn.conditional = runs is None
                draw.passes.append(drawn)
                if call.uv_animated:
                    draw.uv.setdefault(drawn.mesh, drawn.note)

    # -- level, excellent and per-type effects --

    def level_glow(self, value):
        forced, evidence_lines = None, []
        for role in ('level', 'effect'):
            branch = self.first(role, value)
            if branch is None:
                continue
            for operator, expression, line in branch.level:
                evidence_lines.append(f'{self.where(branch, line)} {branch.function}: Level {operator} {expression}'.rstrip())
                number = number_of(expression) if operator == '=' else None
                forced = int(number) if number is not None else f'Level {operator} {expression}'.strip()
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
        restricted = self.glow_meshes(value)
        if restricted:
            summary += f'; {restricted[0]}'
            evidence_lines.append(restricted[1])
        return {'kind': kind, 'level': forced if isinstance(forced, int) else None, 'summary': summary,
                'evidence': evidence_lines}

    def glow_meshes(self, value):
        branch = self.first('glow-meshes', value)
        if branch is None:
            return None
        where = f'{self.where(branch)} {branch.function}'
        meshes = sorted({int(number_of(call.args[0])) for call in branch.calls
                         if call.name == 'RenderMesh' and number_of(call.args[0]) is not None})
        if meshes:
            return f'the chrome/metal level passes draw only mesh {", ".join(map(str, meshes))}', where
        skipped = [call.args[6].strip() for call in branch.calls if call.name == 'RenderBody' and len(call.args) > 6]
        if not skipped:
            return None
        state = ObjectState(rules.Value(-1), rules.Value(-1))
        attributes = self.first('attributes', value)
        for assignment in attributes.assignments if attributes else ():
            apply_assignment(assignment, state)
        hidden = int_value(skipped[0], state)
        if hidden.known and hidden.number >= 0:
            return f'the chrome/metal level passes skip mesh {int(hidden.number)}', where
        return None

    def excellent(self, value):
        text = NO_EXCELLENT_TEXT if value in self.no_excellent else EXCELLENT_TEXT
        return {'summary': text, 'evidence': [self.anchors['excellent']]}

    def branch_effects(self, branch):
        contexts = list(branch.contexts)
        # Sprites, particles and joints live in the world: an inventory draw does not show them.
        world_contexts = [context for context in contexts if context != INVENTORY]
        found = []
        spawned = OrderedDict()
        for call in branch.calls:
            kind = SPAWN_KINDS.get(call.name)
            if kind is None:
                continue
            index = 1 if call.name == 'RenderBrightEffect' else 0
            what = ' '.join(call.args[index].split()) if len(call.args) > index else '?'
            spawned.setdefault(kind, OrderedDict()).setdefault(what, []).append(call.line)
        for kind, things in spawned.items():
            lines = sorted({line for found_lines in things.values() for line in found_lines})
            found.append(effect(kind, world_contexts, f'{SPAWN_TEXT[kind]}: {", ".join(things)}',
                                [f'{self.where(branch, line)} {branch.function}' for line in lines]))
        for assignment in branch.assignments:
            where = [f'{self.where(branch, assignment.line)} {branch.function}']
            if assignment.target == 'BlendMeshLight' and number_of(assignment.value) is None:
                word = 'flickers' if 'rand' in assignment.value else 'pulses'
                found.append(effect('pulse', contexts, f'The blend mesh brightness {word} ({assignment.value})', where))
            elif assignment.target in ('BlendMeshTexCoordU', 'BlendMeshTexCoordV') and number_of(assignment.value) is None:
                word = 'jumps to a random offset' if 'rand' in assignment.value else 'scrolls'
                found.append(effect('uv', contexts, f'The blend/stream mesh texture {word} '
                                    f'along {assignment.target[-1]} ({assignment.value})', where))
            elif assignment.target == 'StreamMesh' and number_of(assignment.value) not in (None, -1.0):
                found.append(effect('uv', contexts, f'Mesh {assignment.value} is the stream mesh (unlit, UV offset)', where))
        if branch.function == 'RenderCharacter' and 'Success = false' not in branch.body:
            found.append(effect('sprite', contexts, 'A light sprite at the blade tip while held, coloured by +level',
                                [f'{self.where(branch)} {branch.function}', self.source.where(HELD_LIGHT)]))
        return found

    def code_effects(self, value, draws):
        effects = []
        for role in ('attributes', 'effect', 'worn-effects', 'held-effects'):
            branch = self.first(role, value)
            if branch is not None:
                effects += self.branch_effects(branch)
        body = self.body_branch(value)
        if body is not None:
            effects += self.branch_effects(body)
        return effects + self.pass_effects(draws)

    @staticmethod
    def pass_effects(draws):
        """Overlay passes (a mesh drawn again) and animated texture coordinates, from the draws."""
        found, seen = [], set()
        for context, draw in draws:
            base, overlays = rules.base_and_overlays(draw.passes)
            for mesh, extra in overlays.items():
                for drawn in extra:
                    key = (mesh, drawn.mode, drawn.texture, drawn.note)
                    same_as_base = (drawn.mode, drawn.texture) == (base[mesh].mode, base[mesh].texture)
                    if key in seen or (drawn.conditional and same_as_base):
                        continue  # another arm of the same condition, not an extra layer
                    seen.add(key)
                    texture = (drawn.texture[len(rules.ENGINE_TEXTURE_PREFIX):]
                               if drawn.texture.startswith(rules.ENGINE_TEXTURE_PREFIX) else 'its own texture')
                    when = ' (only under a runtime condition)' if drawn.conditional else ''
                    found.append(effect('mesh-pass', list(CONTEXTS),
                                        f'Mesh {mesh} is drawn again, {drawn.mode}, with {texture}{when}',
                                        [drawn.note]))
            for mesh, where in draw.uv.items():
                if ('uv', mesh, where) in seen:
                    continue
                seen.add(('uv', mesh, where))
                found.append(effect('uv', list(CONTEXTS), f'Mesh {mesh}: texture coordinates animated by the code', [where]))
        return found


def effect(kind, contexts, summary, evidence_lines, source='code'):
    return {'kind': kind, 'contexts': contexts, 'summary': summary, 'evidence': evidence_lines, 'source': source}


# --- one item -----------------------------------------------------------------------------------

def mesh_entry(mesh, draws_by_context, anchors):
    """render-facts.json entry of one mesh: its mode per context and the evidence."""
    entry = OrderedDict([('mesh', mesh.index), ('texture', mesh.texture), ('flags', mesh_flags(mesh))])
    evidence_lines, problems = [], []
    for context in CONTEXTS:
        draw = draws_by_context.get(context)
        if draw is None:
            entry[context] = None
            continue
        base, _ = rules.base_and_overlays(draw.passes)
        drawn = base.get(mesh.index)
        if drawn is None:
            entry[context] = rules.HIDDEN
            evidence_lines += hidden_evidence(mesh, context, anchors)
            continue
        entry[context] = drawn.mode
        if drawn.mode == rules.UNKNOWN:
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
        self.reader = ItemReader(self.root, self.source, self.regions)
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
        return entry

    def model(self, item, model, item_value, worn):
        value = model_value(self.source, model, item_value)
        meshes = model_meshes(self.root, model)
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
            ('regions', [self.region_line(region) for region in REGIONS]),
            ('level_tiers', LEVEL_TIERS),
            ('excellent', EXCELLENT_TEXT),
            ('ancient', ANCIENT_TEXT),
            ('counts', counts(out_items)),
            ('items', out_items),
        ])

    def region_line(self, region):
        where = self.source.where(Anchor(region.file, region.needles))
        return f'{where} {region.name} ({region.role})'


# --- what requests and the editor use ------------------------------------------------------------

def mesh_modes(mesh):
    return {mesh[context] for context in CONTEXTS if mesh.get(context)}


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
