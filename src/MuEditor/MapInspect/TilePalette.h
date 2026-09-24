#pragma once

#ifdef _EDITOR

#include "Image.h" // Rgb

#include <cstdint>
#include <string>

// The terrain texture slots a map's tile layers index, and the colours the exported
// texture layers show them in.
namespace Editor::MapInspect
{
// The loader fills slots 0..29: 14 named Tile* textures, then ExtTile01..16. A cell
// of the tile layers holds the slot of the texture drawn there.
constexpr int TILE_SLOT_COUNT = 30;
// Layer 2 (overlay) value for "no overlay texture on this cell".
constexpr std::uint8_t NO_OVERLAY_TILE = 255;

// The file name (without folder and extension) the loader reads into `slot`, as
// MapManager's terrain load names it: TileGrass01 ... TileRock07, ExtTile01 ...
// ExtTile16. "none" for NO_OVERLAY_TILE.
std::string TileSlotName(int slot);

// A colour per slot for the texture images: the named slots in their material's
// colour (grass green, water blue, rock grey...), the ExtTile slots in distinct hues,
// NO_OVERLAY_TILE black.
Rgb TileSlotColor(int slot);
} // namespace Editor::MapInspect

#endif // _EDITOR
