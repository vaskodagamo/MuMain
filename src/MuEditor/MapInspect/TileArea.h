#pragma once

#ifdef _EDITOR

#include "Editing/TerrainLayers.h" // CellRect
#include "TerrainView.h"

#include <string>

// Rectangles of map tiles as scripted clients name them: [x0, y0, x1, y1], corners
// in any order, both included.
namespace Editor::MapInspect
{
using Editor::Editing::CellRect;

constexpr CellRect WholeMap()
{
    return CellRect{0, 0, MAP_TILES - 1, MAP_TILES - 1};
}

// The tile a world coordinate lies on, clamped to the map.
int TileOf(float worldCoordinate);

// The world coordinate of a tile's centre.
float TileCentre(int tile);

// True when (x, y) is a tile of the map.
bool IsOnMap(int x, int y);

// The rectangle spanned by two corners given in any order. False, with the reason in
// `error`, when a corner lies outside the map.
bool AreaFromCorners(int x0, int y0, int x1, int y1, CellRect& area, std::string& error);

bool Contains(const CellRect& area, int x, int y);

// Cells in `area` (0 for an empty one).
int CellCount(const CellRect& area);
} // namespace Editor::MapInspect

#endif // _EDITOR
