#include "stdafx.h"

#ifdef _EDITOR

#include "ServerExportFiles.h"

#include "LiveGates.h"
#include "LiveMap.h"
#include "Gates/GateQueries.h"
#include "ServerExport/ExportHowTo.h"
#include "ServerExport/ExportSql.h"
#include "ServerExport/OpenMuDocuments.h"
#include "ServerExport/ServerTerrain.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapEditorFileUtil.h"

#include "Core/Text/Utf8.h"
#include "World/MapInfra/MapNumbers.h"

#include <fstream>
#include <system_error>

namespace Editor::ServerExportFiles
{
namespace
{
namespace fs = std::filesystem;
namespace Export = Editor::ServerExport;
namespace Numbers = World::MapNumbers;

constexpr const char* OUT_FOLDER = "out";
constexpr const char* EXPORT_FOLDER = "openmu-export";
constexpr const char* MAP_FOLDER_PREFIX = "map";
constexpr const char* MAP_FILE = "map.json";
constexpr const char* GATES_FILE = "gates.json";
constexpr const char* HOWTO_FILE = "HOWTO.md";
constexpr const char* SQL_FILE = "openmu.sql";
constexpr const char* TERRAIN_FILE_PREFIX = "terrain_map";
constexpr const char* TERRAIN_FILE_SUFFIX = "_server.att";
constexpr int JSON_INDENT = 2;
constexpr int LORENCIA = 0;

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%ls\r\n", Core::Text::FromUtf8(message).c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}

bool WriteFile(const fs::path& file, const std::string& text, ExportResult& result, std::string& error)
{
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        error = "cannot write " + Editor::Files::PathToUtf8(file);
        return false;
    }
    result.files.push_back(file);
    return true;
}

int AutomaticSafezone(const Editor::Gates::GateTable& table, const Export::ExportGates& gates)
{
    for (const int enter : gates.enterOnMap)
    {
        const int target = Editor::Gates::TargetMap(table, enter);
        if (target >= 0)
            return target;
    }
    return LORENCIA;
}

// A new map's walk map, from its saved client file.
bool AddTerrain(Export::ExportInput& input, ExportResult& result, std::string& error)
{
    const Editor::Gates::WalkMap walkMap = Editor::LiveGates::SavedWalkMapOf(input.map);
    if (walkMap.tiles.empty())
    {
        error = "the walk map of map " + std::to_string(input.map) + " cannot be read: " + walkMap.problem;
        return false;
    }
    const Export::ServerTerrain terrain = Export::BuildTerrainData(walkMap.tiles);
    input.terrainData = terrain.data;
    input.terrainDiffering = terrain.differing;
    input.terrainFile = TERRAIN_FILE_PREFIX + std::to_string(input.map) + TERRAIN_FILE_SUFFIX;
    const bool loaded = Editor::LiveMap::Identity().map == input.map;
    if (loaded && Editor::LiveMap::UnsavedUnits()[static_cast<std::size_t>(Editor::MapInspect::SaveUnit::Attribute)])
        result.warnings.push_back("the walk map has unsaved edits; the export holds the saved one (map-save "
                                  "\"attribute\" first)");
    if (terrain.differing > 0)
        result.warnings.push_back(std::to_string(terrain.differing) +
                                  " tiles are walkable in the client but blocked for OpenMU (water or combined "
                                  "walkability values)");
    return true;
}

Export::ExportInput GatherInput(int map, int safezoneMap, const Editor::Gates::GateTable& table)
{
    Export::ExportInput input;
    input.map = map;
    input.world = Numbers::FolderOf(map);
    input.name = Editor::LiveGates::MapName(map);
    input.newMap = map >= Numbers::FIRST_NEW_MAP;
    input.gates = &table;
    input.gateSet = Export::CollectGates(table, map);
    input.safezoneMap = safezoneMap == AUTO_SAFEZONE ? AutomaticSafezone(table, input.gateSet) : safezoneMap;
    input.mapName = Editor::LiveGates::MapName;
    return input;
}

bool WriteExport(const Export::ExportInput& input, const fs::path& folder, ExportResult& result, std::string& error)
{
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec)
    {
        error = "cannot create " + Editor::Files::PathToUtf8(folder) + ": " + ec.message();
        return false;
    }
    const std::string terrain(input.terrainData.begin(), input.terrainData.end());
    return (input.terrainFile.empty() || WriteFile(folder / input.terrainFile, terrain, result, error)) &&
           WriteFile(folder / MAP_FILE, Export::MapDocument(input).dump(JSON_INDENT) + "\n", result, error) &&
           WriteFile(folder / GATES_FILE, Export::GatesDocument(input).dump(JSON_INDENT) + "\n", result, error) &&
           WriteFile(folder / HOWTO_FILE, Export::HowToText(input), result, error) &&
           WriteFile(folder / SQL_FILE, Export::SqlScript(input), result, error);
}

std::string Report(const ExportResult& result, int map)
{
    std::string report = "Wrote the OpenMU export of map " + std::to_string(map) + " (not applied to any server):\n  " +
                         Editor::Files::PathToUtf8(result.folder) + "\n  follow HOWTO.md there";
    for (const std::string& warning : result.warnings)
        report += "\n  note: " + warning;
    return report;
}
} // namespace

fs::path DefaultFolder(int map)
{
    const fs::path mapFolder = MAP_FOLDER_PREFIX + std::to_string(map);
    const fs::path& repo = Editor::Files::RepoRoot().root;
    if (!repo.empty())
        return repo / OUT_FOLDER / EXPORT_FOLDER / mapFolder;
    return Editor::Files::AbsolutePath(fs::path(EXPORT_FOLDER) / mapFolder);
}

bool Export(int map, int safezoneMap, const fs::path& folder, ExportResult& result, std::string& error)
{
    if (!Editor::LiveGates::MapHasFolder(map))
    {
        error = "map " + std::to_string(map) + " has no folder Data/World" + std::to_string(Numbers::FolderOf(map));
        return false;
    }
    if (safezoneMap != AUTO_SAFEZONE && !Editor::LiveGates::MapHasFolder(safezoneMap))
    {
        error = "the safe-zone map " + std::to_string(safezoneMap) + " has no folder";
        return false;
    }

    result = ExportResult{};
    if (!Editor::LiveGates::Refresh(result.warnings, error))
        return false;
    const Editor::Gates::GateTable table = Editor::LiveGates::Table();
    Export::ExportInput input = GatherInput(map, safezoneMap, table);
    result.folder = folder.empty() ? DefaultFolder(map) : folder;
    result.safezoneMap = input.safezoneMap;
    result.newMap = input.newMap;
    if ((input.newMap && !AddTerrain(input, result, error)) || !WriteExport(input, result.folder, result, error))
        return false;
    result.report = Report(result, map);
    Log("[MapEditor] OpenMU export of map " + std::to_string(map) + " written to " +
        Editor::Files::PathToUtf8(result.folder));
    return true;
}
} // namespace Editor::ServerExportFiles

#endif // _EDITOR
