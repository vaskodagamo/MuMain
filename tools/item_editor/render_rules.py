"""How BMD::RenderMesh and BMD::RenderBody draw one mesh; standard library only.

This is a port of the engine's rules, not a renderer. It answers, for one mesh of an item model and
one render call of the client's item code, whether the mesh is drawn, with which blend state and
with which texture. render_facts.py feeds it the calls it finds in the C++ (render_code.py) and the
mesh facts from the BMD; the result is the per-mesh draw mode in render-facts.json.

Sources (line numbers are resolved at build time from the anchors in render_facts.py):
- texture name flags: TextureScriptParsing::parsingTScriptA (Render/Sprites/TextureScript.cpp),
  applied per mesh by BMD::Open2 (Render/Models/ZzzBMD.cpp) and honoured by BMD::RenderBody;
- file kinds: CLoadData::OpenTexture (Data/DataHandler/LoadData.cpp): a name starting with "hid" is
  never loaded (BITMAP_HIDE), "ski"/"level" marks skin and "hair" marks hair; a .tga is decoded with
  4 components (CGlobalBitmap::OpenTga), a .jpg with 3 (CGlobalBitmap::OpenJpegTurbo);
- blend states: EnableAlphaBlend (GL_ONE, GL_ONE: additive, no depth write), EnableAlphaTest
  (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA with texels at alpha <= 0.25 discarded, depth write on),
  DisableAlphaBlend (opaque), EnableAlphaBlendMinus (subtractive) in
  Render/Textures/ZzzOpenglUtil.cpp and MuRendererSDLGpu.cpp.
"""

from dataclasses import dataclass, field

# Draw modes of render-facts.json (assets-work/Items/README.md explains them for artists).
OPAQUE = 'opaque'
ALPHA_TEST = 'alpha-test'
ADDITIVE = 'blended-additive'
ALPHA_BLEND = 'blended-alpha'
SUBTRACT = 'blended-subtract'
HIDDEN = 'hidden'
UNKNOWN = 'unknown'

# RENDER_* flags (Render/Models/ZzzBMD.h). Only the names matter here.
RENDER_COLOR = 'RENDER_COLOR'
RENDER_TEXTURE = 'RENDER_TEXTURE'
RENDER_BRIGHT = 'RENDER_BRIGHT'
RENDER_DARK = 'RENDER_DARK'
RENDER_LIGHTMAP = 'RENDER_LIGHTMAP'
RENDER_EXTRA = 'RENDER_EXTRA'
CHROME_FLAGS = ('RENDER_CHROME', 'RENDER_CHROME2', 'RENDER_CHROME3', 'RENDER_CHROME4', 'RENDER_CHROME5',
                'RENDER_CHROME6', 'RENDER_CHROME7', 'RENDER_METAL', 'RENDER_OIL')
# Chrome passes that BMD::RenderMesh draws additively (EnableAlphaBlend).
ADDITIVE_CHROME_FLAGS = ('RENDER_CHROME3', 'RENDER_CHROME4', 'RENDER_CHROME5', 'RENDER_CHROME7', RENDER_BRIGHT)
# The engine texture a chrome pass binds when the call names none (BMD::RenderMesh, in this order).
CHROME_TEXTURES = (('RENDER_CHROME2', 'BITMAP_CHROME2'), ('RENDER_CHROME3', 'BITMAP_CHROME2'),
                   ('RENDER_CHROME4', 'BITMAP_CHROME2'), ('RENDER_CHROME6', 'BITMAP_CHROME6'),
                   ('RENDER_CHROME', 'BITMAP_CHROME'), ('RENDER_METAL', 'BITMAP_SHINY'))

# TextureScriptParsing::parsingTScriptA: the characters after the first '_' of the name, up to the
# first '.', at most four of them, must all be flags; otherwise the name has no flags at all.
SCRIPT_SEPARATOR = '_'
SCRIPT_MAX_LENGTH = 5  # std::min(5, strlen(token)) with the '_' at position 0
SCRIPT_NAME_BYTES = 32
FLAG_BRIGHT = 'R'
FLAG_HIDDEN = 'H'
FLAG_STREAM = 'S'
FLAG_NONE_BLEND = 'N'
FLAG_SHADOW = 'D'
SHADOW_KINDS = {'C': 'colour', 'T': 'texture'}
# Built with PJH_ADD_PANDA_CHANGERING (Core/Globals/Defined_Global.h): this one name is bright.
PANDA_BRIGHT_NAME = 'mu_rgb_lights.jpg'

