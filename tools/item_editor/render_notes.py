"""Render facts read or checked by hand, merged into render-facts.json by render_facts.py.

The automatic reading (render_code.py + render_rules.py) covers the RenderMesh / RenderBody calls of
the per-type render branches. This table adds what it cannot see or gets wrong, always with the code
lines (anchors) that show it: capes that are drawn as cloth when worn, draws that depend on a
condition, effects outside the scanned branches (weapon swing trails), and what was checked in the
running client. Keys are catalog item keys ("<group>-<index>").

Per item (all fields optional):
- summary: one or two sentences for artists;
- verified: what was checked in the client and when (the item's status becomes verified-in-client);
- meshes: {(model index, mesh index): {context: (mode, reason, [anchors])}} overriding a mode;
- effects: [(kind, contexts, summary, [anchors])];
- unverified: open questions;
- resolved: substrings of automatic problems this note has checked by hand (they are dropped).
"""

from render_code import Anchor

OBJECT = 'Engine/Object/ZzzObject.cpp'
CHARACTER = 'Engine/Object/ZzzCharacter.cpp'
OPEN_DATA = 'Engine/Object/ZzzOpenData.cpp'
WORN, DROPPED, INVENTORY = 'worn', 'dropped', 'inventory'

# Effects that come from code outside the per-type branches and apply to a whole group of items:
# (item groups, worn kinds or None for any, kind, contexts, summary, anchors).
HAND_GROUPS = (0, 1, 2, 3)  # swords, axes, maces, spears
BACK_GROUPS = (12, 13)

GENERIC_EFFECTS = (
    (HAND_GROUPS, ('hand',), 'trail', (WORN,),
     'A swing trail (weapon blur) follows the blade during attacks; its texture comes from the attack, '
     'not from the item',
     [Anchor(CHARACTER, ('void CreateWeaponBlur(', 'int BlurType = 0;'), 'CreateWeaponBlur')]),
    (BACK_GROUPS, ('back',), 'animation', (WORN,),
     'Worn wings and capes play their own animation (slow while standing, faster while flying)',
     [Anchor(CHARACTER, ('PART_t* w = &c->Wing;', 'w->PlaySpeed = 0.25f;'), 'RenderCharacter: wing play speed')]),
)

# How the client checks were made (render-facts.json "verified").
PREVIEW = 'Item Editor preview 2026-09-24 (turntable, inventory, equipped, ground)'

# Code lines shared by several notes.
LINK_SKIPS_CAPES = Anchor(CHARACTER, ('void RenderLinkObject(', 'if (Type != MODEL_CAPE_OF_LORD'),
                          'RenderLinkObject: the Cape of Lord, Cape of Fighter and both small capes are not drawn as a model when worn')
CLOTH_RENDER = Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)', 'pCloth[i].Render(&CloakLight);'),
                      'RenderCharacter: the cloth pieces are drawn')
CLOTH_ALPHA = Anchor('Engine/Physics/PhysicsManager.cpp', ('case PCT_MASK_ALPHA:', 'EnableAlphaTest();'),
                     'cloth with PCT_MASK_ALPHA is alpha-tested')
LORD_CAPE_CLOTH = Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)', 'else\n                {',
                                     'BITMAP_ROBE + 7, BITMAP_ROBE + 7'),
                         'the Dark Lord cape cloth uses BITMAP_ROBE + 7')
ROBE_7 = Anchor(OPEN_DATA, ('BITMAP_ROBE + 7);',), 'BITMAP_ROBE + 7 = Player/DarklordRobe.tga')
BODY_LIGHT_WHITE = Anchor(OBJECT, ('void RenderPartObjectBody(BMD* b', 'else if (Type == MODEL_CAPE_OF_EMPEROR)',
                                   'if (b->BodyLight[0] == 1 && b->BodyLight[1] == 1 && b->BodyLight[2] == 1)'),
                          'Cape of Emperor: all meshes only when the light is exactly white, else mesh 0')

