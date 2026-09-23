#pragma once

// Which terrain cell a drawn tile's corner reads. Plain arithmetic, no engine state.
namespace Render::Terrain::TileIndex
{
// Cells along each side of a map.
constexpr int SIDE = 256;
constexpr int CELLS = SIDE * SIDE;

// The cell of corner (x, y) of a tile, for the tile renderers' height, attribute and
// light lookups: y * 256 + x wherever that lies inside the terrain arrays (so a tile of
// column 255 still reads the next row's first cell for its right corners, as it always
// has). The top corners of the tiles in row 255, which would lie past the end of the
// arrays, repeat the last row instead.
int Corner(int x, int y);
} // namespace Render::Terrain::TileIndex
