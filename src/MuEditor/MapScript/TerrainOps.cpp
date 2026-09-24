#include "TerrainOps.h"

#ifdef _EDITOR

#include "ScriptReader.h" // FormatNumber
#include "ValueNoise.h"

#include "Editing/FieldBrush.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace Editor::MapScript::Ops
{
namespace
{
using Editor::Editing::FloatField;

constexpr float HALF = 0.5f;
constexpr const char* NO_CORNER_WARNING = "the shape reaches no terrain corner; nothing changed (make it larger)";

// An object whose ground a terrain op may move, with the ground under it before.
struct Follower
{
    std::size_t index = 0;
    float groundBefore = 0.0f;
};

// The ground under an object is blended from the four corners of its tile, so an object
// on tile (x, y) reads corners x..x+1, y..y+1 (as the Height tab's followers).
bool StandsOn(const MapObject& object, const CellRect& corners)
{
    const int tileX = static_cast<int>(std::floor(object.state.position[0] / TILE_WORLD));
    const int tileY = static_cast<int>(std::floor(object.state.position[1] / TILE_WORLD));
    return tileX + 1 >= corners.minX && tileX <= corners.maxX && tileY + 1 >= corners.minY && tileY <= corners.maxY;
}

float GroundUnder(const MapState& state, const MapObject& object, float specialHeight)
{
    return GroundHeight(state.terrain, object.state.position[0], object.state.position[1], specialHeight);
}

std::vector<Follower> NoteFollowers(const MapState& state, const CellRect& corners, float specialHeight)
{
    std::vector<Follower> followers;
    for (std::size_t i = 0; i < state.objects.size(); ++i)
    {
        if (StandsOn(state.objects[i], corners))
            followers.push_back({i, GroundUnder(state, state.objects[i], specialHeight)});
    }
    return followers;
}

void MoveFollowers(const std::vector<Follower>& followers, MapState& state, float specialHeight)
{
    for (const Follower& follower : followers)
    {
        MapObject& object = state.objects[follower.index];
        object.state.position[2] += GroundUnder(state, object, specialHeight) - follower.groundBefore;
    }
}

Point ShapeMiddle(const Shape& shape)
{
    if (shape.kind == ShapeKind::Circle)
        return shape.center;
    const Bounds bounds = ShapeBounds(shape);
    return Point{(bounds.minX + bounds.maxX) * HALF, (bounds.minY + bounds.maxY) * HALF};
}

float GroundAt(const MapTerrain& terrain, Point point, float specialHeight)
{
    return GroundHeight(terrain, point.x * TILE_WORLD, point.y * TILE_WORLD, specialHeight);
}

// The weighted mean, the lowest or the highest height of the corners `mask` reaches.
float MaskStatistic(HeightReference reference, const WeightMask& mask, const MapTerrain& terrain)
{
    float weighted = 0.0f;
    float weights = 0.0f;
    float lowest = std::numeric_limits<float>::max();
    float highest = std::numeric_limits<float>::lowest();
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
    {
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
        {
            const float weight = mask.At(x, y);
            if (weight <= 0.0f)
                continue;
            const float height = terrain.height[Editor::MapInspect::CellIndex(x, y)];
            weighted += height * weight;
            weights += weight;
            lowest = std::min(lowest, height);
            highest = std::max(highest, height);
        }
    }
    if (weights <= 0.0f)
        return 0.0f;
    if (reference == HeightReference::Lowest)
        return lowest;
    if (reference == HeightReference::Highest)
        return highest;
    return weighted / weights;
}

bool HasWeight(const WeightMask& mask)
{
    return std::any_of(mask.weights.begin(), mask.weights.end(), [](float weight) { return weight > 0.0f; });
}

bool CheckHeight(const Op& op, const char* field, float height, const MapContext& context, std::string& error)
{
    if (height >= 0.0f && height <= context.maxHeight)
        return true;
    error = OpField(op, field) + ": the height comes to " + FormatNumber(height) + ", outside 0 to " +
            FormatNumber(context.maxHeight) + ", the heights this map's height file stores";
    return false;
}

// Arc length along the path to the point on it nearest `point`, as a share of the
// path's length (0 at its first point, 1 at its last).
float PathParameter(const std::vector<Point>& points, Point point)
{
    float total = 0.0f;
    float bestDistance = std::numeric_limits<float>::max();
    float bestAlong = 0.0f;
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        const Point& a = points[i];
        const Point& b = points[i + 1];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length = std::hypot(dx, dy);
        const float t = length > 0.0f
                            ? std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / (length * length), 0.0f, 1.0f)
                            : 0.0f;
        const float distance = std::hypot(point.x - (a.x + dx * t), point.y - (a.y + dy * t));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestAlong = total + length * t;
        }
        total += length;
    }
    return total > 0.0f ? bestAlong / total : 0.0f;
}

// Where along the ramp corner (x, y) lies, 0 at its start and 1 at its end.
float RampParameter(const TerrainEdit& edit, int x, int y)
{
    const CellRect& tiles = edit.shape.tiles;
    switch (edit.axis)
    {
    case RampAxis::X:
        return std::clamp((static_cast<float>(x) - tiles.minX) / static_cast<float>(tiles.Width()), 0.0f, 1.0f);
    case RampAxis::Y:
        return std::clamp((static_cast<float>(y) - tiles.minY) / static_cast<float>(tiles.Height()), 0.0f, 1.0f);
    case RampAxis::Path:
        break;
    }
    return PathParameter(edit.shape.points, Point{static_cast<float>(x), static_cast<float>(y)});
}