ITEM_NOTES = {
    '12-0': {
        'verified': (PREVIEW + '; with a black/grey/white/red test texture the black band vanished and the '
                               'character showed through it: additive'),
        'summary': 'Wings of Elf: one mesh, drawn additively everywhere (ItemObjectAttribute sets BlendMesh 0): '
                   'black is see-through, bright colour glows. Paint the feathers light on black; a dark or '
                   'opaque texture turns into faint or hard-edged streaks. No +level glow, no excellent shine.',
    },
    '12-1': {
        'verified': (PREVIEW + '; a white test .tga with alpha bands 0, 20, 30, 60 and 100 % showed only the 30, '
                               '60 and 100 % bands: cut-out at 25 %'),
        'summary': ('Wings of Heaven: one .tga mesh cut out by its alpha (alpha-test); the colour is drawn as '
                    'painted.'),
    },
    '12-2': {
        'summary': ('Wings of Satan: one .tga mesh cut out by its alpha (alpha-test); the colour is drawn as '
                    'painted.'),
    },
    '12-3': {
        'summary': 'Wings of Spirits: one mesh drawn additively (BlendMesh 0), like the Wings of Elf; half size on the '
                   'ground and in the inventory (ItemObjectAttribute scale 0.5).',
    },
    '12-4': {
        'verified': (PREVIEW + '; with a test texture on both meshes mesh 0 kept its black band, mesh 1 lost it'),
        'summary': 'Wings of Soul: mesh 0 opaque, mesh 1 (NEWW_R.jpg, _R) additive with a pulsing brightness that '
                   'the code sets every frame.',
    },
    '12-5': {
        'verified': 'Item Editor preview 2026-09-24 (turntable, inventory, equipped, ground)',
        'summary': ('Wings of Dragon: mesh 0 opaque, mesh 1 (NDW_R.jpg, _R) additive with a slowly pulsing '
                    'brightness.'),
    },
    '12-6': {
        'summary': 'Wings of Darkness: the .tga mesh is drawn twice, first as an additive chrome layer '
                   '(BITMAP_CHROME + 1), then cut out by its alpha; blue flare sprites and thunder/spirit beams run '
                   'between the wing bones. In a safe zone the worn wings use action 1.',
        'effects': [('animation', (WORN,), 'In a safe zone the worn wings switch to action 1',
                     [Anchor(CHARACTER, ('void RenderLinkObject(', 'MODEL_WINGS_OF_DARKNESS) && c->SafeZone)'),
                             'RenderLinkObject')])],
    },
    '12-36': {
        'verified': (PREVIEW + '; the red light sprites show on the character'),
        'summary': 'Wing of Storm: mesh 0 additive (RENDER_BRIGHT), mesh 1 additive with a 4-frame UV flip-book '
                   '(lightningblast.jpg, 4 columns), mesh 2 (.tga) cut out by alpha; clouds, lights and random '
                   'thunder effects at 50+ bones.',
    },
    '12-37': {
        'summary': 'Wing of Eternal: one .tga mesh cut out by alpha; 28 light sprites at the bones.',
    },
    '12-38': {
        'summary': 'Wing of Illusion: one mesh (elfwing3d_R.jpg, _R) drawn additively; flare and light sprites, '
                   'shiny particles.',
    },
    '12-39': {
        'verified': (PREVIEW + '; purple glow and particles on the character'),
        'summary': 'Wing of Ruin: mesh 0 (msword02.tga) additive with a pulsing light (the .tga alpha is ignored), '
                   'mesh 1 opaque plus an additive layer with msword01_r.jpg; chrome-energy particles. Worn by a '
                   'Magic Gladiator it also has a cloth cape with msword03.tga.',
        'effects': [('cloth', (WORN,), 'A cloth cape with Item/msword03.tga (alpha-tested) when a Magic Gladiator wears it',
                     [Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)',
                                         'if (c->Wing.Type == MODEL_WING_OF_RUIN)', 'BITMAP_ROBE + 8'), 'cloth'),
                      Anchor(OPEN_DATA, ('BITMAP_ROBE + 8);',), 'BITMAP_ROBE + 8 = Item/msword03.tga'),
                      CLOTH_ALPHA])],
    },
    '12-40': {
        'verified': (PREVIEW + ': turntable and ground showed only the collar (mesh 0), the inventory all three '
                               'meshes, worn a cloth cape'),
        'summary': 'Cape of Emperor: the model draws all three meshes only when its light is exactly white '
                   '(the inventory); otherwise only mesh 0 (the collar). Worn, the cape itself is cloth: '
                   'dl_redwings02.tga (cape) and dl_redwings03.tga (two ribbons), alpha-tested.',
        'meshes': {
            (0, 1): {WORN: ('hidden', 'the model draws mesh 1 only under a white light; the worn cape is cloth',
                            [BODY_LIGHT_WHITE]),
                     DROPPED: ('hidden', 'the model draws mesh 1 only under a white light; the ground light is not white',
                               [BODY_LIGHT_WHITE])},
            (0, 2): {WORN: ('hidden', 'the model draws mesh 2 only under a white light; the worn cape is cloth',
                            [BODY_LIGHT_WHITE]),
                     DROPPED: ('hidden', 'the model draws mesh 2 only under a white light; the ground light is not white',
                               [BODY_LIGHT_WHITE])},
        },
        'effects': [('cloth', (WORN,), 'Worn: cloth cape with Item/dl_redwings02.tga and two cloth ribbons with '
                     'Item/dl_redwings03.tga (alpha-tested), the same files as meshes 2 and 1',
                     [Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)',
                                         'BITMAP_ROBE + 9, BITMAP_ROBE + 9'), 'cloth'),
                      Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)',
                                         'BITMAP_ROBE + 10, BITMAP_ROBE + 10'), 'ribbons'),
                      Anchor(OPEN_DATA, ('BITMAP_ROBE + 9);',), 'Item/dl_redwings02.tga'),
                      Anchor(OPEN_DATA, ('BITMAP_ROBE + 10);',), 'Item/dl_redwings03.tga'),
                      CLOTH_ALPHA])],
        'resolved': ('a draw call under a runtime condition',),
    },
    '12-42': {
        'summary': 'Wind of Despair: both meshes opaque; mesh 1 gets an additive chrome6 layer on top.',
    },
    '12-43': {
        'summary': 'Wing of Dimension: meshes 0 and 1 opaque, mesh 2 (.tga) cut out by alpha; mesh 1 gets an additive '
                   'chrome layer; flare sprites.',
    },
    '12-49': {
        'verified': (PREVIEW + ': model in turntable, inventory and ground; worn a cloth cape'),
        'summary': 'Cape of Fighter: in the inventory and on the ground the .tga model is cut out by alpha. Worn by a '
                   'Rage Fighter the model is not drawn; the cape is cloth with the same file, Item/NCcape.tga.',
        'meshes': {(0, 0): {WORN: ('hidden', 'not drawn as a model when worn; the cape is cloth', [LINK_SKIPS_CAPES])}},
        'effects': [('cloth', (WORN,), 'Worn: a cloth cape with Item/NCcape.tga (alpha-tested), the model\'s own texture',
                     [Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)', 'else\n                {',
                                         'BITMAP_NCCAPE, BITMAP_NCCAPE'), 'cloth'),
                      Anchor(OPEN_DATA, ('BITMAP_NCCAPE,',), 'Item/NCcape.tga'), CLOTH_ALPHA, CLOTH_RENDER])],
    },
    '12-50': {
        'verified': (PREVIEW + ': turntable showed only mesh 0, inventory and ground all meshes, worn a cloth '
                               'cape'),
        'summary': 'Cape of Overrule: the model draws all three meshes only under an exactly white light '
                   '(inventory; on the ground the code forces white), otherwise mesh 0. Worn, the cape is cloth: '
                   'monke_manto.TGA (cape) and monk_manto01.TGA (two strips).',
        'meshes': {
            (0, 1): {WORN: ('hidden', 'the model draws mesh 1 only under a white light; the worn cape is cloth',
                            [Anchor(OBJECT, ('void RenderPartObjectBody(BMD* b', 'else if (Type == MODEL_CAPE_OF_OVERRULE)',
                                             'if (b->BodyLight[0] == 1'), 'white light only')])},
            (0, 2): {WORN: ('hidden', 'the model draws mesh 2 only under a white light; the worn cape is cloth',
                            [Anchor(OBJECT, ('void RenderPartObjectBody(BMD* b', 'else if (Type == MODEL_CAPE_OF_OVERRULE)',
                                             'if (b->BodyLight[0] == 1'), 'white light only')])},
        },
        'effects': [('cloth', (WORN,), 'Worn: a cloth cape with Item/monke_manto.TGA and two cloth strips with '
                     'Item/monk_manto01.TGA (alpha-tested), the files of meshes 2 and 1',
                     [Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)', 'BITMAP_MANTOE, BITMAP_MANTOE'), 'cape'),
                      Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)', 'BITMAP_MANTO01, BITMAP_MANTO01'), 'strips'),
                      Anchor(OBJECT, ('void RenderDroppedItem(', 'else if (o->Type == MODEL_CAPE_OF_OVERRULE)'),
                             'on the ground the light is forced to white'),
                      CLOTH_ALPHA])],
        'resolved': ('a draw call under a runtime condition',),
    },
    '13-30': {
        'verified': (PREVIEW + ': model in turntable, inventory and ground; worn a cloth cape'),
        'summary': 'Cape of Lord: in the inventory and on the ground the .tga model (Item/DarkLordRobe) is cut out by '
                   'alpha. Worn by a Dark Lord the model is not drawn; the cape is cloth with a different file, '
                   'Player/DarklordRobe.tga - repainting the item texture does not change the worn cape.',
        'meshes': {(0, 0): {WORN: ('hidden', 'not drawn as a model when worn; the cape is cloth', [LINK_SKIPS_CAPES])}},
        'effects': [('cloth', (WORN,), 'Worn: a cloth cape with Player/DarklordRobe.tga (alpha-tested), not the item\'s '
                     'Item/DarkLordRobe texture',
                     [LORD_CAPE_CLOTH, ROBE_7, CLOTH_ALPHA, CLOTH_RENDER])],
    },
    '12-130': {
        'verified': (PREVIEW + ': the model showed only in the inventory (not in the turntable or on the ground);'
                               ' worn a small cloth cape'),
        'summary': 'Small Cape of Lord: the model is drawn only under an exactly white light (the inventory). Worn, the '
                   'cape is a smaller cloth with Player/DarklordRobe.tga.',
        'meshes': {(0, 0): {WORN: ('hidden', 'not drawn as a model when worn; the cape is cloth', [LINK_SKIPS_CAPES]),
                            DROPPED: ('hidden', 'the model draws only under an exactly white light; the ground light is not',
                                      [Anchor(OBJECT, ('void RenderPartObjectBody(BMD* b', 'else if (o->Type == MODEL_WING + 130)',
                                                       'if (b->BodyLight[0] == 1'), 'white light only')])}},
        'effects': [('cloth', (WORN,), 'Worn: a small cloth cape with Player/DarklordRobe.tga (alpha-tested)',
                     [Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)',
                                         'else if (c->Wing.Type == MODEL_WING + 130)', 'BITMAP_ROBE + 7'), 'cloth'),
                      ROBE_7, CLOTH_ALPHA])],
        'resolved': ('a draw call under a runtime condition',),
    },
    '12-135': {
        'summary': 'Little Warrior\'s Cloak: like the Cape of Fighter (same model and texture); worn, a smaller cloth '
                   'with Item/NCcape.tga.',
        'meshes': {(0, 0): {WORN: ('hidden', 'not drawn as a model when worn; the cape is cloth', [LINK_SKIPS_CAPES])}},
        'effects': [('cloth', (WORN,), 'Worn: a cloth cape with Item/NCcape.tga (alpha-tested)',
                     [Anchor(CHARACTER, ('void RenderCharacter(CHARACTER* c', 'if (bCloak)',
                                         'else if (c->Wing.Type == MODEL_WING + 135)', 'BITMAP_NCCAPE'), 'cloth'),
                      CLOTH_ALPHA])],
    },
    '12-41': {
        'verified': (PREVIEW + '; with the black/grey/white/red test texture the black band stayed black: opaque'),
        'summary': 'Wing of Curse: one opaque .jpg mesh, drawn as painted (no blending).',
    },
    '0-14': {
        'verified': (PREVIEW + '; the blue lightning of mesh 1 glows over the blade; with a test texture the '
                               'inner blade (mesh 0) showed through the black band of mesh 1'),
        'summary': ('Lighting Sword: mesh 0 (blade) opaque, mesh 1 (lightning) additive with a pulsing brightness'
                    ' and a random U offset; a tip light while held.'),
    },
    '0-26': {
        'verified': (PREVIEW + ': the flame glows in every view, also in the inventory and on the ground'),
        'summary': ('Flameberge: meshes 1, 3, 4 and 5 are additive; mesh 5 (flamestani.jpg) is a 4-frame UV '
                    'flip-book of the flame; mesh 1 gets an extra chrome pass.'),
    },
    '6-16': {
        'verified': 'Item Editor preview 2026-09-24 (turntable, inventory, equipped, ground)',
        'summary': ('Elemental Shield: mesh 0 (.tga) cut out by alpha, meshes 1 and 3 see-through (alpha 0.8 and '
                    '0.5), mesh 2 additive with a scrolling texture, mesh 3 again additive with a random offset.'),
    },
}


