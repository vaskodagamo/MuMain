#include "stdafx.h"

#ifdef _EDITOR

#include "LiveGates.h"

#include "Assets/EditorText.h" // Join
#include "Gates/GateFile.h"
#include "MapScript/AttributeRules.h"
#include "NewMap/WorldFolders.h" // FindIgnoringCase
#include "UI/Console/MuEditorConsoleUI.h"

#include "Core/Globals/_crypt.h" // BuxConvert
#include "Core/Text/Utf8.h"
#include "Engine/Object/ZzzInfomation.h"  // GateAttribute
#include "Render/Terrain/ZzzLodTerrain.h" // TerrainWall, MapFileDecrypt
#include "World/MapInfra/CustomMapName.h"
#include "World/MapInfra/MapManager.h"
#include "World/MapInfra/MapNumbers.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace Editor::LiveGates
{
namespace
{
namespace Gates = Editor::Gates;
namespace Numbers = World::MapNumbers;

static_assert(sizeof(GATE_ATTRIBUTE) == Gates::RECORD_BYTES, "Gate.bmd's records are GATE_ATTRIBUTE's bytes");
static_assert(MAX_GATES == Gates::GATE_COUNT, "Gate.bmd holds MAX_GATES records");
static_assert(sizeof(TerrainWall[0]) == sizeof(std::uint16_t), "TerrainWall holds 16-bit attributes");

constexpr const char* GATE_FILE = "Gate.bmd";
constexpr int MAP_CELLS = TERRAIN_SIZE * TERRAIN_SIZE;

void Bux(std::uint8_t* record, std::size_t size)
{
    BuxConvert(record, static_cast<int>(size));
}

// Data/Gate.bmd as it is spelled on disk: the repository keeps it as gate.bmd, and on a
// case-sensitive file system only that spelling is found (the engine's loader resolves
// the name the same way), so saves and the repository copy go to the file that is there.
std::filesystem::path GateFile()
{
    const std::filesystem::path data = Editor::Files::DataDir();
    return Editor::NewMap::FindIgnoringCase(data, GATE_FILE).value_or(data / GATE_FILE);
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%ls\r\n", Core::Text::FromUtf8(message).c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}

void Apply(const Gates::GateTable& table)
{
    if (GateAttribute == nullptr)
        return;
    for (int number = 0; number < Gates::GATE_COUNT; ++number)
    {
        std::uint8_t plain[Gates::RECORD_BYTES];
        Gates::EncodeRecord(table[static_cast<std::size_t>(number)], plain);
        std::memcpy(&GateAttribute[number], plain, sizeof(plain));
    }
}

bool WriteGateFile(const std::vector<std::uint8_t>& bytes, std::string& error)
{
    const std::filesystem::path file = GateFile();
    FILE* fp = _wfopen(file.wstring().c_str(), L"wb");
    if (fp == nullptr)
    {
        error = "could not open " + Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(file)) + " for writing";
        return false;
    }
    const bool written = fwrite(bytes.data(), 1, bytes.size(), fp) == bytes.size();
    const bool closed = fclose(fp) == 0;
    if (written && closed)
        return true;
    error = "could not write " + Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(file)) + " (disk full?)";
    return false;
}

// Writes `table` to Data/Gate.bmd (and the repository), then makes it the one the client
// uses. Nothing changes when the file cannot be written.
bool Commit(const Gates::GateTable& table, EditResult& result, std::string& error)
{
    if (!WriteGateFile(Gates::EncodeGateFile(table, Bux), error))
        return false;
    Apply(table);
    result.file = Editor::Files::MirrorSavedFile(GateFile());
    result.report = Editor::Files::DescribeSavedFiles({result.file});
    result.saved = true;
    return true;
}

std::vector<std::uint16_t> LoadedWalkMap()
{
    std::vector<std::uint16_t> tiles(MAP_CELLS);
    std::memcpy(tiles.data(), TerrainWall, tiles.size() * sizeof(std::uint16_t));
    return tiles;
}

bool ReadClientWalkMap(int map, std::vector<std::uint16_t>& tiles, std::string& error)
{
    const std::filesystem::path file = Editor::Files::TerrainAttributeFile(Numbers::FolderOf(map));
    std::vector<std::uint8_t> stored = Editor::Files::ReadWholeFile(file);
    if (stored.empty())
    {
        error = Editor::Files::PathToUtf8(file) + " is missing";
        return false;
    }
    std::vector<std::uint8_t> plain(stored.size());
    MapFileDecrypt(plain.data(), stored.data(), static_cast<int>(stored.size()));
    BuxConvert(plain.data(), static_cast<int>(plain.size()));
    return Editor::MapScript::Attributes::DecodeClientFile(plain.data(), plain.size(), map, tiles, error);
}

bool CheckMapFolder(int map, const char* side, std::string& error)
{
    if (MapHasFolder(map))
        return true;
    error = std::string(side) + " map " + std::to_string(map) + " has no folder Data/World" +
            std::to_string(Numbers::FolderOf(map)) + "; create the map first (map-new)";
    return false;
}

std::string NumberList(const std::vector<int>& numbers)
{
    std::string text;
    for (const int number : numbers)
        text += (text.empty() ? "" : ", ") + std::to_string(number);
    return text;
}
} // namespace

Gates::GateTable Table()
{
    Gates::GateTable table{};
    if (GateAttribute == nullptr)
        return table;
    for (int number = 0; number < Gates::GATE_COUNT; ++number)
    {
        std::uint8_t plain[Gates::RECORD_BYTES];
        std::memcpy(plain, &GateAttribute[number], sizeof(plain));
        table[static_cast<std::size_t>(number)] = Gates::DecodeRecord(plain);
    }
    return table;
}

