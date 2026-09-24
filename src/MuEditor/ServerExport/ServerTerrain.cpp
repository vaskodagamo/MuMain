#include "ServerTerrain.h"

#ifdef _EDITOR

#include "MapInspect/AttributeBits.h"

namespace Editor::ServerExport
{
namespace
{
namespace Bits = Editor::MapInspect::Attribute;

constexpr std::uint8_t SERVER_VERSION = 0;
constexpr std::uint8_t SERVER_EXTENT = 255;
constexpr std::uint16_t STORED_BYTE_MASK = 0x00FF;
constexpr std::uint16_t SERVER_WALKABLE_MAX = Bits::SAFEZONE;

std::uint8_t StoredValue(std::uint16_t tile)
{
    return static_cast<std::uint8_t>(tile & STORED_BYTE_MASK & ~Bits::CHARACTER);
}

bool ClientWalks(std::uint8_t value)
{
    return (value & (Bits::NOMOVE | Bits::NOGROUND)) == 0;
}
} // namespace

ServerTerrain BuildTerrainData(const std::vector<std::uint16_t>& tiles)
{
    ServerTerrain terrain;
    if (tiles.size() != MAP_CELLS)
        return terrain;
    terrain.data = {SERVER_VERSION, SERVER_EXTENT, SERVER_EXTENT};
    terrain.data.reserve(TERRAIN_DATA_BYTES);
    for (const std::uint16_t tile : tiles)
    {
        const std::uint8_t value = StoredValue(tile);
        terrain.data.push_back(value);
        if (ClientWalks(value) && value > SERVER_WALKABLE_MAX)
            ++terrain.differing;
    }
    return terrain;
}
} // namespace Editor::ServerExport

#endif // _EDITOR
