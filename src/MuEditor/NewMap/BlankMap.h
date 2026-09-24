#pragma once

#ifdef _EDITOR

#include "MapFileCodec.h"

#include <array>
#include <cstdint>
#include <string>

// The files of a new flat map: one texture, one height, one walkability value and one
// light colour everywhere, and no objects. The plain layouts are the ones the client
// loads (EncTerrainFile.h); the caller encrypts them.
namespace Editor::NewMap
{
struct BlankGround
{
    std::uint8_t heightByte = 0; // TerrainHeight.OZB stores height / 1.5 in one byte
    std::uint8_t tileSlot = 0;   // the texture slot of layer 1 (0 TileGrass01 ... 29 ExtTile16)
    std::uint8_t attribute = 0;  // the walkability value of every tile
    std::array<float, 3> light = {1.0f, 1.0f, 1.0f};
};

// Plain EncTerrain{folder}.map: every tile `tileSlot`, no overlay.
Bytes BlankMappingPlain(int folder, std::uint8_t tileSlot);
// Plain EncTerrain{folder}.att: one byte a tile, every tile `attribute`.
Bytes BlankAttributePlain(int folder, std::uint8_t attribute);
// Plain EncTerrain{folder}.obj without objects.
Bytes EmptyObjectsPlain(int folder);

// True for a TerrainHeight.OZB with one byte a corner, the layout every map loads unless
// the client names it as one of the few 24-bit maps (IsTerrainHeightExtMap); a new map
// number is never one of those.
bool IsEightBitHeightFile(const Bytes& file);

// TerrainHeight.OZB with every corner at `heightByte`: `templateFile` (another map's 8-bit
// height file) with its heights replaced, so the prefix and the BMP header stay the ones
// the loader knows. False with the reason when the template is not such a file.
bool FlatHeightFile(const Bytes& templateFile, std::uint8_t heightByte, Bytes& out, std::string& error);

// TerrainLight.OZJ with every corner at `rgb` (0..1). Empty when the encoder fails.
Bytes FlatLightFile(const std::array<float, 3>& rgb);
} // namespace Editor::NewMap

#endif // _EDITOR
