#include "App/stdafx.h"

#include <doctest.h>

#include "Core/Globals/_crypt.h" // BuxConvert, Gate.bmd's cipher
#include "Gates/GateChecks.h"
#include "Gates/GateDirection.h"
#include "Gates/GateFile.h"
#include "Gates/GateQueries.h"
#include "Gates/GateTableEdit.h"
#include "ServerExport/ExportHowTo.h"
#include "ServerExport/ExportSql.h"
#include "ServerExport/OpenMuDocuments.h"
#include "ServerExport/ServerTerrain.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Editor::Gates;

namespace
{
using Bytes = std::vector<std::uint8_t>;
constexpr int LORENCIA = 0;
constexpr int NEW_MAP = 82;
constexpr int MAP_SIDE = 256;
constexpr std::size_t MAP_CELLS = static_cast<std::size_t>(MAP_SIDE) * MAP_SIDE;
// Decoded from the shipped file: 215 enter and arrival records, the highest 343; 344 is a
// spawn record of Karutan 2 (map 81), which OpenMU's seed has too.
constexpr int SHIPPED_GATES_IN_USE = 215;
constexpr int SHIPPED_HIGHEST_GATE = 343;
constexpr int KARUTAN_SPAWN_GATE = 344;
constexpr int KARUTAN_2 = 81;

void Bux(std::uint8_t* record, std::size_t size)
{
    BuxConvert(record, static_cast<int>(size));
}

Bytes ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

Bytes ShippedGateFile()
{
    // The spelling git tracks: a case-sensitive file system finds no other.
    return ReadFile(std::filesystem::path(MU_REPO_ROOT) / "src/bin/Data/gate.bmd");
}

// The game's own gates of the shipped file: the gates added with the editor (the checkout may
// hold some) are cleared, so the cases below start from the same table on every checkout.
GateTable ShippedTable()
{
    GateTable table;
    std::string error;
    REQUIRE(DecodeGateFile(ShippedGateFile(), Bux, table, error));
    for (int number = FIRST_CUSTOM_GATE; number <= LAST_GATE; ++number)
        table[number] = GateRecord{};
    return table;
}

// Lorencia's east edge to the new map's west edge, as the owner's first outskirts gate.
NewGatePair EastGate()
{
    NewGatePair pair;
    pair.from = {LORENCIA, {247, 92, 248, 97}};
    pair.to = {NEW_MAP, {3, 120, 5, 124}};
    pair.direction = 5; // east
    pair.level = 10;
    return pair;
}

WalkMap OpenWalkMap()
{
    WalkMap walkMap;
    walkMap.tiles.assign(MAP_CELLS, 0);
    return walkMap;
}

void SetTile(WalkMap& walkMap, int x, int y, std::uint16_t value)
{
    walkMap.tiles[static_cast<std::size_t>(y) * MAP_SIDE + x] = value;
}

bool Mentions(const std::vector<std::string>& warnings, const std::string& text)
{
    return std::any_of(warnings.begin(), warnings.end(),
                       [&text](const std::string& warning) { return warning.find(text) != std::string::npos; });
}

std::string MapName(int map)
{
    return map == LORENCIA ? "Lorencia" : map == NEW_MAP ? "Outskirts" : "Map " + std::to_string(map);
}
} // namespace

TEST_CASE("Gate.bmd: the shipped file decodes and encodes back to the same bytes")
{
    const Bytes shipped = ShippedGateFile();
    REQUIRE(shipped.size() == FILE_BYTES);
    GateTable read;
    std::string error;
    REQUIRE(DecodeGateFile(shipped, Bux, read, error));
    CHECK(EncodeGateFile(read, Bux) == shipped);

    const GateTable table = ShippedTable();
    const int inUse = static_cast<int>(
        std::count_if(table.begin(), table.end(), [](const GateRecord& record) { return record.flag != 0; }));
    CHECK(inUse == SHIPPED_GATES_IN_USE);
    int highest = 0;
    for (int number = 0; number < GATE_COUNT; ++number)
    {
        if (table[number].flag != 0)
            highest = number;
    }
    CHECK(highest == SHIPPED_HIGHEST_GATE);
    CHECK(FreeNumbers(table).size() == static_cast<std::size_t>(LAST_GATE - FIRST_CUSTOM_GATE + 1));

    // Lorencia's dungeon entrance: gate 1 lands on gate 2 in the Dungeon (map 1).
    const GateRecord& dungeon = table[1];
    CHECK(dungeon.flag == FLAG_ENTER);
    CHECK(dungeon.map == LORENCIA);
    CHECK(AreaOf(dungeon).x1 == 121);
    CHECK(AreaOf(dungeon).y1 == 232);
    CHECK(AreaOf(dungeon).x2 == 123);
    CHECK(AreaOf(dungeon).y2 == 233);
    CHECK(dungeon.target == 2);
    CHECK(dungeon.level == 20);
    CHECK(dungeon.maxLevel == DEFAULT_MAX_LEVEL);
    CHECK(TargetMap(table, 1) == 1);
    CHECK(std::string(KindName(table[17])) == "spawn");
    CHECK(std::string(KindName(table[4])) == "arrival");
    CHECK(std::string(KindName(table[KARUTAN_SPAWN_GATE])) == "spawn");
    CHECK(table[KARUTAN_SPAWN_GATE].map == KARUTAN_2);
    CHECK(FIRST_CUSTOM_GATE == KARUTAN_SPAWN_GATE + 1);
}

