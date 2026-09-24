#pragma once

#ifdef _EDITOR

#include "TerrainView.h"
#include "TileArea.h"

#include <array>
#include <cstdint>
#include <map>

// Numbers about a rectangle of a map, for quick checks without images (map-query).
namespace Editor::MapInspect
{
struct HeightStats
{
    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;
};

struct AreaStats
{
    CellRect area;
    int cells = 0;
    HeightStats height;
    std::map<std::uint16_t, int> attributes; // stored attribute value -> cells
    std::map<int, int> baseTiles;            // layer 1 slot -> cells
    std::map<int, int> overlayTiles;         // layer 2 slot -> cells (255 = none)
    float overlayAlphaMean = 0.0f;
    std::array<float, 3> lightMean = {};
};

// Heights, attribute and texture histograms, mean overlay opacity and mean light of
// the cells in `area` (which must lie on the map).
AreaStats MeasureArea(const TerrainView& terrain, const CellRect& area);
} // namespace Editor::MapInspect

#endif // _EDITOR
