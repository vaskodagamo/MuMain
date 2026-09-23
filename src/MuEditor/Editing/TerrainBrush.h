#pragma once

#ifdef _EDITOR

#include "TerrainLayers.h" // CellRect

namespace Editor::Editing
{
// A round brush on a grid of terrain cells, in cell units (one cell is one tile,
// 100 world units): the cursor's position and the radius.
struct BrushCircle
{
    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 1.0f;
};

// Where a cell's value lives: heights, terrain light and the overlay texture's
// opacity are stored per grid corner (cell x, y is the point x, y); walkability and
// the base texture per tile (cell x, y covers x..x+1, so its centre is x+0.5, y+0.5).
enum class CellAnchor
{
    Corner,
    TileCentre,
};

// A soft brush acts fully out to this share of its radius, then fades to nothing at
// the rim along a smoothstep curve (no visible step where the fade starts or ends).
constexpr float FALLOFF_START = 0.5f;

// How strongly a soft brush acts at `distance` from its centre: 1 inside
// FALLOFF_START * radius, 0 at the rim and beyond.
float Falloff(float distance, float radius);

// The same for cell (x, y) of `circle`.
float SoftWeight(const BrushCircle& circle, int x, int y, CellAnchor anchor);

// True when cell (x, y) of `circle` lies inside it (a brush with a hard edge).
bool IsInside(const BrushCircle& circle, int x, int y, CellAnchor anchor);

// The cells `circle` can reach on a width x height map. The brush stops at the map's
// edges (it never wraps around to the other side): the rectangle is clipped, and
// empty when the circle lies outside the map.
CellRect Footprint(const BrushCircle& circle, int width, int height);

// `rect` grown by `border` cells on every side, clipped to the map.
CellRect Grow(const CellRect& rect, int border, int width, int height);
} // namespace Editor::Editing

#endif // _EDITOR