TEST_CASE("Gate.bmd: a file of the wrong size is refused")
{
    Bytes shipped = ShippedGateFile();
    shipped.pop_back();
    GateTable table{};
    std::string error;
    CHECK_FALSE(DecodeGateFile(shipped, Bux, table, error));
    CHECK(error.find("7168") != std::string::npos);
}

TEST_CASE("Gate queries: Lorencia's gates, and who lands where")
{
    const GateTable table = ShippedTable();
    CHECK(GatesOnMap(table, LORENCIA) == std::vector<int>{1, 4, 17, 18, 21, 23, 26, 102, 108});
    const std::vector<int> into = EnterGatesInto(table, LORENCIA);
    CHECK(std::find(into.begin(), into.end(), 3) != into.end()); // Dungeon's way up lands on gate 4
    CHECK(EnterGatesTargeting(table, 4) == std::vector<int>{3});
    CHECK(TargetMap(table, 17) == -1); // a spawn gate leads nowhere
}

TEST_CASE("Gate edits: a pair takes the first free numbers and removing it restores the file")
{
    const Bytes shipped = EncodeGateFile(ShippedTable(), Bux);
    GateTable table = ShippedTable();
    AddedGates added;
    std::string error;
    REQUIRE(AddGatePair(table, EastGate(), added, error));
    CHECK(added.enter == FIRST_CUSTOM_GATE);
    CHECK(added.arrival == FIRST_CUSTOM_GATE + 1);

    const GateRecord& enter = table[added.enter];
    CHECK(enter.flag == FLAG_ENTER);
    CHECK(enter.map == LORENCIA);
    CHECK(enter.target == added.arrival);
    CHECK(enter.level == 10);
    CHECK(enter.maxLevel == DEFAULT_MAX_LEVEL);
    const GateRecord& arrival = table[added.arrival];
    CHECK(arrival.flag == FLAG_ARRIVAL);
    CHECK(arrival.map == NEW_MAP);
    CHECK(arrival.direction == 5);
    CHECK(TargetMap(table, added.enter) == NEW_MAP);
    CHECK(EnterGatesInto(table, NEW_MAP) == std::vector<int>{added.enter});

    // Written and read back, then removed: the shipped bytes again.
    GateTable reread{};
    REQUIRE(DecodeGateFile(EncodeGateFile(table, Bux), Bux, reread, error));
    CHECK(EncodeGateFile(reread, Bux) == EncodeGateFile(table, Bux));
    std::vector<int> removed;
    REQUIRE(RemoveGate(table, added.enter, removed, error));
    CHECK(removed == std::vector<int>{added.enter, added.arrival});
    CHECK(EncodeGateFile(table, Bux) == shipped);
}

TEST_CASE("Gate edits: corners in any order, and the checks on every value")
{
    GateTable table = ShippedTable();
    AddedGates added;
    std::string error;

    NewGatePair swapped = EastGate();
    swapped.from.area = {248, 97, 247, 92};
    REQUIRE(AddGatePair(table, swapped, added, error));
    CHECK(AreaOf(table[added.enter]).x1 == 247);
    CHECK(AreaOf(table[added.enter]).y2 == 97);

    NewGatePair offMap = EastGate();
    offMap.to.area.x2 = 256;
    CHECK_FALSE(AddGatePair(table, offMap, added, error));
    CHECK(error.find("arrival") != std::string::npos);

    NewGatePair badDirection = EastGate();
    badDirection.direction = 9;
    CHECK_FALSE(AddGatePair(table, badDirection, added, error));

    NewGatePair badLevel = EastGate();
    badLevel.level = MAX_LEVEL_REQUIREMENT + 1;
    CHECK_FALSE(AddGatePair(table, badLevel, added, error));

    NewGatePair badMap = EastGate();
    badMap.to.map = 256;
    CHECK_FALSE(AddGatePair(table, badMap, added, error));
}

