#include "AreaStats.h"

#ifdef _EDITOR

#include "AttributePalette.h"

#include <algorithm>
#include <limits>

namespace Editor::MapInspect
{
namespace
{
constexpr std::size_t LIGHT_CHANNELS = 3;

// Sums kept in double: 65536 floats added in float lose the last digits.
struct Sums
{
    double height = 0.0;
    double alpha = 0.0;
    std::array<double, LIGHT_CHANNELS> light = {};
};

void CountCell(const TerrainView& terrain, std::size_t index, AreaStats& stats, Sums& sums)
{
    const float height = terrain.height[index];
    stats.height.min = std::min(stats.height.min, height);
    stats.height.max = std::max(stats.height.max, height);
    sums.height += height;
    ++stats.attributes[StoredAttribute(terrain.attribute[index])];
    ++stats.baseTiles[terrain.baseTiles[index]];
    ++stats.overlayTiles[terrain.overlayTiles[index]];
    sums.alpha += terrain.overlayAlpha[index];
    for (std::size_t channel = 0; channel < LIGHT_CHANNELS; ++channel)
        sums.light[channel] += terrain.light[index * LIGHT_CHANNELS + channel];
}

bool HasEveryLayer(const TerrainView& terrain)
{
    return terrain.height != nullptr && terrain.attribute != nullptr && terrain.baseTiles != nullptr &&
           terrain.overlayTiles != nullptr && terrain.overlayAlpha != nullptr && terrain.light != nullptr;
}
} // namespace

AreaStats MeasureArea(const TerrainView& terrain, const CellRect& area)
{
    AreaStats stats;
    stats.area = area;
    stats.cells = CellCount(area);
    if (stats.cells == 0 || !HasEveryLayer(terrain))
        return stats;

    stats.height.min = std::numeric_limits<float>::max();
    stats.height.max = std::numeric_limits<float>::lowest();
    Sums sums;
    for (int y = area.minY; y <= area.maxY; ++y)
    {
        for (int x = area.minX; x <= area.maxX; ++x)
            CountCell(terrain, CellIndex(x, y), stats, sums);
    }

    const double cells = static_cast<double>(stats.cells);
    stats.height.mean = static_cast<float>(sums.height / cells);
    stats.overlayAlphaMean = static_cast<float>(sums.alpha / cells);
    for (std::size_t channel = 0; channel < LIGHT_CHANNELS; ++channel)
        stats.lightMean[channel] = static_cast<float>(sums.light[channel] / cells);
    return stats;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
