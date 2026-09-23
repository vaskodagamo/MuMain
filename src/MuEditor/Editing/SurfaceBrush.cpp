#include "SurfaceBrush.h"

#ifdef _EDITOR

#include <algorithm>
#include <cstddef>

namespace Editor::Editing
{
namespace
{
// Below this the map file stores the opacity as 0 (one byte per corner).
constexpr float INVISIBLE_OPACITY = 0.5f / 255.0f;

std::size_t IndexOf(const OverlayLayer& layer, int x, int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(layer.width) + static_cast<std::size_t>(x);
}

// The brush's footprint plus the corners one step below and to the left of it: the
// first corners of the tiles along its lower and left rim.
CellRect PaintRect(const OverlayLayer& layer, const BrushCircle& circle)
{
    CellRect rect = Footprint(circle, layer.width, layer.height);
    if (rect.IsEmpty())
        return rect;
    rect.minX = std::max(rect.minX - 1, 0);
    rect.minY = std::max(rect.minY - 1, 0);
    return rect;
}

void PaintCorner(const OverlayLayer& layer, std::size_t index, std::uint8_t tile, float target, float weight)
{
    std::uint8_t& slot = layer.tiles[index];
    float& alpha = layer.alpha[index];
    if (slot == tile)
    {
        alpha += (target - alpha) * weight;
        return;
    }
    if (weight > 0.0f)
    {
        slot = tile;
        alpha = target * weight;
        return;
    }
    if (slot == NO_OVERLAY_TILE && alpha <= 0.0f)
        slot = tile;
}
} // namespace

CellRect PaintOverlay(const OverlayLayer& layer, const BrushCircle& circle, std::uint8_t tile, float opacity,
                      float rate)
{
    const CellRect rect = PaintRect(layer, circle);
    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    const float target = std::clamp(opacity, 0.0f, 1.0f);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            const float weight = SoftWeight(circle, x, y, CellAnchor::Corner) * clampedRate;
            PaintCorner(layer, IndexOf(layer, x, y), tile, target, weight);
        }
    }
    return rect;
}

CellRect EraseOverlay(const OverlayLayer& layer, const BrushCircle& circle, float rate)
{
    const CellRect rect = Footprint(circle, layer.width, layer.height);
    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            const std::size_t index = IndexOf(layer, x, y);
            const float weight = SoftWeight(circle, x, y, CellAnchor::Corner) * clampedRate;
            if (weight <= 0.0f || layer.tiles[index] == NO_OVERLAY_TILE)
                continue;
            layer.alpha[index] -= layer.alpha[index] * weight;
            if (layer.alpha[index] > INVISIBLE_OPACITY)
                continue;
            layer.alpha[index] = 0.0f;
            layer.tiles[index] = NO_OVERLAY_TILE;
        }
    }
    return rect;
}
} // namespace Editor::Editing

#endif // _EDITOR