TEST_CASE("Gate edits: the game's own gates and arrivals in use are not removed")
{
    GateTable table = ShippedTable();
    std::vector<int> removed;
    std::string error;
    CHECK_FALSE(RemoveGate(table, 1, removed, error));
    CHECK(error.find("game's own") != std::string::npos);
    CHECK_FALSE(RemoveGate(table, KARUTAN_SPAWN_GATE, removed, error));
    CHECK_FALSE(RemoveGate(table, FIRST_CUSTOM_GATE, removed, error)); // free
    CHECK_FALSE(RemoveGate(table, GATE_COUNT, removed, error));

    AddedGates added;
    REQUIRE(AddGatePair(table, EastGate(), added, error));
    CHECK_FALSE(RemoveGate(table, added.arrival, removed, error));
    CHECK(error.find(std::to_string(added.enter)) != std::string::npos);
}

TEST_CASE("Gate edits: an arrival two enter gates share stays until the last one goes")
{
    GateTable table = ShippedTable();
    AddedGates first;
    std::string error;
    REQUIRE(AddGatePair(table, EastGate(), first, error));
    // A second way in to the same arrival, written by hand as the game's own gates are.
    NewGatePair second = EastGate();
    second.from.area = {247, 100, 248, 102};
    AddedGates other;
    REQUIRE(AddGatePair(table, second, other, error));
    table[other.enter].target = static_cast<std::uint16_t>(first.arrival);

    std::vector<int> removed;
    REQUIRE(RemoveGate(table, first.enter, removed, error));
    CHECK(removed == std::vector<int>{first.enter});
    CHECK(table[first.arrival].flag == FLAG_ARRIVAL);
    REQUIRE(RemoveGate(table, other.enter, removed, error));
    CHECK(removed == std::vector<int>{other.enter, first.arrival});
}

TEST_CASE("Gate edits: changing an added gate's area, direction and level")
{
    GateTable table = ShippedTable();
    AddedGates added;
    std::string error;
    REQUIRE(AddGatePair(table, EastGate(), added, error));

    GateChange change;
    change.area = TileRect{240, 90, 244, 91};
    change.level = 30;
    REQUIRE(ChangeGate(table, added.enter, change, error));
    CHECK(AreaOf(table[added.enter]).x1 == 240);
    CHECK(table[added.enter].level == 30);

    GateChange turn;
    turn.direction = 7;
    REQUIRE(ChangeGate(table, added.arrival, turn, error));
    CHECK(table[added.arrival].direction == 7);

    GateChange bad;
    bad.direction = 12;
    CHECK_FALSE(ChangeGate(table, added.arrival, bad, error));
    CHECK_FALSE(ChangeGate(table, 1, change, error));
}

TEST_CASE("Gate edits: when every custom number is taken, a pair is refused")
{
    GateTable table = ShippedTable();
    AddedGates added;
    std::string error;
    while (FreeNumbers(table).size() >= 2)
        REQUIRE(AddGatePair(table, EastGate(), added, error));
    CHECK_FALSE(AddGatePair(table, EastGate(), added, error));
    CHECK(error.find("free") != std::string::npos);
}

TEST_CASE("Gate directions: OpenMU's names and numbers")
{
    CHECK(std::string(DirectionName(0)) == "undefined");
    CHECK(std::string(DirectionName(5)) == "east");
    CHECK(std::string(DirectionName(9)) == "?");
    int direction = -1;
    CHECK(DirectionFromName("North-West", direction));
    CHECK(direction == 8);
    CHECK(DirectionFromName("3", direction));
    CHECK(direction == 3);
    CHECK_FALSE(DirectionFromName("up", direction));
    CHECK_FALSE(DirectionFromName("9", direction));
    CHECK(DirectionNames().find("southeast") != std::string::npos);
}

