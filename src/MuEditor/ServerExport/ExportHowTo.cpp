#include "ExportHowTo.h"

#ifdef _EDITOR

#include "Gates/GateDirection.h"
#include "Gates/GateQueries.h"

#include <algorithm>
#include <sstream>

namespace Editor::ServerExport
{
namespace
{
namespace Gates = Editor::Gates;

const Gates::GateRecord& Record(const ExportInput& input, int number)
{
    return (*input.gates)[static_cast<std::size_t>(number)];
}

std::string MapLabel(const ExportInput& input, int map)
{
    const std::string name = input.mapName ? input.mapName(map) : "";
    return "map " + std::to_string(map) + (name.empty() ? "" : " (" + name + ")");
}

// Low corner first, as the Admin Panel stores it.
std::string AreaText(const Gates::GateRecord& record)
{
    return "X1 " + std::to_string(std::min(record.x1, record.x2)) + ", Y1 " +
           std::to_string(std::min(record.y1, record.y2)) + ", X2 " + std::to_string(std::max(record.x1, record.x2)) +
           ", Y2 " + std::to_string(std::max(record.y1, record.y2));
}

void WriteIntro(std::ostringstream& out, const ExportInput& input)
{
    out << "# Setting up " << MapLabel(input, input.map) << " on OpenMU\n\n"
        << "Written by the MuMain Map Editor. **Nothing here has been applied to a server**: you apply it, in the "
           "Admin Panel (steps below) or with `openmu.sql`.\n\n"
        << "**Numbers:** the server's map Number is **" << input.map << "**; the client keeps the map in the folder "
        << "**Data/World" << input.world << "** (the folder number is the map number + 1). Gates keep their "
        << "Gate.bmd numbers on both sides.\n\n"
        << "Files in this folder:\n\n";
    if (!input.terrainFile.empty())
        out << "- `" << input.terrainFile << "`: the walk map (the map's Terrain Data, 65539 bytes, not encrypted)\n";
    out << "- `map.json`: the game map definition, to read while you fill in step 1\n"
        << "- `gates.json`: the gates, grouped by map, to read while you fill in step 3\n"
        << "- `openmu.sql`: steps 1 to 3 as a script for OpenMU's PostgreSQL database, **not applied**\n"
        << "- `HOWTO.md`: this file\n\n"
        << "**Do not load `map.json` or `gates.json` with the Admin Panel's Map Editor Import button.** That import "
           "is for monster spawns: it deletes every spawn of the selected map and ignores gates. (These files carry "
           "a format of their own, so the import stops before it deletes anything, but it creates nothing either.)\n\n"
        << "These steps follow the OpenMU Admin Panel's code; they were not clicked through on a live server.\n\n";
}

void WriteClientStep(std::ostringstream& out, const ExportInput& input)
{
    out << "## 0. The client\n\n";
    if (input.newMap)
        out << "Every client that should show the map needs `Data/World" << input.world << "`, `Data/Object"
            << input.world << "` and the gate table `Data/gate.bmd` from the checkout's `src/bin/Data`.";
    else
        out << "Every client needs the gate table `Data/gate.bmd` from the checkout's `src/bin/Data`.";
    out << " Git tracks the gate table as `src/bin/Data/gate.bmd` (lowercase: use that spelling, or a "
        << "case-sensitive checkout gets a second file)";
    if (input.newMap)
        out << "; the new map's folders are new files under `src/bin`, which git ignores until they are added "
            << "with `-f`";
    out << ":\n\n```sh\ngit add src/bin/Data/gate.bmd\n";
    if (input.newMap)
        out << "git add -f src/bin/Data/World" << input.world << " src/bin/Data/Object" << input.world << "\n";
    out << "```\n\n";
}

void WriteMapStep(std::ostringstream& out, const ExportInput& input)
{
    out << "## 1. The map\n\n";
    if (!input.newMap)
    {
        out << "Map " << input.map << " is one of the game's own maps; OpenMU has it already. Its walk map is not "
            << "part of this export: the server's copy differs from the client's on purpose. To change it, use the "
            << "Map Editor's Attribute tab (\"Save server .att\").\n\n"
            << "## 2. Host it\n\nNothing to do: the game servers host the game's own maps.\n\n";
        return;
    }
    out << "Admin Panel, **Game configuration > Game maps**, create a new entry:\n\n"
        << "- **Number:** " << input.map << "\n"
        << "- **Name:** " << input.name << "\n"
        << "- **Discriminator:** 0\n"
        << "- **Exp multiplier:** 1\n"
        << "- **Safezone map:** " << MapLabel(input, input.safezoneMap) << " (where players who die there return)\n"
        << "- **Terrain Data:** upload `" << input.terrainFile << "` from this folder\n\n"
        << "Save. Without the Terrain Data OpenMU lets players walk only on rows y < 128.";
    if (input.terrainDiffering > 0)
        out << " " << input.terrainDiffering << " tiles are walkable in the client but blocked on the server "
            << "(water or combined walkability values).";
    out << "\n\n## 2. Host it\n\n"
        << "Admin Panel start page (the server list): click your **game server's name**, open its **Server "
        << "configuration**, add **" << input.name << "** to **Maps** and save. Do this for every game server "
        << "that should host the map; a map that is not listed is not hosted.\n\n";
}

void WriteExitGate(std::ostringstream& out, const ExportInput& input, int number)
{
    const Gates::GateRecord& record = Record(input, number);
    if (!Gates::IsCustomNumber(number))
    {
        out << "- **Exit gate** (Gate.bmd " << number << "): " << AreaText(record)
            << ": one of the game's own, on the server already; nothing to create\n";
        return;
    }
    out << "- **Exit gate** (Gate.bmd " << number << "): " << AreaText(record) << ", Direction "
        << static_cast<int>(record.direction) << " (" << Gates::DirectionName(record.direction)
        << ": the way players face when they arrive), Is spawn gate off\n";
}

void WriteEnterGate(std::ostringstream& out, const ExportInput& input, int number)
{
    const Gates::GateRecord& record = Record(input, number);
    const int targetMap = Gates::TargetMap(*input.gates, number);
    out << "- **Enter gate** Number **" << number << "**: " << AreaText(record) << ", Level requirement "
        << record.level;
    if (targetMap < 0)
    {
        out << ", no arrival gate to land on (Gate.bmd " << record.target << "): leave it out\n";
        return;
    }
    out << ", Target gate: the exit gate on " << MapLabel(input, targetMap) << " at "
        << AreaText(Record(input, record.target)) << " (Gate.bmd " << record.target << ")\n";
}

void WriteGateMap(std::ostringstream& out, const ExportInput& input, int map, const std::vector<int>& exits,
                  const std::vector<int>& enters)
{
    out << "### " << MapLabel(input, map) << "\n\n";
    for (const int number : exits)
        WriteExitGate(out, input, number);
    for (const int number : enters)
        WriteEnterGate(out, input, number);
    if (exits.empty() && enters.empty())
        out << "- nothing to create here\n";
    out << "\n";
}

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

void WriteGateStep(std::ostringstream& out, const ExportInput& input)
{
    out << "## 3. Gates\n\n"
        << "Admin Panel, **Game configuration > Map Editor**. For each map below choose the map, add the exit "
        << "gates first (area and direction), then the enter gates (their **Number**, area, level requirement "
        << "and target gate, which is the exit gate named). An enter gate's Number must be its Gate.bmd number: "
        << "the client sends that number when a player walks in, and OpenMU looks it up among the enter gates of "
        << "the player's map. Only the gates the Map Editor added (345 and up) are listed: the game's own gates "
        << "are on the server already. Add them by hand; the Map Editor's Import button is for monster spawns "
        << "and would delete the map's spawns.\n\n";
    if (input.gates == nullptr)
        return;
    const ExportGates& set = input.gateSet;
    WriteGateMap(out, input, input.map, set.arrivalsOnMap, set.enterOnMap);
    for (const int map : MapsTouched(input))
    {
        if (map != input.map)
            WriteGateMap(out, input, map, OnMap(input, set.arrivalsElsewhere, map), OnMap(input, set.enterInto, map));
    }
}

void WriteClosing(std::ostringstream& out)
{
    out << "## 4. Optional\n\n"
        << "- **Monsters:** in the Map Editor, add spawn areas with existing monster numbers (a new kind of monster "
        << "also needs client code).\n"
        << "- **/move:** Game configuration > Warp list, an entry whose gate is the map's exit gate.\n\n"
        << "## 5. Restart and try it\n\n"
        << "Restart the game server so it loads the map and the gates, log in and walk into the enter gate.\n\n"
        << "## When it does not work\n\n"
        << "- **Nothing happens at the gate:** the enter gate's Number on the server is not its Gate.bmd number, "
        << "or the client's Gate.bmd is an older one.\n"
        << "- **Players only walk on half the map:** the Terrain Data was not uploaded.\n"
        << "- **The warp is refused:** the map is not in the game server's Maps list, or the level requirement is "
        << "higher than the character's level.\n"
        << "- **Players walk through walls and snap back:** the client's and the server's walk maps differ; save "
        << "the client's walk map, export again and upload the new Terrain Data.\n";
}
} // namespace

std::string HowToText(const ExportInput& input)
{
    std::ostringstream out;
    WriteIntro(out, input);
    WriteClientStep(out, input);
    WriteMapStep(out, input);
    WriteGateStep(out, input);
    WriteClosing(out);
    return out.str();
}
} // namespace Editor::ServerExport

#endif // _EDITOR
