#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#ifdef _EDITOR

#include "App/Control/ControlMapArguments.h"
#include "Assets/EditorText.h"
#include "Core/LiveMap.h"
#include "Core/NewMapFiles.h"
#include "Core/ServerExportFiles.h"
#include "MapInspect/TilePalette.h"
#include "MapScript/AttributeRules.h"

#include "World/MapInfra/MapNumbers.h"

#include "json.hpp"

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <string>

namespace
{
using App::Control::Act;
using App::Control::EncodeError;
using App::Control::EncodeResult;
using App::Control::ErrorCode;
using App::Control::Request;
using nlohmann::json;
namespace Arguments = App::Control::MapArguments;
namespace NewMap = Editor::NewMap;
namespace Numbers = World::MapNumbers;

// TerrainHeight.OZB keeps one byte a corner as height / 1.5.
constexpr double HEIGHT_STEP = 1.5;
constexpr double MAX_HEIGHT = 255 * HEIGHT_STEP;
constexpr int DEFAULT_TEXTURES_WORLD = 1; // Lorencia's tile textures
constexpr std::size_t RGB_VALUES = 3;

bool ReadHeight(const json& blank, std::uint8_t& heightByte, std::string& error)
{
    const json value = blank.value("height", json(0));
    if (!value.is_number() || value.get<double>() < 0 || value.get<double>() > MAX_HEIGHT)
    {
        error = "`from.blank.height` is the ground's height, 0 to 382.5 (steps of 1.5)";
        return false;
    }
    heightByte = static_cast<std::uint8_t>(std::lround(value.get<double>() / HEIGHT_STEP));
    return true;
}

bool ReadTexture(const json& blank, std::uint8_t& slot, std::string& error)
{
    const json value = blank.value("texture", json(Editor::MapInspect::TileSlotName(0)));
    for (int candidate = 0; candidate < Editor::MapInspect::TILE_SLOT_COUNT; ++candidate)
    {
        const bool byNumber = value.is_number_integer() && value.get<int>() == candidate;
        const bool byName =
            value.is_string() &&
            Editor::Text::EqualIgnoringCase(value.get<std::string>(), Editor::MapInspect::TileSlotName(candidate));
        if (byNumber || byName)
        {
            slot = static_cast<std::uint8_t>(candidate);
            return true;
        }
    }
    error = "`from.blank.texture` is a texture slot 0 to 29 or its name (TileGrass01, TileGround01, TileWater01, "
            "TileRock01, ExtTile01, ...)";
    return false;
}

bool ReadAttribute(const json& blank, std::uint8_t& attribute, std::string& error)
{
    const json value = blank.value("attribute", json("walkable"));
    std::uint16_t named = 0;
    if (value.is_string() && Editor::MapScript::Attributes::FromName(value.get<std::string>(), named))
    {
        attribute = static_cast<std::uint8_t>(named);
        return true;
    }
    if (value.is_number_integer() && Editor::MapScript::Attributes::IsCleanValue(value.get<double>()))
    {
        attribute = static_cast<std::uint8_t>(value.get<int>());
        return true;
    }
    error = std::string("`from.blank.attribute` is one of ") + Editor::MapScript::Attributes::KnownNames();
    return false;
}

bool ReadLight(const json& blank, std::array<float, 3>& light, std::string& error)
{
    const json value = blank.value("light", json(1.0));
    const json channels = value.is_number() ? json::array({value, value, value}) : value;
    bool valid = channels.is_array() && channels.size() == RGB_VALUES;
    for (std::size_t i = 0; valid && i < RGB_VALUES; ++i)
    {
        valid = channels[i].is_number() && channels[i].get<double>() >= 0.0 && channels[i].get<double>() <= 1.0;
        if (valid)
            light[i] = channels[i].get<float>();
    }
    if (!valid)
        error = "`from.blank.light` is a brightness 0 to 1 or [r, g, b]";
    return valid;
}

bool ReadBlank(const json& blank, NewMap::NewMapRequest& request, std::string& error)
{
    request.source = NewMap::MapSource::Blank;
    request.texturesWorld = DEFAULT_TEXTURES_WORLD;
    if (!blank.is_object())
    {
        error = "`from.blank` is {\"height\", \"texture\", \"attribute\", \"light\", \"textures_from\"}";
        return false;
    }
    if (!Arguments::OnlyKnownFields(blank, {"height", "texture", "attribute", "light", "textures_from"}, "from.blank",
                                    error))
        return false;
    int texturesMap = Numbers::MapOfFolder(DEFAULT_TEXTURES_WORLD);
    if (blank.contains("textures_from") && !Arguments::MapFromJson(blank.at("textures_from"), texturesMap, error))
    {
        error = "`from.blank.textures_from`: " + error;
        return false;
    }
    request.texturesWorld = Numbers::FolderOf(texturesMap);
    NewMap::BlankGround& ground = request.blank;
    return ReadHeight(blank, ground.heightByte, error) && ReadTexture(blank, ground.tileSlot, error) &&
           ReadAttribute(blank, ground.attribute, error) && ReadLight(blank, ground.light, error);
}

bool ReadSource(const Request& request, NewMap::NewMapRequest& newMap, std::string& error)
{
    std::string encoded;
    const json from = request.GetStructured("from", encoded) ? json::parse(encoded, nullptr, false) : json();
    if (!from.is_object() || from.contains("template") == from.contains("blank") || from.size() != 1)
    {
        error = "`from` is {\"template\": {\"map\" or \"world\": N}} or {\"blank\": {\"height\", \"texture\", "
                "\"attribute\", \"light\", \"textures_from\"}}";
        return false;
    }
    if (from.contains("blank"))
        return ReadBlank(from.at("blank"), newMap, error);

    int templateMap = 0;
    if (!Arguments::MapFromJson(from.at("template"), templateMap, error))
    {
        error = "`from.template`: " + error;
        return false;
    }
    newMap.source = NewMap::MapSource::Template;
    newMap.templateWorld = Numbers::FolderOf(templateMap);
    newMap.modelsWorld = newMap.templateWorld;
    return true;
}

bool ReadModels(const Request& request, NewMap::NewMapRequest& newMap, std::string& error)
{
    if (!request.Has("models_from"))
        return true;
    std::string encoded;
    const json models = request.GetStructured("models_from", encoded) ? json::parse(encoded, nullptr, false) : json();
    int map = 0;
    if (!Arguments::MapFromJson(models, map, error))
    {
        error = "`models_from`: " + error;
        return false;
    }
    newMap.modelsWorld = Numbers::FolderOf(map);
    return true;
}

bool ReadNewMapRequest(const Request& request, NewMap::NewMapRequest& newMap, std::string& error)
{
    bool given = false;
    if (!Arguments::ReadMapReference(request, newMap.map, given, error))
        return false;
    if (!given)
    {
        error = "`map` is the new map's number (" + std::to_string(Numbers::FIRST_NEW_MAP) + " to " +
                std::to_string(Numbers::LAST_NEW_MAP) + "), or `world` its folder number";
        return false;
    }
    if (!NewMap::CheckNewMapNumber(newMap.map, error))
        return false;
    if (!request.GetString("name", newMap.name))
    {
        error = "`name` is the map's name as the game shows it";
        return false;
    }
    if (request.Has("minimap") && !request.GetBool("minimap", newMap.copyMinimap))
    {
        error = "`minimap` is true or false";
        return false;
    }
    return ReadSource(request, newMap, error) && ReadModels(request, newMap, error);
}

json NewMapJson(const Editor::NewMapFiles::CreateResult& created, bool dryRun)
{
    const NewMap::NewMapPlan& plan = created.plan;
    json answer = {{"map", plan.map}, {"world", plan.world}, {"dry_run", dryRun}};
    json folders = json::array();
    for (const auto& folder : plan.folders)
        folders.push_back(Editor::Text::GenericPathToUtf8(folder));
    answer["folders"] = folders;
    json files = json::array();
    for (const NewMap::PlannedFile& file : plan.files)
    {
        if (file.relative.parent_path() == plan.folders.front())
            files.push_back(Editor::Text::GenericPathToUtf8(file.relative));
    }
    answer["world_files"] = files;
    answer["file_count"] = plan.files.size();
    answer["warnings"] = plan.warnings;
    answer["report"] = created.report;
    if (!dryRun)
    {
        std::map<std::string, int> repoResults;
        for (const NewMap::WrittenFile& file : created.written)
            ++repoResults[Arguments::RepoResultName(file.repo.result)];
        answer["repo_results"] = repoResults;
        answer["open_with"] = {{"cmd", "map-open"}, {"world", plan.world}};
    }
    return answer;
}

bool ReadSafezone(const Request& request, int& safezone, std::string& error)
{
    safezone = Editor::ServerExportFiles::AUTO_SAFEZONE;
    if (!request.Has("safezone_map"))
        return true;
    if (request.GetInt("safezone_map", safezone) && safezone >= 0 && safezone <= Numbers::LAST_MAP)
        return true;
    error = "`safezone_map` is the game's number of the map players return to when they die there";
    return false;
}
} // namespace

