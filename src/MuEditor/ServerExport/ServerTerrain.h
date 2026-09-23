#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>
#include <vector>

// A map's walk map as OpenMU stores it (GameMapDefinition.TerrainData, uploaded in the
// Admin Panel's "Terrain Data" field): 3 header bytes (0, 255, 255), then one byte a tile,
// index y * 256 + x, not encrypted (MAP_EDITOR.md, "On-disk formats").
namespace Editor::ServerExport
{
constexpr std::size_t SERVER_HEADER_BYTES = 3;
constexpr std::size_t MAP_CELLS = 256 * 256;
constexpr std::size_t TERRAIN_DATA_BYTES = SERVER_HEADER_BYTES + MAP_CELLS;

struct ServerTerrain
{
    std::vector<std::uint8_t> data; // TERRAIN_DATA_BYTES
    // Tiles the client lets players walk on that OpenMU blocks: it reads the exact value
    // (0 and 1 walk), the client tests bits, so water (16) and combined values differ.
    int differing = 0;
};

// The whole walk map `tiles` (65536 client attribute values) in the server's layout,
// without the bit the game sets at run time where a character stands. Empty `data` when
// `tiles` does not hold 65536 values.
ServerTerrain BuildTerrainData(const std::vector<std::uint16_t>& tiles);
} // namespace Editor::ServerExport

#endif // _EDITOR
