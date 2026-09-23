// Arguments of the Map Editor's control socket commands (map-*, and screenshot's
// clean and region options): tiles and rectangles as [x, y] and [x0, y0, x1, y1],
// layer lists, and where output files go. Editor builds only.
#pragma once

#ifdef _EDITOR

#include "App/Control/ControlProtocol.h"
#include "Editing/TerrainLayers.h"          // CellRect
#include "MapInspect/LayerImage.h"          // MapLayer
#include "UI/MapEditor/MapEditorFileUtil.h" // SavedFile

#include "json.hpp"

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace App::Control::MapArguments
{
using Editor::Editing::CellRect;

// `key` as [x0, y0, x1, y1] (corners in any order, both included). False, with the
// reason in `error`, when it is missing, not four whole numbers, or off the map.
bool ReadArea(const Request& request, std::string_view key, CellRect& area, std::string& error);
// The same for a decoded JSON value.
bool AreaFromJson(const nlohmann::json& value, CellRect& area, std::string& error);

// `key` as [x, y], a tile of the map.
bool ReadTile(const Request& request, std::string_view key, int& x, int& y, std::string& error);

// An optional number: `value` keeps its default when `key` is absent. False when it
// is present but not a number from `min` to `max`.
bool ReadOptionalNumber(const Request& request, std::string_view key, float min, float max, float& value,
                        std::string& error);

// `layers` as a list of layer names; every layer when absent.
bool ReadLayers(const Request& request, std::vector<Editor::MapInspect::MapLayer>& layers, std::string& error);

// A caller's path, absolute (relative ones are relative to the client's working
// folder, which a caller does not know).
std::filesystem::path ResolvePath(const std::string& utf8Path);

// <repo>/out/<folder>/<name>, or <working folder>/<folder>/<name> without a repository.
std::filesystem::path DefaultOutput(std::string_view folder, const std::string& name);

// A name for a default output that no earlier one of this run has: the local time
// and a running number, e.g. "20260923-113305-1".
std::string UniqueStamp();

// A map named by {"map": N} (the game's number, which OpenMU and Gate.bmd use) or
// {"world": N} (its Data folder, N = map + 1), one of the two; other fields of the object
// are left to the caller. `map` gets the game's number.
bool MapFromJson(const nlohmann::json& value, int& map, std::string& error);
// The same from the request's own `map` or `world` field. `given` is false, and true is
// returned, when the request has neither.
bool ReadMapReference(const Request& request, int& map, bool& given, std::string& error);

// False, naming the first one, when the object `value` has a field that is not in `known`
// (a typo such as "rct" is an error, not ignored); `what` names the object ("to").
bool OnlyKnownFields(const nlohmann::json& value, std::initializer_list<const char*> known, const char* what,
                     std::string& error);

// `dry_run`: false when absent.
bool ReadDryRun(const Request& request, bool& dryRun, std::string& error);

// "created", "replaced", "unchanged", "in_place" or "failed".
const char* RepoResultName(Editor::Files::RepoCopyResult result);
// Where a saved file went: file, repo_result, repo, backup (and local_copy without a repository).
nlohmann::json SavedFileJson(const Editor::Files::SavedFile& file);

nlohmann::json AreaJson(const CellRect& area);
// map-info's "unsaved": one flag per file of the loaded map that holds edits not saved yet.
nlohmann::json UnsavedJson();
std::string PathJson(const std::filesystem::path& path);
} // namespace App::Control::MapArguments

#endif // _EDITOR