// The ramp's first and last point: the path's ends, or the middle of the rectangle's
// low and high edge along the axis.
void RampEnds(const TerrainEdit& edit, Point& start, Point& end)
{
    const CellRect& tiles = edit.shape.tiles;
    const float middleX = (tiles.minX + tiles.maxX + 1) * HALF;
    const float middleY = (tiles.minY + tiles.maxY + 1) * HALF;
    switch (edit.axis)
    {
    case RampAxis::X:
        start = Point{static_cast<float>(tiles.minX), middleY};
        end = Point{static_cast<float>(tiles.maxX + 1), middleY};
        return;
    case RampAxis::Y:
        start = Point{middleX, static_cast<float>(tiles.minY)};
        end = Point{middleX, static_cast<float>(tiles.maxY + 1)};
        return;
    case RampAxis::Path:
        start = edit.shape.points.front();
        end = edit.shape.points.back();
        return;
    }
}

float RampEndHeight(const HeightTarget& target, Point end, const TerrainEdit& edit, const WeightMask& mask,
                    const MapTerrain& terrain, float specialHeight)
{
    if (target.reference == HeightReference::Ground)
        return GroundAt(terrain, end, specialHeight) + target.value;
    return ResolveHeight(target, edit.shape, mask, terrain, specialHeight);
}

bool Ramp(const Op& op, const TerrainEdit& edit, const WeightMask& mask, const MapContext& context, MapState& state,
          std::string& error)
{
    Point start;
    Point end;
    RampEnds(edit, start, end);
    const float from = RampEndHeight(edit.from, start, edit, mask, state.terrain, context.specialHeight);
    const float to = RampEndHeight(edit.to, end, edit, mask, state.terrain, context.specialHeight);
    if (!CheckHeight(op, "from", from, context, error) || !CheckHeight(op, "to", to, context, error))
        return false;
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
    {
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
        {
            const float weight = mask.At(x, y) * edit.strength;
            if (weight <= 0.0f)
                continue;
            float& height = state.terrain.height[Editor::MapInspect::CellIndex(x, y)];
            const float target = from + (to - from) * RampParameter(edit, x, y);
            height += (target - height) * weight;
        }
    }
    return true;
}

void Noise(const TerrainEdit& edit, const WeightMask& mask, MapState& state)
{
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
    {
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
        {
            const float weight = mask.At(x, y);
            if (weight <= 0.0f)
                continue;
            const float noise =
                FractalNoise(static_cast<float>(x), static_cast<float>(y), edit.scale, edit.octaves, edit.seed);
            state.terrain.height[Editor::MapInspect::CellIndex(x, y)] += edit.amount * noise * weight;
        }
    }
}

bool MoveToTarget(const Op& op, const TerrainEdit& edit, const WeightMask& mask, const MapContext& context,
                  MapState& state, std::string& error)
{
    const float target = ResolveHeight(edit.target, edit.shape, mask, state.terrain, context.specialHeight);
    const char* field = op.kind == OpKind::TerrainSet ? "height" : "to";
    if (!CheckHeight(op, field, target, context, error))
        return false;
    Editor::Editing::MoveFieldToward(state.terrain.Heights(), mask, &target, edit.strength);
    return true;
}

bool Sculpt(const Op& op, const TerrainEdit& edit, const WeightMask& mask, const MapContext& context, MapState& state,
            std::string& error)
{
    const FloatField heights = state.terrain.Heights();
    switch (op.kind)
    {
    case OpKind::TerrainRaise:
    case OpKind::TerrainLower:
    {
        const float amount = op.kind == OpKind::TerrainLower ? -edit.amount : edit.amount;
        Editor::Editing::AddToField(heights, mask, &amount);
        return true;
    }
    case OpKind::TerrainFlatten:
    case OpKind::TerrainSet:
        return MoveToTarget(op, edit, mask, context, state, error);
    case OpKind::TerrainRamp:
        return Ramp(op, edit, mask, context, state, error);
    case OpKind::TerrainSmooth:
        for (int i = 0; i < edit.iterations; ++i)
            Editor::Editing::SmoothField(heights, mask, edit.strength);
        return true;
    case OpKind::TerrainNoise:
        Noise(edit, mask, state);
        return true;
    default:
        error = OpLabel(op) + ": is not a terrain op";
        return false;
    }
}
} // namespace

float ResolveHeight(const HeightTarget& target, const Shape& shape, const WeightMask& mask, const MapTerrain& terrain,
                    float specialHeight)
{
    switch (target.reference)
    {
    case HeightReference::Absolute:
        return target.value;
    case HeightReference::Average:
    case HeightReference::Lowest:
    case HeightReference::Highest:
        return MaskStatistic(target.reference, mask, terrain) + target.value;
    case HeightReference::Center:
    case HeightReference::Ground:
        return GroundAt(terrain, ShapeMiddle(shape), specialHeight) + target.value;
    }
    return target.value;
}

bool ApplyTerrain(const Op& op, const TerrainEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                  std::string& error)
{
    if (!context.heightsEditable)
    {
        error = OpLabel(op) + ": this map stores 24-bit heights, which edit scripts (and the height save) do not "
                              "write; leave its heights alone";
        return false;
    }
    const WeightMask mask = Rasterize(edit.shape, CellAnchor::Corner, EffectiveFalloff(edit.shape, EdgeDefault::Soft));
    if (!HasWeight(mask))
    {
        report.warnings.push_back(NO_CORNER_WARNING);
        return true;
    }
    std::vector<Follower> followers;
    if (edit.objectsFollow)
        followers = NoteFollowers(state, mask.rect, context.specialHeight);
    if (!Sculpt(op, edit, mask, context, state, error))
        return false;
    Editor::Editing::ClampField(state.terrain.Heights(), mask.rect, 0.0f, context.maxHeight);
    MoveFollowers(followers, state, context.specialHeight);
    report.area = mask.rect;
    return true;
}
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
