#pragma once

#ifdef _EDITOR

#include "GateRecord.h"

#include <optional>
#include <string>
#include <vector>

// Adding, changing and removing the gates the editor adds to Gate.bmd. The game's own
// gates (numbers below FIRST_CUSTOM_GATE) are only read.
namespace Editor::Gates
{
// A rectangle of tiles, both corners included.
struct TileRect
{
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
};

// One side of a gate pair: a map (the game's number, Lorencia 0) and an area on it.
struct GateEnd
{
    int map = 0;
    TileRect area;
};

// A one-way connection: walking into `from` warps to `to`. A way back is a second pair.
struct NewGatePair
{
    GateEnd from;           // the enter gate
    GateEnd to;             // the arrival gate
    int direction = 0;      // where an arriving player faces (GateDirection.h)
    int level = 0;          // the level needed to use the gate
    bool allowTrap = false; // add it even where players would be stuck (GateChecks.h, PairTraps)
};

struct AddedGates
{
    int enter = -1;
    int arrival = -1;
};

// What to change on a gate the editor added; unset fields stay as they are.
struct GateChange
{
    std::optional<TileRect> area;
    std::optional<int> direction; // arrival gates
    std::optional<int> level;     // enter gates
};

// Puts the corners in order. False with the reason when a corner lies off the map.
bool NormalizeArea(TileRect& area, std::string& error);
TileRect AreaOf(const GateRecord& record);
bool Overlaps(const TileRect& a, const TileRect& b);
int TileCount(const TileRect& area);

// The free numbers from FIRST_CUSTOM_GATE on, lowest first.
std::vector<int> FreeNumbers(const GateTable& table);

// Writes `pair` into the two lowest free numbers: the enter gate first, then its arrival
// gate. False with the reason, and `table` unchanged, when a map number, an area, the
// direction or the level is out of range, or fewer than two numbers are free.
bool AddGatePair(GateTable& table, NewGatePair pair, AddedGates& added, std::string& error);

// Removes gate `number`, which the editor added: an enter gate together with its arrival
// gate when no other enter gate lands there, an arrival gate only when no enter gate lands
// on it. `removed` lists the numbers cleared. False with the reason, and `table`
// unchanged, otherwise.
bool RemoveGate(GateTable& table, int number, std::vector<int>& removed, std::string& error);

// Applies `change` to gate `number`, which the editor added. False with the reason, and
// `table` unchanged, when the gate is not one or a value is out of range.
bool ChangeGate(GateTable& table, int number, const GateChange& change, std::string& error);
} // namespace Editor::Gates

#endif // _EDITOR
