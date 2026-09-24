#include "TerrainBrush.h"

#ifdef _EDITOR

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Editor::Editing
{
namespace
{
constexpr float TILE_CENTRE_OFFSET = 0.5f;

float AnchorOffset(CellAnchor anchor)
{
    return anchor == CellAnchor::TileCentre ? TILE_CENTRE_OFFSET : 0.0f;
}

float DistanceSquared(const BrushCircle& circle, int x, int y, CellAnchor anchor)
{
    const float offset = AnchorOffset(anchor);
    const float dx = static_cast<float>(x) + offset - circle.centerX;
    const float dy = static_cast<float>(y) + offset - circle.centerY;
    return dx * dx + dy * dy;
}

CellRect Clip(CellRect rect, int width, int height)
{
    rect.minX = std::max(rect.minX, 0);
    rect.minY = std::max(rect.minY, 0);
    rect.maxX = std::min(rect.maxX, width - 1);
    rect.maxY = std::min(rect.maxY, height - 1);
    return rect;
}
} // namespace

float WeightMask::At(int x, int y) const
{
    if (rect.IsEmpty() || x < rect.minX || x > rect.maxX || y < rect.minY || y > rect.maxY)
        return 0.0f;
    const std::size_t row = static_cast<std::size_t>(y - rect.minY);
    const std::size_t column = static_cast<std::size_t>(x - rect.minX);
    const std::size_t cell = row * static_cast<std::size_t>(rect.Width()) + column;
    return cell < weights.size() ? weights[cell] : 0.0f;
}

float Falloff(float distance, float radius)
{
    if (radius <= 0.0f || distance >= radius)
        return 0.0f;
    const float t = distance / radius;
    if (t <= FALLOFF_START)
        return 1.0f;
    const float fade = (t - FALLOFF_START) / (1.0f - FALLOFF_START);
    return 1.0f - fade * fade * (3.0f - 2.0f * fade);
}

float SoftWeight(const BrushCircle& circle, int x, int y, CellAnchor anchor)
{
    return Falloff(std::sqrt(DistanceSquared(circle, x, y, anchor)), circle.radius);
}

bool IsInside(const BrushCircle& circle, int x, int y, CellAnchor anchor)
{
    return circle.radius > 0.0f && DistanceSquared(circle, x, y, anchor) <= circle.radius * circle.radius;
}

CellRect Footprint(const BrushCircle& circle, int width, int height)
{
    if (circle.radius <= 0.0f)
        return {};
    // Wide enough for either anchor; the cells it adds get no weight.
    CellRect rect;
    rect.minX = static_cast<int>(std::floor(circle.centerX - circle.radius));
    rect.minY = static_cast<int>(std::floor(circle.centerY - circle.radius));
    rect.maxX = static_cast<int>(std::ceil(circle.centerX + circle.radius));
    rect.maxY = static_cast<int>(std::ceil(circle.centerY + circle.radius));
    return Clip(rect, width, height);
}

CellRect Grow(const CellRect& rect, int border, int width, int height)
{
    if (rect.IsEmpty())
        return rect;
    return Clip({rect.minX - border, rect.minY - border, rect.maxX + border, rect.maxY + border}, width, height);
}
} // namespace Editor::Editing

#endif // _EDITOR