HIDE_PREFIX = 'hid'
SKIN_PREFIX = 'ski'
SKIN_LEVEL_PREFIX = 'level'
HAIR_PREFIX = 'hair'
ALPHA_EXTENSION = 't'  # .tga / .ozt: loaded with an alpha channel

FULL_ALPHA = 0.99  # BMD::RenderMesh: alpha below this switches to the alpha-test state
OWN_TEXTURE = 'own'
ENGINE_TEXTURE_PREFIX = 'engine:'


@dataclass(frozen=True)
class TextureFlags:
    bright: bool = False
    hidden: bool = False
    stream: bool = False
    none_blend: bool = False
    shadow: str = ''

    def names(self):
        """The flags as render-facts.json lists them."""
        found = []
        for present, name in ((self.bright, 'bright'), (self.hidden, 'hidden'), (self.stream, 'stream'),
                              (self.none_blend, 'no-chrome')):
            if present:
                found.append(name)
        if self.shadow:
            found.append(f'shadow-{self.shadow}')
        return found


NO_FLAGS = TextureFlags()


def texture_flags(name):
    """The render flags a BMD texture name sets for its mesh, exactly like parsingTScriptA."""
    raw = name[:SCRIPT_NAME_BYTES]
    underscore = raw.find(SCRIPT_SEPARATOR)
    if underscore < 0:
        return NO_FLAGS
    token = raw[underscore:].split('.', 1)[0]
    length = min(SCRIPT_MAX_LENGTH, len(token))
    found = {}
    position = 1
    while position < length:
        char = token[position]
        if char == FLAG_BRIGHT:
            found['bright'] = True
        elif char == FLAG_HIDDEN:
            found['hidden'] = True
        elif char == FLAG_STREAM:
            found['stream'] = True
        elif char == FLAG_NONE_BLEND:
            found['none_blend'] = True
        elif char == FLAG_SHADOW and position + 1 < length and token[position + 1] in SHADOW_KINDS:
            found['shadow'] = SHADOW_KINDS[token[position + 1]]
            position += 1
        elif char == FLAG_SHADOW:
            return NO_FLAGS
        elif name == PANDA_BRIGHT_NAME:
            return TextureFlags(bright=True)
        else:
            return NO_FLAGS
        position += 1
    return TextureFlags(**found) if found else NO_FLAGS


@dataclass(frozen=True)
class MeshFacts:
    """One mesh of a BMD as the engine sees it."""
    index: int
    texture: str      # the name stored in the BMD
    slot: int = -1    # Mesh_t::Texture; -1 = same as index
    flags: TextureFlags = NO_FLAGS
    missing: bool = False  # no texture file in the model's folders

    @property
    def texture_slot(self):
        return self.index if self.slot < 0 else self.slot

    @property
    def never_drawn(self):
        return self.texture.startswith(HIDE_PREFIX)

    @property
    def has_alpha(self):
        extension = self.texture.rpartition('.')[2]
        return extension[:1].lower() == ALPHA_EXTENSION

    @property
    def skin_or_hair(self):
        lower = self.texture.lower()
        return (self.texture.startswith(SKIN_PREFIX) or lower.startswith(SKIN_LEVEL_PREFIX)
                or self.texture.startswith(HAIR_PREFIX))


def mesh_facts(index, texture, slot=-1, missing=False):
    return MeshFacts(index, texture, slot, texture_flags(texture), missing)


@dataclass
class Value:
    """An argument of a render call: known number, a known name, or unknown C++ text."""
    number: float = None
    text: str = ''

    @property
    def known(self):
        return self.number is not None


@dataclass
class DrawCall:
    """One BMD::RenderMesh (mesh set) or BMD::RenderBody (mesh None) call with evaluated arguments."""
    flags: frozenset
    blend: Value
    mesh: int = None
    alpha: Value = field(default_factory=lambda: Value(1.0))
    hidden: Value = field(default_factory=lambda: Value(-1))
    texture: str = ''        # an engine BITMAP_* the call binds instead of the mesh's own texture
    uv_animated: bool = False
    line: int = 0
    unknown: tuple = ()      # argument texts the evaluator could not read


@dataclass
class Pass:
    """How one call draws one mesh."""
    mesh: int
    mode: str
    texture: str      # 'own', 'engine:<BITMAP_...>' or 'none'
    line: int
    rule: str         # which BMD::RenderMesh branch decided it
    note: str = ''
    conditional: bool = False  # the call runs only under a runtime condition


def _has(flags, name):
    return name in flags


def _texture_of(call, chrome=False):
    if call.texture:
        return ENGINE_TEXTURE_PREFIX + call.texture
    if chrome:
        for flag, bitmap in CHROME_TEXTURES:
            if flag in call.flags:
                return ENGINE_TEXTURE_PREFIX + bitmap
        return 'none'
    return OWN_TEXTURE


