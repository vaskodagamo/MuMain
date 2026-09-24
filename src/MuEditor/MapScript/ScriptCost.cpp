#include "ScriptCost.h"

#ifdef _EDITOR

#include "ScriptShape.h"

#include <algorithm>
#include <variant>

namespace Editor::MapScript
{
namespace
{
// A polygon's inner radius is searched on a grid of quarter tiles, at most 257 points a
// side (ScriptShape.cpp, PolygonInnerRadius).
constexpr float INNER_RADIUS_STEPS_PER_TILE = 4.0f;
constexpr float INNER_RADIUS_MAX_STEPS = 256.0f;
constexpr std::uint64_t UNITS_PER_SECOND = MAX_SCRIPT_COST * 2 / 3;
constexpr std::uint64_t HUNDREDTHS = 100;

std::uint64_t Cells(const Shape& shape)
{
    const CellRect cells = Footprint(shape, CellAnchor::Corner);
    return cells.IsEmpty() ? 0 : static_cast<std::uint64_t>(cells.Width()) * static_cast<std::uint64_t>(cells.Height());
}

std::uint64_t Segments(const Shape& shape)
{
    if (shape.kind == ShapeKind::Path)
        return std::max<std::uint64_t>(shape.points.size(), 2) - 1;
    if (shape.kind == ShapeKind::Polygon)
        return std::max<std::uint64_t>(shape.points.size(), 1);
    return 1;
}

// The points a polygon's search for its deepest point tests.
std::uint64_t InnerRadiusSamples(const Shape& shape)
{
    const Bounds bounds = ShapeBounds(shape);
    const float span = std::max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY);
    const auto perSide =
        static_cast<std::uint64_t>(std::min(span * INNER_RADIUS_STEPS_PER_TILE, INNER_RADIUS_MAX_STEPS)) + 1;
    return perSide * perSide;
}

// Rasterizing the shape, and a polygon's search for its deepest point when its soft edge
// is not given.
std::uint64_t ShapeCost(const Shape& shape)
{
    const std::uint64_t segments = Segments(shape);
    const bool searchesInnerRadius = shape.kind == ShapeKind::Polygon && !shape.falloff.has_value();
    const std::uint64_t innerRadius = searchesInnerRadius ? InnerRadiusSamples(shape) * segments : 0;
    return Cells(shape) * segments + innerRadius;
}

// Per-cell passes after the shape: smoothing iterations and noise octaves.
std::uint64_t PassCost(const Op& op, const Shape& shape)
{
    const std::uint64_t cells = Cells(shape);
    if (const auto* terrain = std::get_if<TerrainEdit>(&op.data))
    {
        if (op.kind == OpKind::TerrainSmooth)
            return cells * static_cast<std::uint64_t>(terrain->iterations);
        if (op.kind == OpKind::TerrainNoise)
            return cells * static_cast<std::uint64_t>(terrain->octaves);
    }
    if (const auto* light = std::get_if<LightEdit>(&op.data); light != nullptr && op.kind == OpKind::LightSmooth)
        return cells * static_cast<std::uint64_t>(light->iterations);
    return cells;
}

const Shape* ShapeOf(const Op& op)
{
    if (const auto* terrain = std::get_if<TerrainEdit>(&op.data))
        return &terrain->shape;
    if (const auto* texture = std::get_if<TextureEdit>(&op.data))
        return &texture->shape;
    if (const auto* attribute = std::get_if<AttributeEdit>(&op.data))
        return attribute->under ? nullptr : &attribute->shape;
    if (const auto* light = std::get_if<LightEdit>(&op.data))
        return &light->shape;
    if (const auto* scatter = std::get_if<ScatterEdit>(&op.data))
        return &scatter->shape;
    if (const auto* edit = std::get_if<ObjectEdit>(&op.data); edit != nullptr && edit->select.inside)
        return &*edit->select.inside;
    return nullptr;
}

std::string Seconds(std::uint64_t units)
{
    const std::uint64_t hundredths = units * HUNDREDTHS / UNITS_PER_SECOND;
    const std::uint64_t fraction = hundredths % HUNDREDTHS;
    return std::to_string(hundredths / HUNDREDTHS) + (fraction < 10 ? ".0" : ".") + std::to_string(fraction) + " s";
}
} // namespace

std::uint64_t OpCost(const Op& op)
{
    const Shape* shape = ShapeOf(op);
    return shape == nullptr ? 0 : ShapeCost(*shape) + PassCost(op, *shape);
}

bool CheckScriptCost(const EditScript& script, std::string& error)
{
    std::uint64_t total = 0;
    const Op* costliest = nullptr;
    std::uint64_t costliestUnits = 0;
    for (const Op& op : script.ops)
    {
        const std::uint64_t units = OpCost(op);
        total += units;
        if (costliest == nullptr || units > costliestUnits)
        {
            costliest = &op;
            costliestUnits = units;
        }
    }
    if (total <= MAX_SCRIPT_COST)
        return true;
    error = "the script would hold the client for about " + Seconds(total) + " (at most " + Seconds(MAX_SCRIPT_COST) +
            " at once); " + OpLabel(*costliest) + " alone takes about " + Seconds(costliestUnits) +
            ". Split it into several scripts, or use smaller shapes or paths and polygons with fewer points";
    return false;
}
} // namespace Editor::MapScript

#endif // _EDITOR