bool Refresh(std::vector<std::string>& notes, std::string& error)
{
    const std::filesystem::path file = GateFile();
    const std::vector<std::uint8_t> bytes = Editor::Files::ReadWholeFile(file);
    if (bytes.empty())
        return true;
    Gates::GateTable onDisk{};
    std::string reason;
    if (!Gates::DecodeGateFile(bytes, Bux, onDisk, reason))
    {
        error = Editor::Files::PathToUtf8(file) + " is not a gate table (" + reason + "); nothing was changed";
        return false;
    }
    const Gates::GateTable loaded = Table();
    int differing = 0;
    for (std::size_t number = 0; number < onDisk.size(); ++number)
        differing += onDisk[number] == loaded[number] ? 0 : 1;
    if (differing == 0)
        return true;
    Apply(onDisk);
    const std::string note = Editor::Files::PathToUtf8(file) + " had changed since this client read it (" +
                             std::to_string(differing) +
                             " records: another client or a checkout); this client now uses the file's gates";
    Log("[MapEditor] " + note);
    notes.push_back(note);
    return true;
}

Gates::WalkMap WalkMapOf(int map)
{
    Gates::WalkMap walkMap;
    if (map == gMapManager.WorldActive)
    {
        walkMap.tiles = LoadedWalkMap();
        return walkMap;
    }
    return SavedWalkMapOf(map);
}

Gates::WalkMap SavedWalkMapOf(int map)
{
    Gates::WalkMap walkMap;
    if (!MapHasFolder(map))
    {
        walkMap.problem = "there is no Data/World" + std::to_string(Numbers::FolderOf(map));
        return walkMap;
    }
    if (!ReadClientWalkMap(map, walkMap.tiles, walkMap.problem))
        walkMap.tiles.clear();
    return walkMap;
}

std::string MapName(int map)
{
    if (map >= Numbers::FIRST_NEW_MAP && World::MapNames::Find(map) == nullptr)
        return "Map " + std::to_string(map);
    return Core::Text::ToUtf8(gMapManager.GetMapName(map));
}

bool MapHasFolder(int map)
{
    std::error_code ec;
    return map >= 0 && map <= Numbers::LAST_NEW_MAP &&
           std::filesystem::is_directory(Editor::Files::WorldDir(Numbers::FolderOf(map)), ec);
}

bool AddPair(const Gates::NewGatePair& pair, bool dryRun, EditResult& result, std::string& error)
{
    if (!CheckMapFolder(pair.from.map, "the enter gate's", error) ||
        !CheckMapFolder(pair.to.map, "the arrival gate's", error) || !Refresh(result.warnings, error))
        return false;

    const Gates::GateTable before = Table();
    Gates::GateTable table = before;
    Gates::AddedGates added;
    if (!Gates::AddGatePair(table, pair, added, error))
        return false;
    result.numbers = {added.enter, added.arrival};

    // The areas as stored (corners in order); the checks look at the table without the pair.
    Gates::NewGatePair stored = pair;
    stored.from.area = Gates::AreaOf(table[static_cast<std::size_t>(added.enter)]);
    stored.to.area = Gates::AreaOf(table[static_cast<std::size_t>(added.arrival)]);
    const Gates::WalkMap arrivalMap = WalkMapOf(pair.to.map);
    const std::vector<std::string> traps = Gates::PairTraps(stored, arrivalMap);
    if (!traps.empty() && !pair.allowTrap)
    {
        error = Editor::Text::Join(traps, "; ") +
                ". Pick other areas (the control socket's gate-add takes allow_trap to add it anyway)";
        return false;
    }
    const std::vector<std::string> warnings = Gates::PairWarnings(before, stored, WalkMapOf(pair.from.map), arrivalMap);
    result.warnings.insert(result.warnings.end(), warnings.begin(), warnings.end());
    if (dryRun)
        return true;
    if (!Commit(table, result, error))
        return false;
    Log("[MapEditor] Gate " + std::to_string(added.enter) + " on map " + std::to_string(pair.from.map) +
        " now leads to gate " + std::to_string(added.arrival) + " on map " + std::to_string(pair.to.map));
    return true;
}

bool Remove(int number, bool dryRun, EditResult& result, std::string& error)
{
    if (!Refresh(result.warnings, error))
        return false;
    Gates::GateTable table = Table();
    if (!Gates::RemoveGate(table, number, result.numbers, error))
        return false;
    if (dryRun)
        return true;
    if (!Commit(table, result, error))
        return false;
    Log("[MapEditor] Removed gate(s) " + NumberList(result.numbers));
    return true;
}

bool Change(int number, const Gates::GateChange& change, EditResult& result, std::string& error)
{
    if (!Refresh(result.warnings, error))
        return false;
    Gates::GateTable table = Table();
    if (!Gates::ChangeGate(table, number, change, error))
        return false;
    result.numbers = {number};
    const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
    const char* what = record.flag == Gates::FLAG_ENTER ? "the enter area" : "the arrival area";
    const std::vector<std::string> warnings =
        Gates::AreaWarnings(WalkMapOf(record.map), record.map, Gates::AreaOf(record), what);
    result.warnings.insert(result.warnings.end(), warnings.begin(), warnings.end());
    return Commit(table, result, error);
}
} // namespace Editor::LiveGates

#endif // _EDITOR
