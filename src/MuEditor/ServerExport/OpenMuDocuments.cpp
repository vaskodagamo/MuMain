#include "OpenMuDocuments.h"

#ifdef _EDITOR

#include "Gates/GateQueries.h"

#include <algorithm>
#include <cstdio>

namespace Editor::ServerExport
{
namespace
{
namespace Gates = Editor::Gates;
using nlohmann::ordered_json;

// OpenMU's Map Editor imports a map's monster spawns from JSON (MapExportImportService): it
// deletes every spawn of the selected map first, then adds the file's "Spawns", and takes
// any object whose "FormatVersion" is missing or "1.0". These files are not for that
// import: their own FormatVersion makes it stop before it deletes anything.
constexpr const char* GATES_FORMAT = "mumain-map-editor/gates-1";
constexpr const char* MAP_FORMAT = "mumain-map-editor/map-1";
constexpr const char* GATE_ID_FORMAT = "6d750000-0000-4000-8000-%012x";
constexpr std::size_t GATE_ID_CHARS = 40;
constexpr int NORMAL_DISCRIMINATOR = 0;
constexpr double DEFAULT_EXP_MULTIPLIER = 1.0;
constexpr const char* GATES_NOTE =
    "Gates the MuMain Map Editor added to Data/Gate.bmd, for OpenMU, grouped by map. Create them in the Admin "
    "Panel (HOWTO.md) or with openmu.sql. Do NOT load this file with the Map Editor's Import button: that import "
    "replaces the map's monster spawns and ignores gates. An enter gate's Number must stay its Gate.bmd number: "
    "the client sends it. Ids only link the entries of this file.";
constexpr const char* MAP_NOTE = "A game map for OpenMU's Admin Panel (HOWTO.md, step 1), written by the MuMain "
                                 "Map Editor. Not a file for the Map Editor's Import button.";

const Gates::GateRecord& Record(const ExportInput& input, int number)
{
    return (*input.gates)[static_cast<std::size_t>(number)];
}

// The area low corner first, as OpenMU stores gates.
ordered_json AreaFields(const Gates::GateRecord& record)
{
    return {{"X1", std::min(record.x1, record.x2)},
            {"Y1", std::min(record.y1, record.y2)},
            {"X2", std::max(record.x1, record.x2)},
            {"Y2", std::max(record.y1, record.y2)}};
}

// An added arrival is never a spawn gate; a game's own one it lands on is on the server
// already (AlreadyOnServer: find it, do not create it).
ordered_json ExitGateJson(const ExportInput& input, int number)
{
    const Gates::GateRecord& record = Record(input, number);
    ordered_json gate = {{"Id", GateId(number)}};
    gate.update(AreaFields(record));
    gate["Direction"] = record.direction;
    gate["DirectionMeaning"] = "where players face when they arrive";
    gate["IsSpawnGate"] = false;
    gate["ClientGate"] = number;
    if (!Gates::IsCustomNumber(number))
        gate["AlreadyOnServer"] = true;
    return gate;
}

ordered_json EnterGateJson(const ExportInput& input, int number)
{
    const Gates::GateRecord& record = Record(input, number);
    const int targetMap = Gates::TargetMap(*input.gates, number);
    ordered_json gate = {{"Id", GateId(number)}};
    gate.update(AreaFields(record));
    gate["LevelRequirement"] = record.level;
    gate["Number"] = number;
    gate["TargetGateId"] = GateId(record.target);
    gate["TargetMap"] = targetMap;
    gate["TargetMapName"] = input.mapName ? input.mapName(targetMap) : "";
    gate["TargetClientGate"] = record.target;
    return gate;
}

// The gates of `numbers` that lie on `map`.
std::vector<int> OnMap(const ExportInput& input, const std::vector<int>& numbers, int map)
{
    std::vector<int> onMap;
    for (const int number : numbers)
    {
        if (Record(input, number).map == map)
            onMap.push_back(number);
    }
    return onMap;
}

ordered_json MapGroup(const ExportInput& input, int map, const std::vector<int>& exits, const std::vector<int>& enters)
{
    ordered_json group = {{"Map", map}, {"MapName", input.mapName ? input.mapName(map) : ""}};
    group["FormatVersion"] = GATES_FORMAT;
    group["ExitGates"] = ordered_json::array();
    for (const int number : exits)
        group["ExitGates"].push_back(ExitGateJson(input, number));
    group["EnterGates"] = ordered_json::array();
    for (const int number : enters)
        group["EnterGates"].push_back(EnterGateJson(input, number));
    return group;
}
} // namespace

std::string GateId(int number)
{
    char id[GATE_ID_CHARS];
    std::snprintf(id, sizeof(id), GATE_ID_FORMAT, static_cast<unsigned>(number));
    return id;
}

ordered_json GatesDocument(const ExportInput& input)
{
    ordered_json document = {{"Note", GATES_NOTE}, {"FormatVersion", GATES_FORMAT}};
    document["Map"] = input.map;
    document["MapName"] = input.name;
    document["Maps"] = ordered_json::array();
    if (input.gates == nullptr)
        return document;

    const ExportGates& set = input.gateSet;
    document["Maps"].push_back(MapGroup(input, input.map, set.arrivalsOnMap, set.enterOnMap));
    for (const int map : MapsTouched(input))
    {
        if (map == input.map)
            continue;
        document["Maps"].push_back(
            MapGroup(input, map, OnMap(input, set.arrivalsElsewhere, map), OnMap(input, set.enterInto, map)));
    }
    return document;
}

ordered_json MapDocument(const ExportInput& input)
{
    ordered_json document = {{"Note", MAP_NOTE}, {"FormatVersion", MAP_FORMAT}};
    document["Number"] = input.map;
    document["Name"] = input.name;
    document["Discriminator"] = NORMAL_DISCRIMINATOR;
    document["ExpMultiplier"] = DEFAULT_EXP_MULTIPLIER;
    document["SafezoneMap"] = {{"Number", input.safezoneMap},
                               {"Name", input.mapName ? input.mapName(input.safezoneMap) : ""}};
    document["TerrainData"] = input.terrainFile.empty() ? ordered_json(nullptr) : ordered_json(input.terrainFile);
    document["NewMap"] = input.newMap;
    document["ClientFolder"] = "Data/World" + std::to_string(input.world);
    document["MonsterSpawns"] = ordered_json::array();
    document["DropItemGroups"] = ordered_json::array();
    return document;
}
} // namespace Editor::ServerExport

#endif // _EDITOR