def _where(source, anchors):
    return [source.where(anchor) for anchor in anchors]


def resolved(source):
    """ITEM_NOTES with every anchor turned into 'file:line what'."""
    notes = {}
    for key, note in ITEM_NOTES.items():
        entry = {name: value for name, value in note.items() if name not in ('meshes', 'effects')}
        entry['meshes'] = {position: {context: (mode, reason, _where(source, anchors))
                                      for context, (mode, reason, anchors) in contexts.items()}
                           for position, contexts in note.get('meshes', {}).items()}
        entry['effects'] = [{'kind': kind, 'contexts': list(contexts), 'summary': summary,
                             'evidence': _where(source, anchors), 'source': 'notes'}
                            for kind, contexts, summary, anchors in note.get('effects', ())]
        notes[key] = entry
    notes['*generic*'] = [(groups, worn, {'kind': kind, 'contexts': list(contexts), 'summary': summary,
                                          'evidence': _where(source, anchors), 'source': 'notes'})
                          for groups, worn, kind, contexts, summary, anchors in GENERIC_EFFECTS]
    return notes


def generic_effects(notes, item, worn):
    return [effect for groups, worn_kinds, effect in notes.get('*generic*', [])
            if item['group'] in groups and (worn_kinds is None or worn in worn_kinds)]


def apply_mesh_notes(models, note):
    """Replace the automatic mode of the noted meshes; the note's reason and lines lead the evidence."""
    for (model_index, mesh_index), contexts in note.get('meshes', {}).items():
        if model_index >= len(models) or mesh_index >= len(models[model_index]['meshes']):
            raise KeyError(f'render_notes: no mesh {model_index}/{mesh_index}')
        mesh = models[model_index]['meshes'][mesh_index]
        for context, (mode, reason, evidence) in contexts.items():
            mesh[context] = mode
            mesh['evidence'] = [f'{context}: {reason}'] + evidence + mesh['evidence']


def is_resolved(note, problem):
    return any(fragment in problem for fragment in note.get('resolved', ()))


def check_keys(notes, items):
    unknown = [key for key in notes if key != '*generic*' and key not in items]
    if unknown:
        raise KeyError(f'render_notes: keys not in the catalog: {unknown}')
