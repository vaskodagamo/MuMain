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

// A brush's footprint plus the corners one step below and to the left of it: the
// first corners of the tiles along its lower and left rim.
CellRect PaintRect(CellRect rect)
{
    if (rect.IsEmpty())
        return rect;
    rect.minX = std::max(rect.minX - 1, 0);
    rect.minY = std::max(rect.minY - 1, 0);
    return rect;
}

// `claimEmpty`: a corner the brush does not reach takes the slot (at opacity 0) when it
// has no overlay, so the tile it starts fades out towards the brush.
void PaintCorner(const OverlayLayer& layer, std::size_t index, std::uint8_t tile, float target, float weight,
                 bool claimEmpty)
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
    if (claimEmpty && slot == NO_OVERLAY_TILE && alpha <= 0.0f)
        slot = tile;
}

template <typename WeightAt, typename ClaimsEmpty>
CellRect Paint(const OverlayLayer& layer, const CellRect& footprint, WeightAt weightAt, ClaimsEmpty claimsEmpty,
               std::uint8_t tile, float opacity, float rate)
{
    const CellRect rect = PaintRect(footprint);
    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    const float target = std::clamp(opacity, 0.0f, 1.0f);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
            PaintCorner(layer, IndexOf(layer, x, y), tile, target, weightAt(x, y) * clampedRate, claimsEmpty(x, y));
    }
    return rect;
}

template <typename WeightAt>
CellRect Erase(const OverlayLayer& layer, const CellRect& rect, WeightAt weightAt, float rate)
{
    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            const std::size_t index = IndexOf(layer, x, y);
            const float weight = weightAt(x, y) * clampedRate;
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

auto CircleWeights(const BrushCircle& circle)
{
    return [&circle](int x, int y) { return SoftWeight(circle, x, y, CellAnchor::Corner); };
}

auto MaskWeights(const WeightMask& mask)
{
    return [&mask](int x, int y) { return mask.At(x, y); };
}

// Every corner of a round brush's square takes the slot, as the Texture tab always did.
bool ClaimAll(int, int)
{
    return true;
}

// A mask's footprint can be a long diagonal road: only the first corners of the tiles it
// reaches take the slot, not every empty corner of its bounding rectangle.
auto ClaimsTileStart(const WeightMask& mask)
{
    return [&mask](int x, int y)
    { return mask.At(x + 1, y) > 0.0f || mask.At(x, y + 1) > 0.0f || mask.At(x + 1, y + 1) > 0.0f; };
}

CellRect MaskRect(const OverlayLayer& layer, const WeightMask& mask)
{
    return Grow(mask.rect, 0, layer.width, layer.height);
}
} // namespace

CellRect PaintOverlay(const OverlayLayer& layer, const BrushCircle& circle, std::uint8_t tile, float opacity,
                      float rate)
{
    return Paint(layer, Footprint(circle, layer.width, layer.height), CircleWeights(circle), ClaimAll, tile, opacity,
                 rate);
}

CellRect EraseOverlay(const OverlayLayer& layer, const BrushCircle& circle, float rate)
{
    return Erase(layer, Footprint(circle, layer.width, layer.height), CircleWeights(circle), rate);
}

CellRect PaintOverlay(const OverlayLayer& layer, const WeightMask& mask, std::uint8_t tile, float opacity, float rate)
{
    return Paint(layer, MaskRect(layer, mask), MaskWeights(mask), ClaimsTileStart(mask), tile, opacity, rate);
}

CellRect EraseOverlay(const OverlayLayer& layer, const WeightMask& mask, float rate)
{
    return Erase(layer, MaskRect(layer, mask), MaskWeights(mask), rate);
}
} // namespace Editor::Editing

#endif // _EDITOR
