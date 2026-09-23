#pragma once

#ifdef _EDITOR

#include <array>
#include <cstddef>
#include <cstdint>

// The client's gate table, Data/Gate.bmd: 512 records of 14 bytes, the layout of
// GATE_ATTRIBUTE (Core/Globals/_struct.h), each record XOR'd with BuxConvert on its own.
// A record's number is its place in the file; the client sends that number when the hero
// walks into an enter gate (CheckGate), and OpenMU looks it up among the map's enter
// gates, so client and server numbers must agree.
//
// Byte layout of a record (little-endian):
//   0 flag, 1 map, 2 x1, 3 y1, 4 x2, 5 y2, 6-7 target, 8 direction, 9 padding,
//   10-11 level, 12-13 max level
namespace Editor::Gates
{
constexpr int GATE_COUNT = 512; // MAX_GATES
constexpr std::size_t RECORD_BYTES = 14;
constexpr std::size_t FILE_BYTES = GATE_COUNT * RECORD_BYTES;

// Numbers 0 to 344 are the game's own gates: the last enter and arrival gates are 342 and
// 343, and 344 is a spawn record of Karutan 2 (map 81), in Gate.bmd and in OpenMU's seed
// alike. Gates added with the editor take free numbers from 345 on and are the only ones
// it changes or removes.
constexpr int FIRST_CUSTOM_GATE = 345;
constexpr int LAST_GATE = GATE_COUNT - 1;

// Every shipped record's upper level limit.
constexpr std::uint16_t DEFAULT_MAX_LEVEL = 400;
constexpr int MAX_LEVEL_REQUIREMENT = DEFAULT_MAX_LEVEL;
// A gate area is a rectangle of tiles on a 256 x 256 map, both corners included.
constexpr int LAST_TILE = 255;
// OpenMU's Direction: 0 undefined, 1 west ... 8 north-west.
constexpr int LAST_DIRECTION = 8;

// GATE_ATTRIBUTE::Flag.
constexpr std::uint8_t FLAG_SPAWN = 0;   // with an area: where the server puts players (a town)
constexpr std::uint8_t FLAG_ENTER = 1;   // walking into its area warps to its target
constexpr std::uint8_t FLAG_ARRIVAL = 2; // where a warp arrives (OpenMU: an exit gate)

struct GateRecord
{
    std::uint8_t flag = 0;
    std::uint8_t map = 0; // the game's map number (Lorencia 0), not the Data folder number
    std::uint8_t x1 = 0;
    std::uint8_t y1 = 0;
    std::uint8_t x2 = 0;
    std::uint8_t y2 = 0;
    std::uint16_t target = 0;   // enter gates: the number of the arrival gate a warp lands on
    std::uint8_t direction = 0; // arrival gates: where the player faces (OpenMU's Direction)
    std::uint8_t padding = 0;   // the byte the struct's alignment leaves; kept as read
    std::uint16_t level = 0;    // the level needed (enter gates)
    std::uint16_t maxLevel = 0;

    bool operator==(const GateRecord&) const = default;
};

using GateTable = std::array<GateRecord, GATE_COUNT>;

// A record with nothing in it: every field zero. Only free records are given to new gates.
bool IsFree(const GateRecord& record);
bool IsCustomNumber(int number);
bool IsValidNumber(int number);

// "enter", "arrival", "spawn" or "free".
const char* KindName(const GateRecord& record);
} // namespace Editor::Gates

#endif // _EDITOR
