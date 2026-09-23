#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The walkability values an edit script may write, and the tiles it must leave alone.
namespace Editor::MapScript::Attributes
{
// The clean single values the client's .att and OpenMU's server map agree on (see
// MAP_EDITOR.md, gotcha 12): the server reads the exact value (walkable = 0 or 1), the
// client tests bits, so a combination such as 5 means different things to the two.
constexpr std::uint16_t WALKABLE = 0x00;
constexpr std::uint16_t SAFEZONE = 0x01;
constexpr std::uint16_t BLOCKED = 0x04;
constexpr std::uint16_t VOID_GROUND = 0x08; // TW_NOGROUND: no ground, nothing stands there
constexpr std::uint16_t WATER = 0x10;

// "walkable", "safezone", "blocked", "void" or "water", or one of those numbers.
bool FromName(const std::string& name, std::uint16_t& value);
bool IsCleanValue(double number);
std::string NameOf(std::uint16_t value);
// "walkable, safezone, blocked, void, water" for error messages.
const char* KnownNames();

// True when a tile with attribute `tile` counts as `listed` for an avoid rule: the same
// flag set (any common bit), or, for walkable (0), a tile with no flag at all.
bool Matches(std::uint16_t tile, std::uint16_t listed);

// The anti-tamper tile a map's .att must keep: OpenTerrainAttribute closes the client
// ("data error") when it holds another value (MAP_EDITOR.md, gotcha 13).
struct SentinelTile
{
    int x = 0;
    int y = 0;
    std::uint16_t value = 0;
};
std::optional<SentinelTile> SentinelFor(int mapIndex);

// The walkability of a client .att after decryption and BuxConvert: a 4-byte header
// (version 0, map number, 255, 255) and one byte per tile, or two per tile in the
// extended layout; OpenTerrainAttribute keeps the low byte. Fills `tiles` (65536
// values) when the file is one the client loads: the right size and header, every value
// below 128 and the map's sentinel tile intact. Otherwise false with the reason.
bool DecodeClientFile(const std::uint8_t* plain, std::size_t size, int mapIndex, std::vector<std::uint16_t>& tiles,
                      std::string& error);
} // namespace Editor::MapScript::Attributes

#endif // _EDITOR
