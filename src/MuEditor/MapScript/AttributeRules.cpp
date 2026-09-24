#include "AttributeRules.h"

#ifdef _EDITOR

#include "MapInspect/TerrainView.h" // MAP_CELLS, CellIndex

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace Editor::MapScript::Attributes
{
namespace
{
struct NamedValue
{
    const char* name;
    std::uint16_t value;
};

constexpr std::array<NamedValue, 5> NAMED_VALUES = {{
    {"walkable", WALKABLE},
    {"safezone", SAFEZONE},
    {"blocked", BLOCKED},
    {"void", VOID_GROUND},
    {"water", WATER},
}};

// OpenTerrainAttribute's checks (ZzzLodTerrain.cpp), by the engine's map number.
struct MapSentinel
{
    int mapIndex;
    SentinelTile tile;
};

constexpr std::array<MapSentinel, 5> SENTINELS = {{
    {0, {135, 123, 5}}, // Lorencia
    {1, {227, 120, 4}}, // Dungeon
    {2, {208, 55, 5}},  // Devias
    {3, {186, 119, 5}}, // Noria
    {4, {193, 75, 5}},  // Lost Tower
}};

constexpr std::size_t HEADER_BYTES = 4;
constexpr std::uint8_t FILE_VERSION = 0;
constexpr std::uint8_t FILE_EXTENT = 255;
constexpr std::size_t VERSION_BYTE = 0;
constexpr std::size_t WIDTH_BYTE = 2;
constexpr std::size_t HEIGHT_BYTE = 3;
constexpr std::size_t BYTE_LAYOUT = HEADER_BYTES + Editor::MapInspect::MAP_CELLS;
constexpr std::size_t WORD_LAYOUT = HEADER_BYTES + Editor::MapInspect::MAP_CELLS * 2;
constexpr std::uint16_t LOW_BYTE = 0xFF;
constexpr std::uint16_t FIRST_REFUSED_VALUE = 128;
constexpr int BYTE_BITS = 8;

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::uint16_t ReadTile(const std::uint8_t* cells, std::size_t index, bool words)
{
    if (!words)
        return cells[index];
    const std::uint16_t low = cells[index * 2];
    const std::uint16_t high = cells[index * 2 + 1];
    return static_cast<std::uint16_t>(low | (high << BYTE_BITS));
}

bool SentinelIntact(const std::vector<std::uint16_t>& tiles, int mapIndex, std::string& error)
{
    const std::optional<SentinelTile> sentinel = SentinelFor(mapIndex);
    if (!sentinel)
        return true;
    const std::uint16_t value = tiles[Editor::MapInspect::CellIndex(sentinel->x, sentinel->y)];
    if (value == sentinel->value)
        return true;
    error = "tile (" + std::to_string(sentinel->x) + ", " + std::to_string(sentinel->y) + ") holds " +
            std::to_string(value) + ", but the client closes this map unless it holds " +
            std::to_string(sentinel->value);
    return false;
}
} // namespace

bool FromName(const std::string& name, std::uint16_t& value)
{
    const std::string lowered = Lower(name);
    for (const NamedValue& named : NAMED_VALUES)
    {
        if (lowered == named.name)
        {
            value = named.value;
            return true;
        }
    }
    return false;
}

bool IsCleanValue(double number)
{
    return std::any_of(NAMED_VALUES.begin(), NAMED_VALUES.end(),
                       [number](const NamedValue& named) { return static_cast<double>(named.value) == number; });
}

std::string NameOf(std::uint16_t value)
{
    for (const NamedValue& named : NAMED_VALUES)
    {
        if (named.value == value)
            return named.name;
    }
    return std::to_string(value);
}

const char* KnownNames()
{
    return "walkable (0), safezone (1), blocked (4), void (8), water (16)";
}

bool Matches(std::uint16_t tile, std::uint16_t listed)
{
    if (listed == WALKABLE)
        return tile == WALKABLE;
    return (tile & listed) != 0;
}

std::optional<SentinelTile> SentinelFor(int mapIndex)
{
    for (const MapSentinel& sentinel : SENTINELS)
    {
        if (sentinel.mapIndex == mapIndex)
            return sentinel.tile;
    }
    return std::nullopt;
}

bool DecodeClientFile(const std::uint8_t* plain, std::size_t size, int mapIndex, std::vector<std::uint16_t>& tiles,
                      std::string& error)
{
    if (plain == nullptr || (size != BYTE_LAYOUT && size != WORD_LAYOUT))
    {
        error = "it is " + std::to_string(size) + " bytes; a walkability file has " + std::to_string(BYTE_LAYOUT) +
                " or " + std::to_string(WORD_LAYOUT);
        return false;
    }
    if (plain[VERSION_BYTE] != FILE_VERSION || plain[WIDTH_BYTE] != FILE_EXTENT || plain[HEIGHT_BYTE] != FILE_EXTENT)
    {
        error = "its header is not version 0 with 255 x 255";
        return false;
    }
    const bool words = size == WORD_LAYOUT;
    std::vector<std::uint16_t> decoded(Editor::MapInspect::MAP_CELLS);
    for (std::size_t i = 0; i < decoded.size(); ++i)
    {
        decoded[i] = ReadTile(plain + HEADER_BYTES, i, words) & LOW_BYTE;
        if (decoded[i] >= FIRST_REFUSED_VALUE)
        {
            error = "tile " + std::to_string(i) + " holds " + std::to_string(decoded[i]) +
                    ", which the client refuses (128 or more)";
            return false;
        }
    }
    if (!SentinelIntact(decoded, mapIndex, error))
        return false;
    tiles = std::move(decoded);
    return true;
}
} // namespace Editor::MapScript::Attributes

#endif // _EDITOR