def _alpha_mode(call, mesh):
    """EnableAlphaTest when the call's alpha is below 1 or the texture has alpha, else opaque. A call
    alpha below 1 (or one that changes over time) makes the whole mesh see-through: blended-alpha;
    a .tga at full alpha is cut out by its own alpha: alpha-test."""
    partial = call.alpha.known and call.alpha.number < FULL_ALPHA
    if partial or not call.alpha.known:
        return ALPHA_BLEND
    if mesh.has_alpha:
        return ALPHA_TEST
    return OPAQUE


def classify(call, mesh, blend, hide_skin):
    """The pass of `call` for `mesh` with the effective blend index (RenderBody may have replaced it
    for a bright mesh), following BMD::RenderMesh branch by branch. None when the mesh is not drawn."""
    if mesh.never_drawn:
        return None
    if hide_skin and mesh.skin_or_hair:
        return None
    flags = call.flags
    blend_matches = blend.known and (blend.number <= -2 or blend.number == mesh.texture_slot)
    if _has(flags, RENDER_COLOR):
        mode = ADDITIVE if _has(flags, RENDER_BRIGHT) else SUBTRACT if _has(flags, RENDER_DARK) else OPAQUE
        return Pass(mesh.index, mode, 'none', call.line, 'colour pass (untextured)')
    if any(flag in flags for flag in CHROME_FLAGS):
        if mesh.flags.none_blend:
            return None
        if any(flag in flags for flag in ADDITIVE_CHROME_FLAGS):
            mode = ADDITIVE
        elif _has(flags, RENDER_DARK):
            mode = SUBTRACT
        elif _has(flags, RENDER_LIGHTMAP):
            mode = 'lightmap'
        else:
            mode = OPAQUE if call.alpha.known and call.alpha.number >= FULL_ALPHA else ALPHA_TEST
        return Pass(mesh.index, mode, _texture_of(call, chrome=True), call.line, 'chrome/metal pass')
    if blend_matches:
        mode = SUBTRACT if _has(flags, RENDER_DARK) else ADDITIVE
        return Pass(mesh.index, mode, _texture_of(call), call.line, 'blend mesh (texture slot == BlendMesh)')
    if not blend.known:
        return Pass(mesh.index, UNKNOWN, _texture_of(call), call.line, 'blend mesh index unknown',
                    note=f'BlendMesh is {blend.text!r}')
    if _has(flags, RENDER_TEXTURE):
        if _has(flags, RENDER_BRIGHT):
            mode = ADDITIVE
        elif _has(flags, RENDER_DARK):
            mode = SUBTRACT
        else:
            mode = _alpha_mode(call, mesh)
        return Pass(mesh.index, mode, _texture_of(call), call.line, 'textured pass')
    if _has(flags, RENDER_BRIGHT):
        if mesh.has_alpha:
            return None
        return Pass(mesh.index, ADDITIVE, 'none', call.line, 'bright pass (untextured)')
    return Pass(mesh.index, UNKNOWN, _texture_of(call), call.line, 'no RENDER_TEXTURE flag')


def passes_of(call, meshes, hide_skin):
    """Every pass one call draws: RenderMesh draws its mesh; RenderBody draws every mesh but the
    hidden ones (_H names and the HiddenMesh argument), with a bright (_R) mesh as its own blend mesh."""
    if call.mesh is not None:
        if call.mesh < 0 or call.mesh >= len(meshes):
            return []
        found = classify(call, meshes[call.mesh], call.blend, hide_skin)
        return [found] if found else []
    found = []
    for mesh in meshes:
        if mesh.flags.hidden:
            continue
        if call.hidden.known and call.hidden.number == mesh.index:
            continue
        blend = Value(mesh.index) if mesh.flags.bright else call.blend
        drawn = classify(call, mesh, blend, hide_skin)
        if drawn:
            found.append(drawn)
    return found


def base_and_overlays(passes):
    """Per mesh: the pass that shows its own texture (the first one; else the first pass at all) is
    its base look, every other pass is an overlay the engine adds (chrome, glow layers)."""
    base = {}
    for drawn in passes:
        current = base.get(drawn.mesh)
        if current is None or (current.texture != OWN_TEXTURE and drawn.texture == OWN_TEXTURE):
            base[drawn.mesh] = drawn
    overlays = {}
    for drawn in passes:
        if base.get(drawn.mesh) is not drawn:
            overlays.setdefault(drawn.mesh, []).append(drawn)
    return base, overlays
