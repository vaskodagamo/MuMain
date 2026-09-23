#include "stdafx.h"
#include "App/Control/ControlMapArguments.h"

#ifdef _EDITOR

#include "Assets/EditorText.h"
#include "Core/LiveMap.h"
#include "Core/Text/Utf8.h"
#include "MapInspect/TileArea.h"
#include "UI/MapEditor/MapEditorFileUtil.h"

#include "World/MapInfra/MapNumbers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <system_error>

namespace App::Control::MapArguments
{
namespace
{
using nlohmann::json;

constexpr std::size_t AREA_VALUES = 4;
constexpr std::size_t TILE_VALUES = 2;
constexpr const char* OUT_FOLDER = "out";
// Far past any map, and small enough that the conversion to int is exact: a tile
// beyond the map is then reported as off the map, not as malformed.
constexpr double MAX_COORDINATE = 1.0e6;

// A JSON array of `count` whole numbers.
bool WholeNumbers(const json& value, std::size_t count, std::vector<int>& numbers)
{
    if (!value.is_array() || value.size() != count)
        return false;
    numbers.clear();
    for (const json& entry : value)
    {
        if (!entry.is_number())
            return false;
        const double number = entry.get<double>();
        double whole = 0.0;
        if (std::modf(number, &whole) != 0.0 || std::fabs(number) > MAX_COORDINATE)
            return false;
        numbers.push_back(static_cast<int>(number));
    }
    return true;
}

std::string FormatNumber(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", static_cast<double>(value));
    return text;
}

// A whole number from `min` to `max` in `value`.
bool WholeNumberIn(const json& value, int min, int max, int& number)
{
    std::vector<int> numbers;
    if (!WholeNumbers(json::array({value}), 1, numbers) || numbers[0] < min || numbers[0] > max)
        return false;
    number = numbers[0];
    return true;
}

std::string MapReferenceHelp()
{
    return "{\"map\": N} (the game's map number, 0 to " + std::to_string(World::MapNumbers::LAST_MAP) +
           ", Lorencia 0) or {\"world\": N} (the Data/World folder, 1 to " +
           std::to_string(World::MapNumbers::LAST_FOLDER) + ", Lorencia 1), one of them";
}

bool ReadStructured(const Request& request, std::string_view key, json& value, std::string& error)
{
    std::string encoded;
    if (!request.GetStructured(key, encoded))
    {
        error = "`" + std::string(key) + "` is missing or not a list";
        return false;
    }
    value = json::parse(encoded, nullptr, false);
    return !value.is_discarded();
}
} // namespace

bool AreaFromJson(const json& value, CellRect& area, std::string& error)
{
    std::vector<int> corners;
    if (!WholeNumbers(value, AREA_VALUES, corners))
    {
        error = "a rectangle is [x0, y0, x1, y1] in tiles";
        return false;
    }
    return Editor::MapInspect::AreaFromCorners(corners[0], corners[1], corners[2], corners[3], area, error);
}

bool ReadArea(const Request& request, std::string_view key, CellRect& area, std::string& error)
{
    json value;
    if (!ReadStructured(request, key, value, error))
        return false;
    if (!AreaFromJson(value, area, error))
    {
        error = "`" + std::string(key) + "`: " + error;
        return false;
    }
    return true;
}

bool ReadTile(const Request& request, std::string_view key, int& x, int& y, std::string& error)
{
    json value;
    std::vector<int> tile;
    if (!ReadStructured(request, key, value, error) || !WholeNumbers(value, TILE_VALUES, tile))
    {
        error = "`" + std::string(key) + "` is [x, y] in tiles";
        return false;
    }
    if (!Editor::MapInspect::IsOnMap(tile[0], tile[1]))
    {
        error = "`" + std::string(key) + "` is off the map; tiles run from 0 to " +
                std::to_string(Editor::MapInspect::MAP_TILES - 1);
        return false;
    }
    x = tile[0];
    y = tile[1];
    return true;
}

bool ReadOptionalNumber(const Request& request, std::string_view key, float min, float max, float& value,
                        std::string& error)
{
    if (!request.Has(key))
        return true;
    double number = 0.0;
    if (!request.GetDouble(key, number) || !(number >= min && number <= max))
    {
        error = "`" + std::string(key) + "` is a number from " + FormatNumber(min) + " to " + FormatNumber(max);
        return false;
    }
    value = static_cast<float>(number);
    return true;
}

bool ReadLayers(const Request& request, std::vector<Editor::MapInspect::MapLayer>& layers, std::string& error)
{
    const auto& all = Editor::MapInspect::AllMapLayers();
    if (!request.Has("layers"))
    {
        layers.assign(all.begin(), all.end());
        return true;
    }
    json value;
    if (!ReadStructured(request, "layers", value, error) || !value.is_array() || value.empty())
    {
        error = "`layers` is a list of layer names";
        return false;
    }
    layers.clear();
    for (const json& entry : value)
    {
        Editor::MapInspect::MapLayer layer{};
        if (!entry.is_string() || !Editor::MapInspect::MapLayerFromName(entry.get<std::string>(), layer))
        {
            error = "unknown layer " + entry.dump() +
                    "; known: height, attribute, texture1, texture2, alpha, light, objects";
            return false;
        }
        layers.push_back(layer);
    }
    return true;
}

std::filesystem::path ResolvePath(const std::string& utf8Path)
{
    // Through the wide form: std::filesystem::path decodes a narrow string with the
    // process code page on Windows, which is not UTF-8 for this client.
    const std::filesystem::path given(Core::Text::FromUtf8(utf8Path));
    std::error_code failure;
    const std::filesystem::path absolute = std::filesystem::absolute(given, failure);
    return failure ? given : absolute;
}

std::filesystem::path DefaultOutput(std::string_view folder, const std::string& name)
{
    const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
    const std::filesystem::path base = repo.empty() ? ResolvePath(".") : repo / OUT_FOLDER;
    return base / Editor::Text::Utf8Path(folder) / Editor::Text::Utf8Path(name);
}

std::string UniqueStamp()
{
    static unsigned int sequence = 0;
    return Editor::Files::Timestamp() + "-" + std::to_string(++sequence);
}

bool MapFromJson(const json& value, int& map, std::string& error)
{
    const bool byMap = value.is_object() && value.contains("map");
    const bool byWorld = value.is_object() && value.contains("world");
    int number = 0;
    bool valid = byMap != byWorld;
    if (valid && byMap)
        valid = WholeNumberIn(value.at("map"), 0, World::MapNumbers::LAST_MAP, number);
    else if (valid)
        valid =
            WholeNumberIn(value.at("world"), World::MapNumbers::FIRST_FOLDER, World::MapNumbers::LAST_FOLDER, number);
    if (!valid)
    {
        error = "a map is " + MapReferenceHelp();
        return false;
    }
    map = byMap ? number : World::MapNumbers::MapOfFolder(number);
    return true;
}

bool ReadMapReference(const Request& request, int& map, bool& given, std::string& error)
{
    given = request.Has("map") || request.Has("world");
    if (!given)
        return true;
    json reference = json::object();
    double number = 0.0;
    for (const char* key : {"map", "world"})
    {
        if (request.Has(key))
            reference[key] = request.GetDouble(key, number) ? json(number) : json(nullptr);
    }
    return MapFromJson(reference, map, error);
}

bool OnlyKnownFields(const json& value, std::initializer_list<const char*> known, const char* what, std::string& error)
{
    for (const auto& entry : value.items())
    {
        const bool isKnown =
            std::any_of(known.begin(), known.end(), [&entry](const char* key) { return entry.key() == key; });
        if (!isKnown)
        {
            error = std::string("`") + what + "." + entry.key() + "` is not a field here";
            return false;
        }
    }
    return true;
}

bool ReadDryRun(const Request& request, bool& dryRun, std::string& error)
{
    dryRun = false;
    if (!request.Has("dry_run") || request.GetBool("dry_run", dryRun))
        return true;
    error = "`dry_run` is true or false";
    return false;
}

const char* RepoResultName(Editor::Files::RepoCopyResult result)
{
    switch (result)
    {
    case Editor::Files::RepoCopyResult::Created:
        return "created";
    case Editor::Files::RepoCopyResult::Replaced:
        return "replaced";
    case Editor::Files::RepoCopyResult::Unchanged:
        return "unchanged";
    case Editor::Files::RepoCopyResult::InPlace:
        return "in_place";
    case Editor::Files::RepoCopyResult::Failed:
        break;
    }
    return "failed";
}

json SavedFileJson(const Editor::Files::SavedFile& file)
{
    json entry;
    entry["file"] = PathJson(file.runtimeFile);
    entry["repo_result"] = RepoResultName(file.repo.result);
    entry["repo"] = file.repo.repoFile.empty() ? json(nullptr) : json(PathJson(file.repo.repoFile));
    entry["backup"] = file.repo.backupFile.empty() ? json(nullptr) : json(PathJson(file.repo.backupFile));
    if (!file.localCopy.empty())
        entry["local_copy"] = PathJson(file.localCopy);
    return entry;
}

json UnsavedJson()
{
    const auto unsaved = Editor::LiveMap::UnsavedUnits();
    json result = json::object();
    for (std::size_t unit = 0; unit < Editor::MapInspect::SAVE_UNIT_COUNT; ++unit)
        result[std::string(Editor::MapInspect::SaveUnitName(static_cast<Editor::MapInspect::SaveUnit>(unit)))] =
            unsaved[unit];
    return result;
}

json AreaJson(const CellRect& area)
{
    return json::array({area.minX, area.minY, area.maxX, area.maxY});
}

std::string PathJson(const std::filesystem::path& path)
{
    return Editor::Text::PathToUtf8(path);
}
} // namespace App::Control::MapArguments

#endif // _EDITOR