TEST_CASE("Gate checks: walkability of the areas and gates in the way")
{
    GateTable table = ShippedTable();
    WalkMap lorencia = OpenWalkMap();
    WalkMap outskirts = OpenWalkMap();
    NewGatePair pair = EastGate();
    CHECK(PairWarnings(table, pair, lorencia, outskirts).empty());

    SetTile(lorencia, 247, 92, 4);  // blocked
    SetTile(lorencia, 248, 92, 16); // water: the client walks, OpenMU does not
    SetTile(lorencia, 247, 93, 2);  // a character stood there: still walkable for both
    const std::vector<std::string> warnings = PairWarnings(table, pair, lorencia, outskirts);
    CHECK(Mentions(warnings, "1 of 12 tiles of the enter area"));
    CHECK(Mentions(warnings, "1 tiles of the enter area"));

    WalkMap blocked = OpenWalkMap();
    for (int y = 120; y <= 124; ++y)
        for (int x = 3; x <= 5; ++x)
            SetTile(blocked, x, y, 8);
    CHECK(Mentions(PairWarnings(table, pair, lorencia, blocked), "no tile of the arrival area"));

    WalkMap unknown;
    unknown.problem = "no such map";
    CHECK(Mentions(PairWarnings(table, pair, lorencia, unknown), "was not checked: no such map"));

    // An arrival inside Lorencia's dungeon entrance would send players straight on.
    NewGatePair bounce;
    bounce.from = {NEW_MAP, {3, 120, 5, 124}};
    bounce.to = {LORENCIA, {122, 232, 122, 232}};
    CHECK(Mentions(PairWarnings(table, bounce, outskirts, lorencia), "overlaps enter gate 1"));

    NewGatePair loop;
    loop.from = {NEW_MAP, {10, 10, 12, 12}};
    loop.to = {NEW_MAP, {11, 11, 11, 11}};
    CHECK(Mentions(PairWarnings(table, loop, outskirts, outskirts), "bounce"));
}

TEST_CASE("Gate checks: an arrival nobody can leave and an endless bounce are traps")
{
    WalkMap open = OpenWalkMap();
    CHECK(PairTraps(EastGate(), open).empty());

    WalkMap walled = OpenWalkMap();
    for (int y = 120; y <= 124; ++y)
        for (int x = 3; x <= 5; ++x)
            SetTile(walled, x, y, 4);
    CHECK(Mentions(PairTraps(EastGate(), walled), "could not move"));
    SetTile(walled, 4, 122, 0); // one walkable tile is enough to step off
    CHECK(PairTraps(EastGate(), walled).empty());

    WalkMap unknown;
    unknown.problem = "no such map";
    CHECK(PairTraps(EastGate(), unknown).empty()); // not known: a warning, not a trap

    NewGatePair loop;
    loop.from = {NEW_MAP, {10, 10, 12, 12}};
    loop.to = {NEW_MAP, {11, 11, 11, 11}};
    CHECK(Mentions(PairTraps(loop, open), "without end"));
    loop.to = {NEW_MAP, {20, 10, 22, 12}};
    CHECK(PairTraps(loop, open).empty());
}

TEST_CASE("Server export: the walk map in OpenMU's layout")
{
    std::vector<std::uint16_t> tiles(MAP_CELLS, 0);
    tiles[1] = 1;
    tiles[2] = 4;
    tiles[3] = 16;     // water: walkable in the client, blocked on the server
    tiles[4] = 0x0102; // a character's bit and a flag the file cannot hold
    const Editor::ServerExport::ServerTerrain terrain = Editor::ServerExport::BuildTerrainData(tiles);
    REQUIRE(terrain.data.size() == Editor::ServerExport::TERRAIN_DATA_BYTES);
    CHECK(terrain.data[0] == 0);
    CHECK(terrain.data[1] == 255);
    CHECK(terrain.data[2] == 255);
    CHECK(terrain.data[3 + 1] == 1);
    CHECK(terrain.data[3 + 2] == 4);
    CHECK(terrain.data[3 + 3] == 16);
    CHECK(terrain.data[3 + 4] == 0);
    CHECK(terrain.differing == 1);
    CHECK(Editor::ServerExport::BuildTerrainData({}).data.empty());
}