namespace App::Control::Commands
{
std::string MapNew(const Request& request, std::unique_ptr<Act>&)
{
    NewMap::NewMapRequest newMap;
    bool dryRun = false;
    std::string error;
    if (!Arguments::ReadDryRun(request, dryRun, error) || !ReadNewMapRequest(request, newMap, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    Editor::NewMapFiles::CreateResult created;
    if (!Editor::NewMapFiles::Create(newMap, dryRun, created, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);
    return EncodeResult(request.EncodedId(), NewMapJson(created, dryRun).dump());
}

std::string MapServerExport(const Request& request, std::unique_ptr<Act>&)
{
    int map = Editor::LiveMap::Identity().map;
    bool given = false;
    int safezone = Editor::ServerExportFiles::AUTO_SAFEZONE;
    std::string error;
    std::string out;
    if (!Arguments::ReadMapReference(request, map, given, error) || !ReadSafezone(request, safezone, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    if (request.Has("out") && (!request.GetString("out", out) || out.empty()))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`out` is the folder to write the export to");

    Editor::ServerExportFiles::ExportResult result;
    const std::filesystem::path folder = out.empty() ? std::filesystem::path() : Arguments::ResolvePath(out);
    if (!Editor::ServerExportFiles::Export(map, safezone, folder, result, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);

    json answer = {{"map", map}, {"world", Numbers::FolderOf(map)}, {"applied", false}};
    answer["folder"] = Arguments::PathJson(result.folder);
    json files = json::array();
    for (const auto& file : result.files)
        files.push_back(Arguments::PathJson(file));
    answer["files"] = files;
    answer["new_map"] = result.newMap;
    answer["safezone_map"] = result.safezoneMap;
    answer["warnings"] = result.warnings;
    return EncodeResult(request.EncodedId(), answer.dump());
}
} // namespace App::Control::Commands

#endif // _EDITOR
