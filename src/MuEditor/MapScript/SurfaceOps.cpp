#include "SurfaceOps.h"

#ifdef _EDITOR

#include "AttributeRules.h"
#include "ObjectOps.h" // MarkUnderObjects

#include "Editing/FieldBrush.h"
#include "Editing/SurfaceBrush.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Editor::MapScript::Ops
{
namespace
{
constexpr int LIGHT_CHANNELS = 3;
constexpr int OVERLAY_LAYER = 2;
constexpr const char* NO_TILE_WARNING = "the shape covers no tile centre; nothing changed (make it larger)";
constexpr const char* NO_CORNER_WARNING = "the shape reaches no terrain corner; nothing changed (make it larger)";
constexpr float DEGREES_TO_RADIANS = std::numbers::pi_v<float> / 180.0f;

// Where light.bake's sun shines from: a unit vector towards it.
struct Sun
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.0f;
};

// From a compass heading (0 north, +y; 90 east, +x) and a height above the horizon.
Sun SunFrom(float azimuthDegrees, float elevationDegrees)
{
    const float azimuth = azimuthDegrees * DEGREES_TO_RADIANS;
    const float elevation = elevationDegrees * DEGREES_TO_RADIANS;
    return Sun{std::sin(azimuth) * std::cos(elevation), std::cos(azimuth) * std::cos(elevation), std::sin(elevation)};
}

// How light.bake changes the light of corner (x, y): 1 on flat ground, more where the
// slope faces the sun and less where it turns away. Lambert's cosine between the ground
// and the sun, taken against flat ground's, scaled by `contrast`.
float ReliefFactor(const MapTerrain& terrain, int x, int y, const Sun& sun, float contrast)
{
    const Normal normal = CornerNormal(terrain, x, y);
    const float facing = std::max(0.0f, normal.x * sun.x + normal.y * sun.y + normal.z * sun.z);
    return std::max(0.0f, 1.0f + contrast * (facing - sun.z));
}

void ScaleCellLight(std::vector<float>& light, int x, int y, float scale)
{
    const std::size_t first = Editor::MapInspect::CellIndex(x, y) * LIGHT_CHANNELS;
    for (int c = 0; c < LIGHT_CHANNELS; ++c)
        light[first + c] *= scale;
}

// light.bake: relief shading from the heights, faded in by the shape's weight.
void BakeLight(const LightEdit& edit, const WeightMask& mask, MapState& state)
{
    const Sun sun = SunFrom(edit.azimuth, edit.elevation);
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
    {
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
        {
            const float weight = mask.At(x, y);
            if (weight <= 0.0f)
                continue;
            const float factor = ReliefFactor(state.terrain, x, y, sun, edit.contrast);
            ScaleCellLight(state.terrain.light, x, y, 1.0f + weight * (factor - 1.0f));
        }
    }
}

bool HasWeight(const WeightMask& mask)
{
    return std::any_of(mask.weights.begin(), mask.weights.end(), [](float weight) { return weight > 0.0f; });
}

// Calls visit(x, y) for every tile a hard-edged op acts on.
template <typename Visit> int ForEachHardTile(const WeightMask& mask, Visit visit)
{
    int tiles = 0;
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
    {
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
        {
            if (mask.At(x, y) < HARD_EDGE_WEIGHT)
                continue;
            visit(x, y);
            ++tiles;
        }
    }
    return tiles;
}

WeightMask HardTiles(const Shape& shape)
{
    return Rasterize(shape, CellAnchor::TileCentre, EffectiveFalloff(shape, EdgeDefault::Hard));
}

WeightMask SoftCorners(const Shape& shape)
{
    return Rasterize(shape, CellAnchor::Corner, EffectiveFalloff(shape, EdgeDefault::Soft));
}

void PaintBase(const TextureEdit& edit, const WeightMask& mask, MapState& state)
{
    const auto slot = static_cast<std::uint8_t>(edit.tile.slot);
    ForEachHardTile(mask, [&state, slot](int x, int y)
                    { state.terrain.baseTiles[Editor::MapInspect::CellIndex(x, y)] = slot; });
}

void LightField(const Op& op, const LightEdit& edit, const WeightMask& mask, MapState& state)
{
    const Editor::Editing::FloatField light = state.terrain.Light();
    float amount[LIGHT_CHANNELS] = {};
    for (int c = 0; c < LIGHT_CHANNELS; ++c)
        amount[c] = edit.color[c] * edit.strength * (op.kind == OpKind::LightSubtract ? -1.0f : 1.0f);
    switch (op.kind)
    {
    case OpKind::LightAdd:
    case OpKind::LightSubtract:
        Editor::Editing::AddToField(light, mask, amount);
        return;
    case OpKind::LightTint:
    case OpKind::LightSet:
        Editor::Editing::MoveFieldToward(light, mask, edit.color, edit.strength);
        return;
    case OpKind::LightSmooth:
        for (int i = 0; i < edit.iterations; ++i)
            Editor::Editing::SmoothField(light, mask, edit.strength);
        return;
    case OpKind::LightBake:
        BakeLight(edit, mask, state);
        return;
    default:
        return;
    }
}
} // namespace

