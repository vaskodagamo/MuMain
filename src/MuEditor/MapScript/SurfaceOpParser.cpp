#include "SurfaceOpParser.h"

#ifdef _EDITOR

#include "ObjectOpParser.h" // SelectorAt
#include "ShapeParser.h"

#include <limits>

namespace Editor::MapScript::Parse
{
namespace
{
constexpr float MIN_AMOUNT = 0.01f;
constexpr int MAX_SEED = std::numeric_limits<int>::max();
constexpr int OVERLAY_LAYER = 2;
constexpr float FULL_CIRCLE = 360.0f;

bool ReadFollow(const ScriptReader& reader, TerrainEdit& edit)
{
    return reader.OptionalBool("objects_follow", edit.objectsFollow);
}

bool ReadStrength(const ScriptReader& reader, float& strength)
{
    return reader.OptionalNumber("strength", 0.0f, 1.0f, strength);
}

bool RaiseOrLower(const ScriptReader& reader, TerrainEdit& edit)
{
    return reader.OnlyKeys({"op", "shape", "amount", "objects_follow"}) && ShapeAt(reader, "shape", edit.shape) &&
           reader.Number("amount", MIN_AMOUNT, MAX_HEIGHT_VALUE, edit.amount) && ReadFollow(reader, edit);
}

bool Flatten(const ScriptReader& reader, TerrainEdit& edit)
{
    if (!reader.OnlyKeys({"op", "shape", "to", "strength", "objects_follow"}) || !ShapeAt(reader, "shape", edit.shape))
        return false;
    edit.target = HeightTarget{HeightReference::Average, 0.0f};
    if (reader.Has("to") && !HeightTargetAt(reader, "to", false, edit.target))
        return false;
    return ReadStrength(reader, edit.strength) && ReadFollow(reader, edit);
}

bool Set(const ScriptReader& reader, TerrainEdit& edit)
{
    return reader.OnlyKeys({"op", "shape", "height", "strength", "objects_follow"}) &&
           ShapeAt(reader, "shape", edit.shape) && HeightTargetAt(reader, "height", false, edit.target) &&
           ReadStrength(reader, edit.strength) && ReadFollow(reader, edit);
}

bool RampAxisOf(const ScriptReader& reader, TerrainEdit& edit)
{
    std::string axis;
    if (!reader.OptionalText("axis", axis))
        return false;
    if (edit.shape.kind == ShapeKind::Path)
    {
        edit.axis = RampAxis::Path;
        return axis.empty() || axis == "path" || reader.Fail("axis", "a ramp on a path runs along it; leave it out");
    }
    if (edit.shape.kind != ShapeKind::Rect)
        return reader.Fail("shape", "a ramp runs along a \"path\" or across a \"rect\"");
    if (axis == "x" || axis == "y")
    {
        edit.axis = axis == "x" ? RampAxis::X : RampAxis::Y;
        return true;
    }
    return reader.Fail("axis", "is \"x\" (west to east) or \"y\" (south to north) for a rectangle");
}

bool Ramp(const ScriptReader& reader, TerrainEdit& edit)
{
    return reader.OnlyKeys({"op", "shape", "from", "to", "axis", "strength", "objects_follow"}) &&
           ShapeAt(reader, "shape", edit.shape) && RampAxisOf(reader, edit) &&
           HeightTargetAt(reader, "from", true, edit.from) && HeightTargetAt(reader, "to", true, edit.to) &&
           ReadStrength(reader, edit.strength) && ReadFollow(reader, edit);
}

bool Smooth(const ScriptReader& reader, TerrainEdit& edit)
{
    return reader.OnlyKeys({"op", "shape", "iterations", "strength", "objects_follow"}) &&
           ShapeAt(reader, "shape", edit.shape) &&
           reader.OptionalInteger("iterations", 1, MAX_ITERATIONS, edit.iterations) &&
           ReadStrength(reader, edit.strength) && ReadFollow(reader, edit);
}

bool Noise(const ScriptReader& reader, TerrainEdit& edit)
{
    edit.scale = DEFAULT_NOISE_SCALE;
    edit.octaves = DEFAULT_OCTAVES;
    int seed = DEFAULT_SEED;
    const bool valid = reader.OnlyKeys({"op", "shape", "amplitude", "scale", "octaves", "seed", "objects_follow"}) &&
                       ShapeAt(reader, "shape", edit.shape) &&
                       reader.Number("amplitude", MIN_AMOUNT, MAX_HEIGHT_VALUE, edit.amount) &&
                       reader.OptionalNumber("scale", MIN_NOISE_SCALE, MAX_NOISE_SCALE, edit.scale) &&
                       reader.OptionalInteger("octaves", 1, MAX_OCTAVES, edit.octaves) &&
                       reader.OptionalInteger("seed", 0, MAX_SEED, seed) && ReadFollow(reader, edit);
    edit.seed = static_cast<std::uint32_t>(seed);
    return valid;
}
bool Bake(const ScriptReader& reader, LightEdit& edit)
{
    return reader.OnlyKeys({"op", "shape", "azimuth", "elevation", "contrast"}) &&
           ShapeAt(reader, "shape", edit.shape) &&
           reader.OptionalNumber("azimuth", -FULL_CIRCLE, FULL_CIRCLE, edit.azimuth) &&
           reader.OptionalNumber("elevation", MIN_SUN_ELEVATION, MAX_SUN_ELEVATION, edit.elevation) &&
           reader.OptionalNumber("contrast", MIN_RELIEF_CONTRAST, MAX_RELIEF_CONTRAST, edit.contrast);
}
} // namespace

bool TerrainOp(const ScriptReader& reader, OpKind kind, TerrainEdit& edit)
{
    switch (kind)
    {
    case OpKind::TerrainRaise:
    case OpKind::TerrainLower:
        return RaiseOrLower(reader, edit);
    case OpKind::TerrainFlatten:
        return Flatten(reader, edit);
    case OpKind::TerrainSet:
        return Set(reader, edit);
    case OpKind::TerrainRamp:
        return Ramp(reader, edit);
    case OpKind::TerrainSmooth:
        return Smooth(reader, edit);
    case OpKind::TerrainNoise:
        return Noise(reader, edit);
    default:
        return reader.Fail("op", "is not a terrain op");
    }
}

bool TextureOp(const ScriptReader& reader, OpKind kind, TextureEdit& edit)
{
    if (kind == OpKind::TextureErase)
    {
        edit.layer = OVERLAY_LAYER;
        return reader.OnlyKeys({"op", "shape", "strength"}) && ShapeAt(reader, "shape", edit.shape) &&
               ReadStrength(reader, edit.strength);
    }
    if (!reader.OnlyKeys({"op", "shape", "layer", "tile", "opacity", "strength"}) ||
        !ShapeAt(reader, "shape", edit.shape) || !reader.OptionalInteger("layer", 1, OVERLAY_LAYER, edit.layer) ||
        !TileAt(reader, "tile", edit.tile))
        return false;
    const bool overlayOnly = reader.Has("opacity") || reader.Has("strength");
    if (edit.layer != OVERLAY_LAYER && overlayOnly)
        return reader.Fail("layer", "\"opacity\" and \"strength\" blend layer 2; layer 1 has one texture per tile");
    return reader.OptionalNumber("opacity", 0.0f, 1.0f, edit.opacity) && ReadStrength(reader, edit.strength);
}

bool AttributeOp(const ScriptReader& reader, AttributeEdit& edit)
{
    if (!reader.OnlyKeys({"op", "shape", "under", "value"}) || !AttributeAt(reader, "value", edit.value))
        return false;
    if (reader.Has("shape") == reader.Has("under"))
        return reader.Fail("shape", "give \"shape\" (the tiles inside it) or \"under\" (an object selector: the tiles "
                                    "the objects stand on), one of them");
    if (reader.Has("shape"))
        return ShapeAt(reader, "shape", edit.shape);
    ObjectSelector under;
    if (!SelectorAt(reader, "under", under))
        return false;
    edit.under = std::move(under);
    return true;
}

bool LightOp(const ScriptReader& reader, OpKind kind, LightEdit& edit)
{
    if (kind == OpKind::LightBake)
        return Bake(reader, edit);
    if (kind == OpKind::LightSmooth)
        return reader.OnlyKeys({"op", "shape", "iterations", "strength"}) && ShapeAt(reader, "shape", edit.shape) &&
               reader.OptionalInteger("iterations", 1, MAX_ITERATIONS, edit.iterations) &&
               ReadStrength(reader, edit.strength);

    const bool additive = kind == OpKind::LightAdd || kind == OpKind::LightSubtract;
    edit.strength =
        additive ? DEFAULT_LIGHT_ADD_STRENGTH : (kind == OpKind::LightTint ? DEFAULT_LIGHT_TINT_STRENGTH : 1.0f);
    if (!reader.OnlyKeys({"op", "shape", "color", "strength"}) || !ShapeAt(reader, "shape", edit.shape))
        return false;
    if (additive && !reader.Has("color"))
        return ReadStrength(reader, edit.strength);
    return reader.Triple("color", 0.0f, 1.0f, edit.color) && ReadStrength(reader, edit.strength);
}
} // namespace Editor::MapScript::Parse

#endif // _EDITOR
