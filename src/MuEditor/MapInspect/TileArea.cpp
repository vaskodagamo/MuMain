#include "TileArea.h"

#ifdef _EDITOR

#include <algorithm>
#include <cmath>

namespace Editor::MapInspect
{
namespace
{
constexpr float HALF_TILE = 0.5f;

std::string DescribeTile(int x, int y)
{
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}
} // namespace

int TileOf(float worldCoordinate)
{
    const int tile = static_cast<int>(std::floor(worldCoordinate / TILE_WORLD_SIZE));
    return std::clamp(tile, 0, MAP_TILES - 1);
}

float TileCentre(int tile)
{
    return (static_cast<float>(tile) + HALF_TILE) * TILE_WORLD_SIZE;
}

bool IsOnMap(int x, int y)
{
    return x >= 0 && y >= 0 && x < MAP_TILES && y < MAP_TILES;
}

bool AreaFromCorners(int x0, int y0, int x1, int y1, CellRect& area, std::string& error)
{
    if (!IsOnMap(x0, y0) || !IsOnMap(x1, y1))
    {
        error = "tile " + DescribeTile(IsOnMap(x0, y0) ? x1 : x0, IsOnMap(x0, y0) ? y1 : y0) +
                " is outside the map; tiles run from 0 to " + std::to_string(MAP_TILES - 1);
        return false;
    }
    area.minX = std::min(x0, x1);
    area.minY = std::min(y0, y1);
    area.maxX = std::max(x0, x1);
    area.maxY = std::max(y0, y1);
    return true;
}

bool Contains(const CellRect& area, int x, int y)
{
    return x >= area.minX && x <= area.maxX && y >= area.minY && y <= area.maxY;
}

int CellCount(const CellRect& area)
{
    return area.IsEmpty() ? 0 : area.Width() * area.Height();
}
} // namespace Editor::MapInspect

#endif // _EDITOR