bool SentinelAllows(const Op& op, const MapContext& context, int x, int y, std::uint16_t value, std::string& error)
{
    const std::optional<Attributes::SentinelTile> sentinel = Attributes::SentinelFor(context.mapIndex);
    if (!sentinel || sentinel->x != x || sentinel->y != y || sentinel->value == value)
        return true;
    error = OpLabel(op) + ": would set tile (" + std::to_string(x) + ", " + std::to_string(y) + ") to " +
            Attributes::NameOf(value) + ", but this map's walkability file must keep it at " +
            std::to_string(sentinel->value) +
            " (the client's anti-tamper check closes the game otherwise, MAP_EDITOR.md gotcha 13); leave that tile "
            "out of the shape";
    return false;
}

bool ApplyTexture(const Op& op, const TextureEdit& edit, MapState& state, OpReport& report, std::string& error)
{
    const bool baseLayer = op.kind == OpKind::TexturePaint && edit.layer != OVERLAY_LAYER;
    const WeightMask mask = baseLayer ? HardTiles(edit.shape) : SoftCorners(edit.shape);
    if (!HasWeight(mask))
    {
        report.warnings.push_back(baseLayer ? NO_TILE_WARNING : NO_CORNER_WARNING);
        return true;
    }
    report.area = mask.rect;
    if (baseLayer)
    {
        PaintBase(edit, mask, state);
        return true;
    }
    const Editor::Editing::OverlayLayer overlay = state.terrain.Overlay();
    if (op.kind == OpKind::TextureErase)
    {
        report.area = Editor::Editing::EraseOverlay(overlay, mask, edit.strength);
        return true;
    }
    if (edit.tile.slot < 0)
    {
        error = OpField(op, "tile") + ": no texture slot";
        return false;
    }
    report.area = Editor::Editing::PaintOverlay(overlay, mask, static_cast<std::uint8_t>(edit.tile.slot), edit.opacity,
                                                edit.strength);
    return true;
}

bool ApplyAttribute(const Op& op, const AttributeEdit& edit, const MapContext& context, MapState& state,
                    OpReport& report, std::string& error)
{
    if (edit.under)
        return MarkUnderObjects(op, edit, context, state, report, error);
    const WeightMask mask = HardTiles(edit.shape);
    bool allowed = true;
    ForEachHardTile(mask,
                    [&](int x, int y) { allowed = allowed && SentinelAllows(op, context, x, y, edit.value, error); });
    if (!allowed)
        return false;
    const int tiles = ForEachHardTile(mask, [&state, &edit](int x, int y)
                                      { state.terrain.attribute[Editor::MapInspect::CellIndex(x, y)] = edit.value; });
    if (tiles == 0)
    {
        report.warnings.push_back(NO_TILE_WARNING);
        return true;
    }
    report.area = mask.rect;
    return true;
}

bool ApplyLight(const Op& op, const LightEdit& edit, MapState& state, OpReport& report, std::string&)
{
    const WeightMask mask = SoftCorners(edit.shape);
    if (!HasWeight(mask))
    {
        report.warnings.push_back(NO_CORNER_WARNING);
        return true;
    }
    LightField(op, edit, mask, state);
    Editor::Editing::ClampField(state.terrain.Light(), mask.rect, 0.0f, 1.0f);
    report.area = mask.rect;
    return true;
}
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
