#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>
#include <vector>

// Writing a map's OpenMU export (ServerExport/) from the running client's data: its
// saved walk map, the loaded gate table and the map's name. Never applied to a server.
namespace Editor::ServerExportFiles
{
constexpr int AUTO_SAFEZONE = -1;

struct ExportResult
{
    std::filesystem::path folder;
    std::vector<std::filesystem::path> files;
    int safezoneMap = 0;
    bool newMap = false;
    std::vector<std::string> warnings;
    std::string report; // the status line
};

// <repo>/out/openmu-export/map{map}, or openmu-export/map{map} next to the game without a
// repository.
std::filesystem::path DefaultFolder(int map);

// Writes the export of `map` into `folder` (created; files there are replaced).
// `safezoneMap` AUTO_SAFEZONE takes the map this map's first enter gate leads to, else
// Lorencia. False with the reason when the map has no folder, its walk map cannot be read
// (new maps) or a file cannot be written.
bool Export(int map, int safezoneMap, const std::filesystem::path& folder, ExportResult& result, std::string& error);
} // namespace Editor::ServerExportFiles

#endif // _EDITOR