TEST_CASE("Server export: gates, map, HOWTO and SQL for a new map joined to Lorencia")
{
    GateTable table = ShippedTable();
    AddedGates out;
    AddedGates back;
    std::string error;
    REQUIRE(AddGatePair(table, EastGate(), out, error));
    NewGatePair returnGate;
    returnGate.from = {NEW_MAP, {0, 120, 1, 124}};
    returnGate.to = {LORENCIA, {244, 92, 245, 97}};
    returnGate.direction = 1;
    REQUIRE(AddGatePair(table, returnGate, back, error));

    Editor::ServerExport::ExportInput input;
    input.map = NEW_MAP;
    input.world = NEW_MAP + 1;
    input.name = "Lorencia's Outskirts";
    input.newMap = true;
    input.safezoneMap = LORENCIA;
    input.gates = &table;
    input.gateSet = Editor::ServerExport::CollectGates(table, NEW_MAP);
    input.mapName = MapName;
    input.terrainData = Editor::ServerExport::BuildTerrainData(std::vector<std::uint16_t>(MAP_CELLS, 0)).data;
    input.terrainFile = "terrain_map82_server.att";

    CHECK(input.gateSet.enterOnMap == std::vector<int>{back.enter});
    CHECK(input.gateSet.arrivalsOnMap == std::vector<int>{out.arrival});
    CHECK(input.gateSet.enterInto == std::vector<int>{out.enter});
    CHECK(input.gateSet.arrivalsElsewhere == std::vector<int>{back.arrival});
    CHECK(Editor::ServerExport::MapsTouched(input) == std::vector<int>{NEW_MAP, LORENCIA});

    const nlohmann::ordered_json gates = Editor::ServerExport::GatesDocument(input);
    REQUIRE(gates["Maps"].size() == 2);
    // OpenMU's Map Editor import deletes a map's spawns for any object whose FormatVersion is
    // missing or "1.0": the document and each group must carry another one, and no Spawns.
    CHECK(gates["FormatVersion"].is_string());
    CHECK(gates["FormatVersion"] != "1.0");
    const auto& own = gates["Maps"][0];
    CHECK(own["Map"] == NEW_MAP);
    CHECK(own["FormatVersion"] == gates["FormatVersion"]);
    CHECK_FALSE(own.contains("Spawns"));
    CHECK(own["ExitGates"][0]["ClientGate"] == out.arrival);
    CHECK(own["ExitGates"][0]["IsSpawnGate"] == false);
    CHECK(own["ExitGates"][0]["Direction"] == 5);
    CHECK(own["EnterGates"][0]["Number"] == back.enter);
    CHECK(own["EnterGates"][0]["TargetGateId"] == Editor::ServerExport::GateId(back.arrival));
    const auto& lorencia = gates["Maps"][1];
    CHECK(lorencia["Map"] == LORENCIA);
    CHECK(lorencia["EnterGates"][0]["Number"] == out.enter);
    CHECK(lorencia["EnterGates"][0]["TargetGateId"] == own["ExitGates"][0]["Id"]);
    CHECK(lorencia["ExitGates"][0]["ClientGate"] == back.arrival);

    const nlohmann::ordered_json map = Editor::ServerExport::MapDocument(input);
    CHECK(map["Number"] == NEW_MAP);
    CHECK(map["SafezoneMap"]["Number"] == LORENCIA);
    CHECK(map["ClientFolder"] == "Data/World83");
    CHECK(map["FormatVersion"] != "1.0");

    const std::string howTo = Editor::ServerExport::HowToText(input);
    CHECK(howTo.find("Number is **82**") != std::string::npos);
    CHECK(howTo.find("Data/World83") != std::string::npos);
    CHECK(howTo.find("Number **345**") != std::string::npos);
    CHECK(howTo.find("terrain_map82_server.att") != std::string::npos);
    CHECK(howTo.find("git add src/bin/Data/gate.bmd") != std::string::npos);
    CHECK(howTo.find("Gate.bmd src/bin") == std::string::npos);
    CHECK(howTo.find("Do not load `map.json` or `gates.json`") != std::string::npos);

    const std::string sql = Editor::ServerExport::SqlScript(input);
    CHECK(sql.rfind("-- NOT APPLIED", 0) == 0);
    CHECK(sql.find("\\set ON_ERROR_STOP on") != std::string::npos);
    // The name as UTF-8 hex, never as text: "Lorencia's Outskirts".
    CHECK(sql.find("Outskirts") == std::string::npos);
    CHECK(sql.find("convert_from(decode('4c6f72656e6369612773204f7574736b69727473', 'hex'), 'UTF8')") !=
          std::string::npos);
    CHECK(sql.find("\"Number\" = 345") != std::string::npos);
    CHECK(sql.find("exit_346 := gen_random_uuid();") != std::string::npos);
    CHECK(sql.find("decode('00ffff00") != std::string::npos);
    CHECK(sql.find("COMMIT;") != std::string::npos);
}

