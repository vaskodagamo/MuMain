#include "GateChecks.h"

#ifdef _EDITOR

#include "GateQueries.h"

#include "MapInspect/AttributeBits.h"

namespace Editor::Gates
{
namespace
{
namespace Bits = Editor::MapInspect::Attribute;

constexpr int MAP_SIDE = LAST_TILE + 1;
constexpr std::size_t MAP_CELLS = static_cast<std::size_t>(MAP_SIDE) * MAP_SIDE;
// The byte a walk map file stores (the client keeps the low byte of each cell).
constexpr std::uint16_t STORED_BYTE_MASK = 0x00FF;
// OpenMU reads the exact stored value: 0 (walkable) and 1 (safe zone) are walkable, every
// other value blocks (MAP_EDITOR.md, gotcha 12).
constexpr std::uint16_t SERVER_WALKABLE_MAX = Bits::SAFEZONE;

bool ClientBlocks(std::uint16_t value)
{
    return (value & (Bits::NOMOVE | Bits::NOGROUND)) != 0;
}

bool ServerBlocks(std::uint16_t value)
{
    const std::uint16_t stored = static_cast<std::uint16_t>(value & STORED_BYTE_MASK & ~Bits::CHARACTER);
    return stored > SERVER_WALKABLE_MAX;
}

std::string AreaText(const TileRect& area)
{
    return "[" + std::to_string(area.x1) + ", " + std::to_string(area.y1) + ", " + std::to_string(area.x2) + ", " +
           std::to_string(area.y2) + "]";
}

std::string MapText(int map)
{
    return "map " + std::to_string(map);
}

// Gates of `table` on `map` of the given flag whose area overlaps `area`.
std::vector<int> OverlappingGates(const GateTable& table, int map, const TileRect& area, std::uint8_t flag)
{
    std::vector<int> numbers;
    for (const int number : GatesOnMap(table, map))
    {
        const GateRecord& record = table[static_cast<std::size_t>(number)];
        if (record.flag == flag && Overlaps(AreaOf(record), area))
            numbers.push_back(number);
    }
    return numbers;
}

void AddOverlapWarnings(const GateTable& table, const NewGatePair& pair, std::vector<std::string>& warnings)
{
    for (const int number : OverlappingGates(table, pair.from.map, pair.from.area, FLAG_ENTER))
        warnings.push_back("the enter area overlaps enter gate " + std::to_string(number) + " on " +
                           MapText(pair.from.map) + ": a player standing in both is sent through either");
    for (const int number : OverlappingGates(table, pair.from.map, pair.from.area, FLAG_ARRIVAL))
        warnings.push_back("the enter area overlaps arrival gate " + std::to_string(number) + " on " +
                           MapText(pair.from.map) + ": players who arrive there are sent on at once");
    for (const int number : OverlappingGates(table, pair.to.map, pair.to.area, FLAG_ENTER))
        warnings.push_back("the arrival area overlaps enter gate " + std::to_string(number) + " on " +
                           MapText(pair.to.map) + ": arriving players are sent on at once");
    if (pair.from.map == pair.to.map && Overlaps(pair.from.area, pair.to.area))
        warnings.push_back("the arrival area overlaps the enter area: players would bounce between them");
}
} // namespace

AreaWalkability CheckWalkability(const std::vector<std::uint16_t>& tiles, const TileRect& area)
{
    AreaWalkability result;
    if (tiles.size() != MAP_CELLS)
        return result;
    for (int y = area.y1; y <= area.y2; ++y)
    {
        for (int x = area.x1; x <= area.x2; ++x)
        {
            const std::uint16_t value = tiles[static_cast<std::size_t>(y) * MAP_SIDE + x];
            ++result.tiles;
            if (ClientBlocks(value))
                ++result.blocked;
            else if (ServerBlocks(value))
                ++result.serverBlocked;
        }
    }
    return result;
}

std::vector<std::string> AreaWarnings(const WalkMap& walkMap, int map, const TileRect& area, const char* what)
{
    const std::string subject = std::string(what) + " " + AreaText(area) + " on " + MapText(map);
    if (walkMap.tiles.size() != MAP_CELLS)
        return {"the walkability of " + subject + " was not checked: " + walkMap.problem};

    std::vector<std::string> warnings;
    const AreaWalkability walk = CheckWalkability(walkMap.tiles, area);
    if (walk.blocked == walk.tiles)
        warnings.push_back("no tile of " + subject + " is walkable (all blocked or without ground)");
    else if (walk.blocked > 0)
        warnings.push_back(std::to_string(walk.blocked) + " of " + std::to_string(walk.tiles) + " tiles of " + subject +
                           " are blocked or without ground");
    if (walk.serverBlocked > 0)
        warnings.push_back(std::to_string(walk.serverBlocked) + " tiles of " + subject +
                           " are walkable in the client but blocked for the server (water or a combined value)");
    return warnings;
}

std::vector<std::string> PairWarnings(const GateTable& table, const NewGatePair& pair, const WalkMap& fromMap,
                                      const WalkMap& toMap)
{
    std::vector<std::string> warnings = AreaWarnings(fromMap, pair.from.map, pair.from.area, "the enter area");
    const std::vector<std::string> arrival = AreaWarnings(toMap, pair.to.map, pair.to.area, "the arrival area");
    warnings.insert(warnings.end(), arrival.begin(), arrival.end());
    AddOverlapWarnings(table, pair, warnings);
    return warnings;
}

std::vector<std::string> PairTraps(const NewGatePair& pair, const WalkMap& toMap)
{
    std::vector<std::string> traps;
    const bool known = toMap.tiles.size() == MAP_CELLS;
    const AreaWalkability arrival = known ? CheckWalkability(toMap.tiles, pair.to.area) : AreaWalkability{};
    if (known && arrival.blocked == arrival.tiles)
        traps.push_back("no tile of the arrival area " + AreaText(pair.to.area) + " on " + MapText(pair.to.map) +
                        " is walkable: arriving players could not move");
    if (pair.from.map == pair.to.map && Overlaps(pair.from.area, pair.to.area))
        traps.push_back("the arrival area overlaps the enter area: players would bounce between them without end");
    return traps;
}
} // namespace Editor::Gates

#endif // _EDITOR
