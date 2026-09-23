#include "ScriptShape.h"

#ifdef _EDITOR

#include "MapInspect/TerrainView.h" // MAP_TILES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace Editor::MapScript
{
namespace
{
constexpr float MAP_EXTENT = static_cast<float>(Editor::MapInspect::MAP_TILES);
constexpr int LAST_CELL = Editor::MapInspect::MAP_TILES - 1;
constexpr float TILE_CENTRE_OFFSET = 0.5f;
constexpr float HALF = 0.5f;
// A polygon's inner radius is searched on a grid this fine (in tiles), coarser on
// polygons wider than this many steps.
constexpr float POLYGON_SEARCH_STEP = 0.25f;
constexpr float POLYGON_SEARCH_STEPS = 256.0f;

float AnchorOffset(CellAnchor anchor)
{
    return anchor == CellAnchor::TileCentre ? TILE_CENTRE_OFFSET : 0.0f;
}

float Distance(Point a, Point b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

float SegmentDistance(Point point, Point a, Point b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0f)
        return Distance(point, a);
    const float t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared, 0.0f, 1.0f);
    return Distance(point, Point{a.x + dx * t, a.y + dy * t});
}

// The distance to the nearest segment of the line through `points` (closed when
// `closed`).
float PolylineDistance(const std::vector<Point>& points, Point point, bool closed)
{
    float nearest = std::numeric_limits<float>::max();
    const std::size_t count = points.size();
    const std::size_t segments = closed ? count : count - 1;
    for (std::size_t i = 0; i < segments && count > 1; ++i)
        nearest = std::min(nearest, SegmentDistance(point, points[i], points[(i + 1) % count]));
    return nearest;
}

// Even-odd rule: a ray towards +x crosses the outline an odd number of times.
bool InsidePolygon(const std::vector<Point>& points, Point point)
{
    bool inside = false;
    const std::size_t count = points.size();
    for (std::size_t i = 0, j = count - 1; i < count; j = i++)
    {
        const Point& a = points[i];
        const Point& b = points[j];
        const bool spans = (a.y > point.y) != (b.y > point.y);
        if (spans && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}

float RectInsideDistance(const CellRect& tiles, Point point)
{
    const float minX = static_cast<float>(tiles.minX);
    const float minY = static_cast<float>(tiles.minY);
    const float maxX = static_cast<float>(tiles.maxX + 1);
    const float maxY = static_cast<float>(tiles.maxY + 1);
    return std::min(std::min(point.x - minX, maxX - point.x), std::min(point.y - minY, maxY - point.y));
}

Bounds PointBounds(const std::vector<Point>& points, float margin)
{
    if (points.empty())
        return {};
    Bounds bounds{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    for (const Point& point : points)
    {
        bounds.minX = std::min(bounds.minX, point.x - margin);
        bounds.minY = std::min(bounds.minY, point.y - margin);
        bounds.maxX = std::max(bounds.maxX, point.x + margin);
        bounds.maxY = std::max(bounds.maxY, point.y + margin);
    }
    return bounds;
}

Bounds UnclippedBounds(const Shape& shape)
{
    switch (shape.kind)
    {
    case ShapeKind::Circle:
        return {shape.center.x - shape.radius, shape.center.y - shape.radius, shape.center.x + shape.radius,
                shape.center.y + shape.radius};
    case ShapeKind::Rect:
        return {static_cast<float>(shape.tiles.minX), static_cast<float>(shape.tiles.minY),
                static_cast<float>(shape.tiles.maxX + 1), static_cast<float>(shape.tiles.maxY + 1)};
    case ShapeKind::Polygon:
        return PointBounds(shape.points, 0.0f);
    case ShapeKind::Path:
        return PointBounds(shape.points, shape.width * HALF);
    }
    return {};
}

float PolygonInnerRadius(const Shape& shape)
{
    const Bounds bounds = UnclippedBounds(shape);
    const float span = std::max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY);
    const float step = std::max(POLYGON_SEARCH_STEP, span / POLYGON_SEARCH_STEPS);
    float deepest = 0.0f;
    for (float y = bounds.minY; y <= bounds.maxY; y += step)
    {
        for (float x = bounds.minX; x <= bounds.maxX; x += step)
            deepest = std::max(deepest, InsideDistance(shape, Point{x, y}));
    }
    return deepest;
}
} // namespace

float InsideDistance(const Shape& shape, Point point)
{
    switch (shape.kind)
    {
    case ShapeKind::Circle:
        return shape.radius - Distance(point, shape.center);
    case ShapeKind::Rect:
        return RectInsideDistance(shape.tiles, point);
    case ShapeKind::Polygon:
    {
        if (shape.points.size() < 3)
            return -std::numeric_limits<float>::max();
        const float edge = PolylineDistance(shape.points, point, true);
        return InsidePolygon(shape.points, point) ? edge : -edge;
    }
    case ShapeKind::Path:
        if (shape.points.size() < 2)
            return -std::numeric_limits<float>::max();
        return shape.width * HALF - PolylineDistance(shape.points, point, false);
    }
    return -std::numeric_limits<float>::max();
}

float InnerRadius(const Shape& shape)
{
    switch (shape.kind)
    {
    case ShapeKind::Circle:
        return shape.radius;
    case ShapeKind::Rect:
        return static_cast<float>(std::min(shape.tiles.Width(), shape.tiles.Height())) * HALF;
    case ShapeKind::Polygon:
        return PolygonInnerRadius(shape);
    case ShapeKind::Path:
        return shape.width * HALF;
    }
    return 0.0f;
}

float EffectiveFalloff(const Shape& shape, EdgeDefault edge)
{
    if (shape.falloff.has_value())
        return std::max(*shape.falloff, 0.0f);
    if (edge == EdgeDefault::Hard)
        return 0.0f;
    return InnerRadius(shape) * DEFAULT_SOFT_SHARE;
}

float EdgeWeight(float insideDistance, float falloff)
{
    if (falloff <= 0.0f)
        return insideDistance >= 0.0f ? 1.0f : 0.0f;
    if (insideDistance <= 0.0f)
        return 0.0f;
    if (insideDistance >= falloff)
        return 1.0f;
    const float t = insideDistance / falloff;
    return t * t * (3.0f - 2.0f * t);
}

float WeightAt(const Shape& shape, Point point, float falloff)
{
    return EdgeWeight(InsideDistance(shape, point), falloff);
}

Bounds ShapeBounds(const Shape& shape)
{
    Bounds bounds = UnclippedBounds(shape);
    bounds.minX = std::clamp(bounds.minX, 0.0f, MAP_EXTENT);
    bounds.minY = std::clamp(bounds.minY, 0.0f, MAP_EXTENT);
    bounds.maxX = std::clamp(bounds.maxX, 0.0f, MAP_EXTENT);
    bounds.maxY = std::clamp(bounds.maxY, 0.0f, MAP_EXTENT);
    return bounds;
}

CellRect Footprint(const Shape& shape, CellAnchor anchor)
{
    // One cell of slack on each side keeps the conversions to int in range for any shape.
    Bounds bounds = UnclippedBounds(shape);
    bounds.minX = std::clamp(bounds.minX, -1.0f, MAP_EXTENT + 1.0f);
    bounds.minY = std::clamp(bounds.minY, -1.0f, MAP_EXTENT + 1.0f);
    bounds.maxX = std::clamp(bounds.maxX, -1.0f, MAP_EXTENT + 1.0f);
    bounds.maxY = std::clamp(bounds.maxY, -1.0f, MAP_EXTENT + 1.0f);
    const float offset = AnchorOffset(anchor);
    CellRect rect;
    rect.minX = std::max(static_cast<int>(std::floor(bounds.minX - offset)), 0);
    rect.minY = std::max(static_cast<int>(std::floor(bounds.minY - offset)), 0);
    rect.maxX = std::min(static_cast<int>(std::ceil(bounds.maxX - offset)), LAST_CELL);
    rect.maxY = std::min(static_cast<int>(std::ceil(bounds.maxY - offset)), LAST_CELL);
    if (rect.IsEmpty())
        return CellRect{};
    return rect;
}

WeightMask Rasterize(const Shape& shape, CellAnchor anchor, float falloff)
{
    WeightMask mask;
    mask.rect = Footprint(shape, anchor);
    if (mask.rect.IsEmpty())
        return mask;
    const float offset = AnchorOffset(anchor);
    mask.weights.reserve(static_cast<std::size_t>(mask.rect.Width()) * static_cast<std::size_t>(mask.rect.Height()));
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
    {
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
        {
            const Point cell{static_cast<float>(x) + offset, static_cast<float>(y) + offset};
            mask.weights.push_back(WeightAt(shape, cell, falloff));
        }
    }
    return mask;
}

int TileArea(const Shape& shape)
{
    const CellRect rect = Footprint(shape, CellAnchor::TileCentre);
    int tiles = 0;
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            const Point centre{static_cast<float>(x) + TILE_CENTRE_OFFSET, static_cast<float>(y) + TILE_CENTRE_OFFSET};
            if (InsideDistance(shape, centre) >= 0.0f)
                ++tiles;
        }
    }
    return tiles;
}
} // namespace Editor::MapScript

#endif // _EDITOR
