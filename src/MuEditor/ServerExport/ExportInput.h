#pragma once

#ifdef _EDITOR

#include "Gates/GateRecord.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// What an OpenMU export of one map is written from (ServerExportFiles gathers it).
namespace Editor::ServerExport
{
// The gates an export sets up (Gate.bmd numbers): the ones the editor added (345 and up)
// on the map, the added enter gates on other maps that lead to it, and the arrivals all
// those enter gates land on. The game's own gates are on the server already, and their
// Gate.bmd records do not match OpenMU's seed everywhere (areas, spawn flags), so they are
// never exported; an added enter gate that lands on one of them (a hand-edited Gate.bmd)
// makes the export look that arrival up, never create it.
struct ExportGates
{
    std::vector<int> enterOnMap;
    std::vector<int> arrivalsOnMap;     // arrivals on the map that the export's enter gates land on
    std::vector<int> enterInto;         // added enter gates on other maps that land on this map
    std::vector<int> arrivalsElsewhere; // arrivals on other maps this map's enter gates land on
};

struct ExportInput
{
    int map = 0;   // the game's number (OpenMU's GameMapDefinition.Number)
    int world = 0; // its Data folder number
    std::string name;
    bool newMap = false; // a map the game does not have (82 and up): the export creates it
    int safezoneMap = 0; // where players who die there return (GameMapDefinition.SafezoneMap)
    const Editor::Gates::GateTable* gates = nullptr;
    ExportGates gateSet;
    std::function<std::string(int map)> mapName;
    // The walk map, written for new maps only: the game's own maps keep the server's own walk
    // map, which differs from the client's on purpose (MAP_EDITOR.md, Attribute).
    std::vector<std::uint8_t> terrainData;
    std::string terrainFile; // its file name in the export folder
    int terrainDiffering = 0;
};

// The gates of `table` that an export of `map` sets up.
ExportGates CollectGates(const Editor::Gates::GateTable& table, int map);

// Every map the gates of `input` touch, `input.map` first.
std::vector<int> MapsTouched(const ExportInput& input);
} // namespace Editor::ServerExport

#endif // _EDITOR
