#include "TerrainTileIndex.h"

#include <algorithm>

namespace Render::Terrain::TileIndex
{
int Corner(int x, int y)
{
    const int index = y * SIDE + x;
    if (index < CELLS)
        return index;
    return (SIDE - 1) * SIDE + std::min(x, SIDE - 1);
}
} // namespace Render::Terrain::TileIndex
