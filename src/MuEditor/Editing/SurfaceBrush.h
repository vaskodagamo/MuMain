#pragma once

#ifdef _EDITOR

#include "TerrainBrush.h"

#include <cstdint>

namespace Editor::Editing
{
// The terrain's overlay texture ("layer 2"): per grid corner a tile slot and how
// opaque it is there. A tile shows the overlay slot of its first corner, blended
// over the base texture with the opacity of its four corners.
struct OverlayLayer
{
    std::uint8_t* tiles = nullptr;
    float* alpha = nullptr;
    int width = 0;
    int height = 0;
};

// The tile slot a corner without an overlay has (as the map loader fills it).
constexpr std::uint8_t NO_OVERLAY_TILE = 255;

// Paints overlay `tile` with the soft brush. Where the corner already shows `tile`,
// its opacity moves towards `opacity` by `rate` (0..1) times the brush weight; a
// corner the brush reaches that shows another overlay tile takes `tile` with the
// opacity it gets from this frame alone (painting over a tile replaces it). Corners
// just outside the circle (one corner below and left of it) that have no overlay
// take the slot at opacity 0, so the tiles whose other corners the brush reached
// fade out towards the rim on every side. Returns the rectangle it may have changed.
CellRect PaintOverlay(const OverlayLayer& layer, const BrushCircle& circle, std::uint8_t tile, float opacity,
                      float rate);

// Fades the overlay out with the soft brush: opacity moves towards 0 by `rate` times
// the weight; a corner that reaches 0 loses its overlay slot.
CellRect EraseOverlay(const OverlayLayer& layer, const BrushCircle& circle, float rate);

// Calls visit(x, y) for every cell inside `circle` (hard edge) on a width x height map.
template <typename Visit>
void ForEachCellInside(const BrushCircle& circle, int width, int height, CellAnchor anchor, Visit visit)
{
    const CellRect rect = Footprint(circle, width, height);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            if (IsInside(circle, x, y, anchor))
                visit(x, y);
        }
    }
}
} // namespace Editor::Editing

#endif // _EDITOR