TEST_CASE("Server export: a map name cannot end the script's quoting")
{
    Editor::ServerExport::ExportInput input;
    input.map = NEW_MAP;
    input.world = NEW_MAP + 1;
    input.name = "x$mu$;ROLLBACK;DROP SCHEMA config CASCADE;--\nnext";
    input.newMap = true;
    input.terrainData = Editor::ServerExport::BuildTerrainData(std::vector<std::uint16_t>(MAP_CELLS, 0)).data;
    const std::string sql = Editor::ServerExport::SqlScript(input);
    CHECK(sql.find("DROP SCHEMA") == std::string::npos);
    CHECK(sql.find("x$mu$") == std::string::npos);
    const std::string blockOpen = "DO $mu$";
    const std::size_t blockStart = sql.find(blockOpen);
    REQUIRE(blockStart != std::string::npos);
    // One block, closed at its end.
    CHECK(sql.find("$mu$", blockStart + blockOpen.size()) == sql.find("$mu$;\n\nCOMMIT;"));
}

TEST_CASE("Server export: only the gates the editor added, and the game's arrivals only looked up")
{
    GateTable table = ShippedTable();
    AddedGates out;
    AddedGates back;
    std::string error;
    REQUIRE(AddGatePair(table, EastGate(), out, error));
    NewGatePair returnGate;
    returnGate.from = {NEW_MAP, {0, 120, 1, 124}};
    returnGate.to = {LORENCIA, {244, 92, 245, 97}};
    REQUIRE(AddGatePair(table, returnGate, back, error));

    // Devias and the other game maps: nothing the editor added, so nothing to export.
    const Editor::ServerExport::ExportGates devias = Editor::ServerExport::CollectGates(table, 2);
    CHECK(devias.enterOnMap.empty());
    CHECK(devias.arrivalsOnMap.empty());
    CHECK(devias.enterInto.empty());
    CHECK(devias.arrivalsElsewhere.empty());

    // Lorencia: the added pair both ways, none of its own nine gates.
    const Editor::ServerExport::ExportGates lorencia = Editor::ServerExport::CollectGates(table, LORENCIA);
    CHECK(lorencia.enterOnMap == std::vector<int>{out.enter});
    CHECK(lorencia.arrivalsOnMap == std::vector<int>{back.arrival});
    CHECK(lorencia.enterInto == std::vector<int>{back.enter});
    CHECK(lorencia.arrivalsElsewhere == std::vector<int>{out.arrival});

    // A hand-edited table: the way back lands on Lorencia's own arrival 4 instead.
    table[back.enter].target = 4;
    Editor::ServerExport::ExportInput input;
    input.map = NEW_MAP;
    input.world = NEW_MAP + 1;
    input.name = "Outskirts";
    input.gates = &table;
    input.gateSet = Editor::ServerExport::CollectGates(table, NEW_MAP);
    input.mapName = MapName;
    CHECK(input.gateSet.arrivalsElsewhere == std::vector<int>{4});
    const std::string sql = Editor::ServerExport::SqlScript(input);
    CHECK(sql.find("exit_4 := gen_random_uuid();") == std::string::npos);
    CHECK(sql.find("the exit gate of Gate.bmd 4") != std::string::npos);
    CHECK(sql.find(", true);") == std::string::npos); // no spawn gate is ever created
    CHECK(Editor::ServerExport::HowToText(input).find("on the server already; nothing to create") != std::string::npos);
    const nlohmann::ordered_json gates = Editor::ServerExport::GatesDocument(input);
    CHECK(gates["Maps"][1]["ExitGates"][0]["AlreadyOnServer"] == true);
}

TEST_CASE("Server export: a game map keeps the server's walk map")
{
    const GateTable table = ShippedTable();
    Editor::ServerExport::ExportInput input;
    input.map = LORENCIA;
    input.world = 1;
    input.name = "Lorencia";
    input.gates = &table;
    input.gateSet = Editor::ServerExport::CollectGates(table, LORENCIA);
    input.mapName = MapName;
    const std::string sql = Editor::ServerExport::SqlScript(input);
    CHECK(sql.find("INSERT INTO config.\"GameMapDefinition\"") == std::string::npos);
    CHECK(sql.find("'map 0 is not in the database'") != std::string::npos);
    CHECK(Editor::ServerExport::HowToText(input).find("one of the game's own maps") != std::string::npos);
}
