#include "GateTableEdit.h"

#ifdef _EDITOR

#include "GateQueries.h"

#include "World/MapInfra/MapNumbers.h"

#include <algorithm>
#include <utility>

namespace Editor::Gates
{
namespace
{
constexpr std::size_t GATES_PER_PAIR = 2;

std::string NumberText(int number)
{
    return "gate " + std::to_string(number);
}

bool CheckMap(int map, const char* side, std::string& error)
{
    if (map >= 0 && map <= World::MapNumbers::LAST_MAP)
        return true;
    error = std::string(side) + " map " + std::to_string(map) + " is not a map number (0 to " +
            std::to_string(World::MapNumbers::LAST_MAP) + ")";
    return false;
}

bool CheckDirection(int direction, std::string& error)
{
    if (direction >= 0 && direction <= LAST_DIRECTION)
        return true;
    error = "the direction is 0 to " + std::to_string(LAST_DIRECTION) + " (OpenMU's Direction)";
    return false;
}

bool CheckLevel(int level, std::string& error)
{
    if (level >= 0 && level <= MAX_LEVEL_REQUIREMENT)
        return true;
    error = "the level is 0 to " + std::to_string(MAX_LEVEL_REQUIREMENT);
    return false;
}

bool CheckEnd(GateEnd& end, const char* side, std::string& error)
{
    if (!CheckMap(end.map, side, error))
        return false;
    std::string areaError;
    if (NormalizeArea(end.area, areaError))
        return true;
    error = std::string(side) + " area: " + areaError;
    return false;
}

void SetArea(GateRecord& record, const TileRect& area)
{
    record.x1 = static_cast<std::uint8_t>(area.x1);
    record.y1 = static_cast<std::uint8_t>(area.y1);
    record.x2 = static_cast<std::uint8_t>(area.x2);
    record.y2 = static_cast<std::uint8_t>(area.y2);
}

GateRecord EnterRecord(const NewGatePair& pair, int arrival)
{
    GateRecord record;
    record.flag = FLAG_ENTER;
    record.map = static_cast<std::uint8_t>(pair.from.map);
    SetArea(record, pair.from.area);
    record.target = static_cast<std::uint16_t>(arrival);
    record.level = static_cast<std::uint16_t>(pair.level);
    record.maxLevel = DEFAULT_MAX_LEVEL;
    return record;
}

// The game's own arrival records repeat their enter gate's level.
GateRecord ArrivalRecord(const NewGatePair& pair)
{
    GateRecord record;
    record.flag = FLAG_ARRIVAL;
    record.map = static_cast<std::uint8_t>(pair.to.map);
    SetArea(record, pair.to.area);
    record.direction = static_cast<std::uint8_t>(pair.direction);
    record.level = static_cast<std::uint16_t>(pair.level);
    record.maxLevel = DEFAULT_MAX_LEVEL;
    return record;
}

bool CheckEditable(const GateTable& table, int number, std::string& error)
{
    if (!IsValidNumber(number))
    {
        error = std::to_string(number) + " is not a gate number (0 to " + std::to_string(LAST_GATE) + ")";
        return false;
    }
    if (!IsCustomNumber(number))
    {
        error = NumberText(number) + " is one of the game's own gates (0 to " + std::to_string(FIRST_CUSTOM_GATE - 1) +
                "); only gates added with the editor (" + std::to_string(FIRST_CUSTOM_GATE) + " to " +
                std::to_string(LAST_GATE) + ") can be changed or removed";
        return false;
    }
    if (IsFree(table[static_cast<std::size_t>(number)]))
    {
        error = NumberText(number) + " is free: there is nothing to change";
        return false;
    }
    return true;
}

std::string NumberList(const std::vector<int>& numbers)
{
    std::string text;
    for (const int number : numbers)
        text += (text.empty() ? "" : ", ") + std::to_string(number);
    return text;
}

// An arrival gate the editor added that no enter gate other than `leaving` lands on.
bool IsOrphanedArrival(const GateTable& table, int arrival, int leaving)
{
    if (!IsCustomNumber(arrival) || table[static_cast<std::size_t>(arrival)].flag != FLAG_ARRIVAL)
        return false;
    const std::vector<int> users = EnterGatesTargeting(table, arrival);
    return std::all_of(users.begin(), users.end(), [leaving](int user) { return user == leaving; });
}
} // namespace

bool NormalizeArea(TileRect& area, std::string& error)
{
    const int corners[] = {area.x1, area.y1, area.x2, area.y2};
    for (const int corner : corners)
    {
        if (corner < 0 || corner > LAST_TILE)
        {
            error = "a corner lies off the map (tiles are 0 to " + std::to_string(LAST_TILE) + ")";
            return false;
        }
    }
    if (area.x1 > area.x2)
        std::swap(area.x1, area.x2);
    if (area.y1 > area.y2)
        std::swap(area.y1, area.y2);
    return true;
}

TileRect AreaOf(const GateRecord& record)
{
    return {record.x1, record.y1, record.x2, record.y2};
}

bool Overlaps(const TileRect& a, const TileRect& b)
{
    return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2;
}

int TileCount(const TileRect& area)
{
    return (area.x2 - area.x1 + 1) * (area.y2 - area.y1 + 1);
}

std::vector<int> FreeNumbers(const GateTable& table)
{
    std::vector<int> numbers;
    for (int number = FIRST_CUSTOM_GATE; number <= LAST_GATE; ++number)
    {
        if (IsFree(table[static_cast<std::size_t>(number)]))
            numbers.push_back(number);
    }
    return numbers;
}

bool AddGatePair(GateTable& table, NewGatePair pair, AddedGates& added, std::string& error)
{
    if (!CheckEnd(pair.from, "the enter gate's", error) || !CheckEnd(pair.to, "the arrival gate's", error) ||
        !CheckDirection(pair.direction, error) || !CheckLevel(pair.level, error))
        return false;

    const std::vector<int> free = FreeNumbers(table);
    if (free.size() < GATES_PER_PAIR)
    {
        error = "Gate.bmd has no two free gate numbers left (" + std::to_string(FIRST_CUSTOM_GATE) + " to " +
                std::to_string(LAST_GATE) + ")";
        return false;
    }
    added.enter = free[0];
    added.arrival = free[1];
    table[static_cast<std::size_t>(added.enter)] = EnterRecord(pair, added.arrival);
    table[static_cast<std::size_t>(added.arrival)] = ArrivalRecord(pair);
    return true;
}

bool RemoveGate(GateTable& table, int number, std::vector<int>& removed, std::string& error)
{
    if (!CheckEditable(table, number, error))
        return false;

    const GateRecord& record = table[static_cast<std::size_t>(number)];
    if (record.flag == FLAG_ARRIVAL)
    {
        const std::vector<int> users = EnterGatesTargeting(table, number);
        if (!users.empty())
        {
            error = NumberText(number) + " is where enter gate " + NumberList(users) +
                    " lands; remove the enter gate instead (its arrival goes with it)";
            return false;
        }
    }

    removed = {number};
    if (record.flag == FLAG_ENTER && IsOrphanedArrival(table, record.target, number))
        removed.push_back(record.target);
    for (const int cleared : removed)
        table[static_cast<std::size_t>(cleared)] = GateRecord{};
    return true;
}

bool ChangeGate(GateTable& table, int number, const GateChange& change, std::string& error)
{
    if (!CheckEditable(table, number, error))
        return false;
    GateRecord record = table[static_cast<std::size_t>(number)];
    if (change.area)
    {
        TileRect area = *change.area;
        if (!NormalizeArea(area, error))
            return false;
        SetArea(record, area);
    }
    if (change.direction)
    {
        if (!CheckDirection(*change.direction, error))
            return false;
        record.direction = static_cast<std::uint8_t>(*change.direction);
    }
    if (change.level)
    {
        if (!CheckLevel(*change.level, error))
            return false;
        record.level = static_cast<std::uint16_t>(*change.level);
    }
    table[static_cast<std::size_t>(number)] = record;
    return true;
}
} // namespace Editor::Gates

#endif // _EDITOR
