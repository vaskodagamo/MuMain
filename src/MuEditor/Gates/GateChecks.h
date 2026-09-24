#pragma once

#ifdef _EDITOR

#include "GateTableEdit.h"

#include <cstdint>
#include <string>
#include <vector>

// What could stop a gate from working: tiles players cannot walk on, and areas that
// overlap other gates (an arrival inside an enter gate sends players straight on).
namespace Editor::Gates
{
// A map's walk map (65536 attribute values, index y * 256 + x), or why it is not known.
struct WalkMap
{
    std::vector<std::uint16_t> tiles;
    std::string problem;
};

struct AreaWalkability
{
    int tiles = 0;
    int blocked = 0;       // blocked or without ground in the client's walk map
    int serverBlocked = 0; // walkable in the client, blocked for OpenMU (water, combined values)
};

// `area` must lie on the map (NormalizeArea).
AreaWalkability CheckWalkability(const std::vector<std::uint16_t>& tiles, const TileRect& area);

// The warnings for adding `pair` to `table` (checked before it is added), one sentence
// each. `fromMap` and `toMap` are the walk maps of pair.from.map and pair.to.map.
std::vector<std::string> PairWarnings(const GateTable& table, const NewGatePair& pair, const WalkMap& fromMap,
                                      const WalkMap& toMap);

// The walkability warnings of one gate area on a map ("the enter area", "the arrival area").
std::vector<std::string> AreaWarnings(const WalkMap& walkMap, int map, const TileRect& area, const char* what);

// What would trap players rather than merely hinder them, one sentence each: an arrival
// area without a walkable tile (players arrive where they cannot move) and an arrival that
// overlaps its own enter area (players bounce between them without end). Adding such a
// pair takes NewGatePair::allowTrap. `toMap` is the walk map of pair.to.map; an arrival
// whose walk map is not known is not called a trap.
std::vector<std::string> PairTraps(const NewGatePair& pair, const WalkMap& toMap);
} // namespace Editor::Gates

#endif // _EDITOR
