#pragma once

#ifdef _EDITOR

#include "ScriptShape.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// An edit script as parsed from its JSON ("schema": "mu-map-edit/1"): a label and a list
// of ops, applied in order and undone as one step. docs/agents/AI_MAP_EDITING.md is the
// reference an agent writes them from.
namespace Editor::MapScript
{
constexpr const char* SCRIPT_SCHEMA = "mu-map-edit/1";

// One object.scatter places at most this many objects (its count, or what its density
// comes to on its shape).
constexpr int MAX_SCATTER_OBJECTS = 5000;

enum class OpKind : std::uint8_t
{
    TerrainRaise,
    TerrainLower,
    TerrainFlatten,
    TerrainSet,
    TerrainRamp,
    TerrainSmooth,
    TerrainNoise,
    TexturePaint,
    TextureErase,
    AttributeSet,
    LightAdd,
    LightSubtract,
    LightTint,
    LightSet,
    LightSmooth,
    LightBake,
    ObjectPlace,
    ObjectScatter,
    ObjectMove,
    ObjectRotate,
    ObjectScale,
    ObjectDelete,
    ObjectDropToGround,
};

// A height an op aims at: an absolute height in world units, or one read from the
// ground inside the op's shape before the op runs (plus `value`).
enum class HeightReference : std::uint8_t
{
    Absolute,
    Average, // the mean of the corners the shape covers, weighted by the shape
    Lowest,
    Highest,
    Center, // the ground at the shape's centre (circle) or middle
    Ground, // ramp ends: the ground at that end of the path or rectangle
};

struct HeightTarget
{
    HeightReference reference = HeightReference::Absolute;
    float value = 0.0f; // the height (Absolute) or what is added to the reference
};

enum class RampAxis : std::uint8_t
{
    Path, // along the path, from its first point to its last
    X,    // across a rectangle, from its west edge (low x) to its east edge
    Y,    // from its south edge (low y) to its north edge
};

// terrain.*: raise/lower (amount), flatten/set (target), ramp (from, to, axis), smooth
// (iterations, strength), noise (amount = amplitude, scale, octaves, seed).
struct TerrainEdit
{
    Shape shape;
    float amount = 0.0f;
    HeightTarget target;
    HeightTarget from;
    HeightTarget to;
    RampAxis axis = RampAxis::Path;
    int iterations = 1;
    float strength = 1.0f;
    float scale = 0.0f;
    int octaves = 1;
    std::uint32_t seed = 0;
    bool objectsFollow = true; // objects on ground that moves keep their height above it
};

// A texture slot as the script names it: a number, or a name resolved on the loaded map.
struct TileChoice
{
    int slot = -1;
    std::string name;
};

// texture.paint (layer 1 or 2, tile, opacity and strength for layer 2) and
// texture.erase (layer 2's overlay faded out by strength).
struct TextureEdit
{
    Shape shape;
    int layer = 1;
    TileChoice tile;
    float opacity = 1.0f;
    float strength = 1.0f;
};

// light.bake's sun: the engine lights the ground from the south-east (ZzzLodTerrain.cpp,
// TerrainLightDirection: (0.5, -0.5, 0.5)), about 35 degrees high.
constexpr float DEFAULT_SUN_AZIMUTH = 135.0f;
constexpr float DEFAULT_SUN_ELEVATION = 35.0f;
constexpr float DEFAULT_RELIEF_CONTRAST = 1.0f;

// light.add, light.subtract, light.tint, light.set (colour, strength), light.smooth
// (iterations, strength) and light.bake (the sun's azimuth and elevation in degrees, and
// how strongly slopes change the light).
struct LightEdit
{
    Shape shape;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float strength = 1.0f;
    int iterations = 1;
    float azimuth = DEFAULT_SUN_AZIMUTH;     // compass heading the sun shines from: 0 north, 90 east
    float elevation = DEFAULT_SUN_ELEVATION; // degrees above the horizon
    float contrast = DEFAULT_RELIEF_CONTRAST;
};

// A model as the script names it: its catalog or model name, or its type number.
struct ModelChoice
{
    std::string name;
    int type = -1;
    float weight = 1.0f;
};

// How high an object stands.
enum class HeightMode : std::uint8_t
{
    Ground,   // on the ground
    Offset,   // `value` above the ground
    Absolute, // at height `value`
    Keep,     // as high above the ground as before the edit (moves)
};

struct ObjectHeight
{
    HeightMode mode = HeightMode::Ground;
    float value = 0.0f;
};

// object.place.
struct PlaceEdit
{
    ModelChoice model;
    Point at;
    ObjectHeight height;
    float angle[3] = {};
    float scale = 1.0f;
};

// Where object.scatter must not put an object.
struct AvoidRules
{
    std::vector<std::uint16_t> attributes; // walkability values (Attributes::Matches)
    std::vector<TileChoice> textures;      // base texture slots
    std::optional<float> maxSlope;         // degrees
    std::optional<float> objectDistance;   // tiles from any object the map has
    std::vector<Shape> areas;              // roads (paths) and other shapes to keep out of
};

// object.scatter.
struct ScatterEdit
{
    std::vector<ModelChoice> models;
    Shape shape;
    int count = 0;
    float density = 0.0f; // objects per tile when no count is given
    float minSpacing = 0.0f;
    std::uint64_t seed = 0;
    float scaleRange[2] = {1.0f, 1.0f};
    float yawRange[2] = {0.0f, 360.0f};
    AvoidRules avoid;
    std::optional<std::uint16_t> markAttribute; // written on each placed object's tile
};

// Which objects an edit touches: all criteria given must hold.
struct ObjectSelector
{
    std::vector<int> ids;
    std::vector<ModelChoice> models;
    std::optional<Shape> inside;
    std::optional<int> placedBy; // the op of this script that placed them
};

// attribute.set: the tiles whose centre lies inside `shape`, or (with `under`) the tiles
// the selected objects stand on, as a scatter's mark_attribute marks them.
struct AttributeEdit
{
    Shape shape;
    std::optional<ObjectSelector> under;
    std::uint16_t value = 0;
};

// Where object.rotate turns a group around.
enum class RotatePivot : std::uint8_t
{
    Each,   // every object turns on its own spot
    Center, // the group turns around its middle, as the gizmo does
};

// object.move (by or to, height), object.rotate (degrees, absolute, pivot),
// object.scale (factor or absolute), object.delete and object.drop_to_ground.
struct ObjectEdit
{
    ObjectSelector select;
    std::optional<Point> by;
    std::optional<Point> to;
    ObjectHeight height{HeightMode::Keep, 0.0f};
    float degrees = 0.0f;
    bool absolute = false; // rotate/scale: `degrees` / `factor` is the new value, not a change
    RotatePivot pivot = RotatePivot::Each;
    float factor = 1.0f;
};

using OpData = std::variant<TerrainEdit, TextureEdit, AttributeEdit, LightEdit, PlaceEdit, ScatterEdit, ObjectEdit>;

struct Op
{
    int index = 0; // its place in the script's "ops" list
    OpKind kind = OpKind::TerrainRaise;
    OpData data;
};

struct EditScript
{
    std::string label; // what the undo step is called; empty: a summary of the ops
    std::vector<Op> ops;
};

// "terrain.raise" ... "object.drop_to_ground".
const char* OpName(OpKind kind);
bool OpKindFromName(const std::string& name, OpKind& kind);
// Every op name, comma separated, for error messages.
std::string OpNames();

// "ops[3] (terrain.set)" and "ops[3].height", for errors about an op or one of its fields.
std::string OpLabel(const Op& op);
std::string OpField(const Op& op, const std::string& field);

// The undo step's label: the script's own, or "Script: terrain.raise, object.scatter"
// (the distinct op names in order, shortened after a few).
std::string StepLabel(const EditScript& script);
} // namespace Editor::MapScript

#endif // _EDITOR
