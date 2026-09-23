#include "ExportSql.h"

#ifdef _EDITOR

#include "Gates/GateQueries.h"
#include "Gates/GateTableEdit.h" // TileRect

#include <algorithm>
#include <sstream>

namespace Editor::ServerExport
{
namespace
{
namespace Gates = Editor::Gates;

constexpr const char* HEX_DIGITS = "0123456789abcdef";
constexpr int HIGH_NIBBLE_SHIFT = 4;
constexpr unsigned LOW_NIBBLE_MASK = 0x0F;
// The columns each INSERT fills (OpenMU's EF model, schema "config").
constexpr const char* MAP_COLUMNS = R"(("Id", "GameConfigurationId", "Number", "Discriminator", "Name", )"
                                    R"("ExpMultiplier", "SafezoneMapId", "TerrainData"))";
constexpr const char* HOSTING_COLUMNS = R"(("GameServerConfigurationId", "GameMapDefinitionId"))";
constexpr const char* EXIT_GATE_COLUMNS = R"(("Id", "MapId", "X1", "Y1", "X2", "Y2", "Direction", "IsSpawnGate"))";
constexpr const char* ENTER_GATE_COLUMNS =
    R"(("Id", "GameMapDefinitionId", "Number", "LevelRequirement", "TargetGateId", "X1", "Y1", "X2", "Y2"))";

const Gates::GateRecord& Record(const ExportInput& input, int number)
{
    return (*input.gates)[static_cast<std::size_t>(number)];
}

std::string MapVariable(int map)
{
    return "map_" + std::to_string(map);
}

std::string ExitVariable(int number)
{
    return "exit_" + std::to_string(number);
}

std::string Hex(const std::vector<std::uint8_t>& bytes)
{
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes)
    {
        hex.push_back(HEX_DIGITS[byte >> HIGH_NIBBLE_SHIFT]);
        hex.push_back(HEX_DIGITS[byte & LOW_NIBBLE_MASK]);
    }
    return hex;
}

// Free text (a map name comes from a file anyone may edit) as a SQL expression that holds
// no quote, dollar sign or other character of the text: its UTF-8 bytes in hex. A name
// can then never end the $mu$ block early and run statements of its own.
std::string TextExpression(const std::string& text)
{
    return "convert_from(decode('" + Hex(std::vector<std::uint8_t>(text.begin(), text.end())) + "', 'hex'), 'UTF8')";
}

// The gate's area with its corners in order: the game's own Gate.bmd has some the other
// way round, and OpenMU's gates are stored low corner first.
Gates::TileRect OrderedArea(const Gates::GateRecord& record)
{
    return {std::min(record.x1, record.x2), std::min(record.y1, record.y2), std::max(record.x1, record.x2),
            std::max(record.y1, record.y2)};
}

std::string AreaMatch(const Gates::GateRecord& record)
{
    const Gates::TileRect area = OrderedArea(record);
    return "\"X1\" = " + std::to_string(area.x1) + " AND \"Y1\" = " + std::to_string(area.y1) +
           " AND \"X2\" = " + std::to_string(area.x2) + " AND \"Y2\" = " + std::to_string(area.y2);
}

std::string AreaValues(const Gates::GateRecord& record)
{
    const Gates::TileRect area = OrderedArea(record);
    return std::to_string(area.x1) + ", " + std::to_string(area.y1) + ", " + std::to_string(area.x2) + ", " +
           std::to_string(area.y2);
}

std::vector<int> ExitGates(const ExportInput& input)
{
    std::vector<int> exits = input.gateSet.arrivalsOnMap;
    exits.insert(exits.end(), input.gateSet.arrivalsElsewhere.begin(), input.gateSet.arrivalsElsewhere.end());
    return exits;
}

std::vector<int> EnterGates(const ExportInput& input)
{
    std::vector<int> enters = input.gateSet.enterOnMap;
    enters.insert(enters.end(), input.gateSet.enterInto.begin(), input.gateSet.enterInto.end());
    return enters;
}

void WriteHeader(std::ostringstream& out, const ExportInput& input)
{
    out << "-- NOT APPLIED. Written by the MuMain Map Editor for OpenMU's PostgreSQL database; nothing has run it.\n"
        << "-- It does steps 1 to 3 of HOWTO.md for map " << input.map << " (client folder Data/World" << input.world
        << "), for the gates the editor added only.\n"
        << "-- Read it first and back the database up (pg_dump). Then run it with psql, for example:\n"
        << "--   docker exec -i <postgres container> psql -U <user> -d <database> < openmu.sql\n"
        << "-- and restart the game server. psql stops at the first error (ON_ERROR_STOP below), and the\n"
        << "-- whole script is one transaction: when it stops, nothing has changed. Needs PostgreSQL 13 or\n"
        << "-- newer (gen_random_uuid).\n\n"
        << "\\set ON_ERROR_STOP on\n\n";
}

void WriteDeclarations(std::ostringstream& out, const ExportInput& input)
{
    out << "BEGIN;\n\nDO $mu$\nDECLARE\n    game_configuration uuid;\n";
    for (const int map : MapsTouched(input))
        out << "    " << MapVariable(map) << " uuid;\n";
    for (const int number : ExitGates(input))
        out << "    " << ExitVariable(number) << " uuid;\n";
    out << "BEGIN\n"
        << "    SELECT \"Id\" INTO game_configuration FROM config.\"GameConfiguration\" LIMIT 1;\n"
        << "    IF game_configuration IS NULL THEN\n"
        << "        RAISE EXCEPTION 'this database has no game configuration';\n    END IF;\n";
}

void WriteFindMap(std::ostringstream& out, int map)
{
    out << "    SELECT \"Id\" INTO " << MapVariable(map)
        << " FROM config.\"GameMapDefinition\" WHERE \"Number\" = " << map << " AND \"Discriminator\" = 0 LIMIT 1;\n"
        << "    IF " << MapVariable(map) << " IS NULL THEN\n"
        << "        RAISE EXCEPTION 'map " << map << " is not in the database';\n    END IF;\n";
}

void WriteNewMap(std::ostringstream& out, const ExportInput& input)
{
    const std::string map = MapVariable(input.map);
    out << "\n    -- Step 1: the map, with its walk map (Terrain Data).\n"
        << "    IF EXISTS (SELECT 1 FROM config.\"GameMapDefinition\" WHERE \"Number\" = " << input.map << ") THEN\n"
        << "        RAISE EXCEPTION 'map " << input.map << " is already in the database; nothing was changed';\n"
        << "    END IF;\n"
        << "    " << map << " := gen_random_uuid();\n"
        << "    INSERT INTO config.\"GameMapDefinition\" " << MAP_COLUMNS << "\n"
        << "    VALUES (" << map << ", game_configuration, " << input.map << ", 0, " << TextExpression(input.name)
        << ", 1.0,\n"
        << "        (SELECT \"Id\" FROM config.\"GameMapDefinition\" WHERE \"Number\" = " << input.safezoneMap
        << " AND \"Discriminator\" = 0 LIMIT 1),\n"
        << "        decode('" << Hex(input.terrainData) << "', 'hex'));\n"
        << "\n    -- Step 2: every game server configuration hosts it.\n"
        << "    INSERT INTO config.\"GameServerConfigurationGameMapDefinition\" " << HOSTING_COLUMNS << "\n"
        << "    SELECT \"Id\", " << map << " FROM config.\"GameServerConfiguration\";\n";
}

void WriteFindExitGate(std::ostringstream& out, int number, const Gates::GateRecord& record)
{
    out << "    SELECT \"Id\" INTO " << ExitVariable(number)
        << " FROM config.\"ExitGate\" WHERE \"MapId\" = " << MapVariable(record.map) << " AND " << AreaMatch(record)
        << " LIMIT 1;\n";
}

// An arrival the editor added: found by map and area, created when missing. It is never a
// spawn gate (OpenMU picks spawn gates for new characters and deaths).
void WriteAddedExitGate(std::ostringstream& out, int number, const Gates::GateRecord& record)
{
    const std::string exit = ExitVariable(number);
    WriteFindExitGate(out, number, record);
    out << "    IF " << exit << " IS NULL THEN\n"
        << "        " << exit << " := gen_random_uuid();\n"
        << "        INSERT INTO config.\"ExitGate\" " << EXIT_GATE_COLUMNS << "\n"
        << "        VALUES (" << exit << ", " << MapVariable(record.map) << ", " << AreaValues(record) << ", "
        << static_cast<int>(record.direction) << ", false);\n    END IF;\n";
}

// One of the game's own arrivals that an added enter gate lands on: the server has it
// already, so it is only looked up.
void WriteGameExitGate(std::ostringstream& out, int number, const Gates::GateRecord& record)
{
    WriteFindExitGate(out, number, record);
    out << "    IF " << ExitVariable(number) << " IS NULL THEN\n"
        << "        RAISE EXCEPTION 'the exit gate of Gate.bmd " << number << " (map " << static_cast<int>(record.map)
        << ", " << AreaValues(record) << ") is not in the database';\n    END IF;\n";
}

void WriteExitGate(std::ostringstream& out, const ExportInput& input, int number)
{
    const Gates::GateRecord& record = Record(input, number);
    out << "    -- Gate.bmd " << number << ": arrival on map " << static_cast<int>(record.map) << "\n";
    if (Gates::IsCustomNumber(number))
        WriteAddedExitGate(out, number, record);
    else
        WriteGameExitGate(out, number, record);
}

void WriteEnterGate(std::ostringstream& out, const ExportInput& input, int number)
{
    const Gates::GateRecord& record = Record(input, number);
    const std::string map = MapVariable(record.map);
    if (Gates::TargetMap(*input.gates, number) < 0)
    {
        out << "    -- Gate.bmd " << number << " has no arrival gate to land on; left out.\n";
        return;
    }
    out << "    -- Gate.bmd " << number << ": enter gate on map " << static_cast<int>(record.map) << "\n"
        << "    IF NOT EXISTS (SELECT 1 FROM config.\"EnterGate\" WHERE \"GameMapDefinitionId\" = " << map
        << " AND \"Number\" = " << number << ") THEN\n"
        << "        INSERT INTO config.\"EnterGate\" " << ENTER_GATE_COLUMNS << "\n"
        << "        VALUES (gen_random_uuid(), " << map << ", " << number << ", " << record.level << ", "
        << ExitVariable(record.target) << ", " << AreaValues(record) << ");\n    END IF;\n";
}
} // namespace

std::string SqlScript(const ExportInput& input)
{
    std::ostringstream out;
    WriteHeader(out, input);
    WriteDeclarations(out, input);
    if (input.newMap)
        WriteNewMap(out, input);
    else
        WriteFindMap(out, input.map);
    for (const int map : MapsTouched(input))
    {
        if (map != input.map)
            WriteFindMap(out, map);
    }
    if (input.gates != nullptr)
    {
        out << "\n    -- Step 3: the gates the editor added.\n";
        for (const int number : ExitGates(input))
            WriteExitGate(out, input, number);
        for (const int number : EnterGates(input))
            WriteEnterGate(out, input, number);
    }
    out << "END\n$mu$;\n\nCOMMIT;\n";
    return out.str();
}
} // namespace Editor::ServerExport

#endif // _EDITOR
