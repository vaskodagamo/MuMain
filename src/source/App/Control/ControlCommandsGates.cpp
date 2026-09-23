#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#ifdef _EDITOR

#include "App/Control/ControlMapArguments.h"
#include "Core/EditorCamera.h"
#include "Core/LiveGates.h"
#include "Core/LiveMap.h"
#include "Gates/GateDirection.h"
#include "Gates/GateQueries.h"
#include "MapInspect/TileArea.h"
#include "UI/MapEditor/MapEditorUI.h"

#include "World/MapInfra/MapNumbers.h"

#include "json.hpp"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace
{
using App::Control::Act;
using App::Control::EncodeError;
using App::Control::EncodeResult;
using App::Control::ErrorCode;
using App::Control::Request;
using nlohmann::json;
namespace Arguments = App::Control::MapArguments;
namespace Gates = Editor::Gates;
namespace Numbers = World::MapNumbers;

constexpr std::size_t TILE_VALUES = 2;

json MapJson(int map)
{
    return {{"map", map}, {"world", Numbers::FolderOf(map)}, {"name", Editor::LiveGates::MapName(map)}};
}

json AreaJson(const Gates::GateRecord& record)
{
    return json::array({record.x1, record.y1, record.x2, record.y2});
}

// An arrival's direction as a sentence: OpenMU turns arriving players that way.
std::string FacingText(int direction)
{
    return std::string("players arriving here face ") + Gates::DirectionName(direction);
}

json TargetJson(const Gates::GateTable& table, const Gates::GateRecord& enter)
{
    if (!Gates::IsValidNumber(enter.target) || Gates::IsFree(table[enter.target]))
        return nullptr;
    const Gates::GateRecord& target = table[enter.target];
    json result = MapJson(target.map);
    result["number"] = enter.target;
    result["kind"] = Gates::KindName(target);
    result["area"] = AreaJson(target);
    result["direction"] = target.direction;
    result["direction_name"] = Gates::DirectionName(target.direction);
    result["faces"] = FacingText(target.direction);
    return result;
}

json GateJson(const Gates::GateTable& table, int number)
{
    const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
    json gate = {{"number", number}, {"kind", Gates::KindName(record)}, {"custom", Gates::IsCustomNumber(number)}};
    gate.update(MapJson(record.map));
    gate["area"] = AreaJson(record);
    gate["level"] = record.level;
    if (record.flag == Gates::FLAG_ENTER)
    {
        gate["target"] = TargetJson(table, record);
        return gate;
    }
    gate["direction"] = record.direction;
    gate["direction_name"] = Gates::DirectionName(record.direction);
    gate["faces"] = FacingText(record.direction);
    gate["arrivals_from"] = Gates::EnterGatesTargeting(table, number);
    return gate;
}

json GatesJson(const Gates::GateTable& table, const std::vector<int>& numbers)
{
    json list = json::array();
    for (const int number : numbers)
        list.push_back(GateJson(table, number));
    return list;
}

bool TileFromJson(const json& value, Gates::TileRect& area, std::string& error)
{
    const bool valid =
        value.is_array() && value.size() == TILE_VALUES && value[0].is_number_integer() && value[1].is_number_integer();
    const int x = valid ? value[0].get<int>() : -1;
    const int y = valid ? value[1].get<int>() : -1;
    if (!Editor::MapInspect::IsOnMap(x, y))
    {
        error = "a tile is [x, y], 0 to 255";
        return false;
    }
    area = {x, y, x, y};
    return true;
}

// {"rect": [x0, y0, x1, y1]} or {"tile": [x, y]}.
bool AreaOfEnd(const json& end, Gates::TileRect& area, std::string& error)
{
    if (end.contains("rect") == end.contains("tile"))
    {
        error = "give `rect` [x0, y0, x1, y1] or `tile` [x, y], one of them";
        return false;
    }
    if (end.contains("tile"))
        return TileFromJson(end.at("tile"), area, error);
    Editor::Editing::CellRect cells;
    if (!Arguments::AreaFromJson(end.at("rect"), cells, error))
        return false;
    area = {cells.minX, cells.minY, cells.maxX, cells.maxY};
    return true;
}

bool ReadDirection(const json& end, int& direction, std::string& error)
{
    direction = 0;
    if (!end.contains("dir"))
        return true;
    const json& value = end.at("dir");
    const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
    if ((value.is_string() || value.is_number_integer()) && Gates::DirectionFromName(text, direction))
        return true;
    error = "`to.dir` is one of " + Gates::DirectionNames() + " or its number 0 to 8";
    return false;
}

bool ReadEnd(const Request& request, const char* key, Gates::GateEnd& end, json& value, std::string& error)
{
    std::string encoded;
    value = request.GetStructured(key, encoded) ? json::parse(encoded, nullptr, false) : json();
    const bool isTo = std::string(key) == "to";
    std::string reason;
    const bool valid =
        value.is_object() &&
        Arguments::OnlyKnownFields(value,
                                   isTo ? std::initializer_list<const char*>{"map", "world", "rect", "tile", "dir"}
                                        : std::initializer_list<const char*>{"map", "world", "rect", "tile"},
                                   key, reason) &&
        Arguments::MapFromJson(value, end.map, reason) && AreaOfEnd(value, end.area, reason);
    if (valid)
        return true;
    error = "`" + std::string(key) + "` is {\"map\" or \"world\": N, \"rect\": [x0, y0, x1, y1] or \"tile\": [x, y]" +
            (isTo ? ", \"dir\": a direction}" : "}") + (reason.empty() ? "" : ": " + reason);
    return false;
}

bool ReadPair(const Request& request, Gates::NewGatePair& pair, std::string& error)
{
    json to;
    json from;
    if (!ReadEnd(request, "from", pair.from, from, error) || !ReadEnd(request, "to", pair.to, to, error) ||
        !ReadDirection(to, pair.direction, error))
        return false;
    if (request.Has("allow_trap") && !request.GetBool("allow_trap", pair.allowTrap))
    {
        error = "`allow_trap` is true or false";
        return false;
    }
    pair.level = 0;
    if (!request.Has("level"))
        return true;
    if (request.GetInt("level", pair.level) && pair.level >= 0 && pair.level <= Gates::MAX_LEVEL_REQUIREMENT)
        return true;
    error = "`level` is the level needed to use the gate, 0 to " + std::to_string(Gates::MAX_LEVEL_REQUIREMENT);
    return false;
}

json EditJson(const Editor::LiveGates::EditResult& result, bool dryRun)
{
    json answer;
    answer["dry_run"] = dryRun;
    answer["warnings"] = result.warnings;
    answer["saved"] = result.saved ? Arguments::SavedFileJson(result.file) : json(nullptr);
    if (!result.report.empty())
        answer["report"] = result.report;
    return answer;
}
} // namespace

namespace App::Control::Commands
{
std::string GateList(const Request& request, std::unique_ptr<Act>&)
{
    int map = Editor::LiveMap::Identity().map;
    bool given = false;
    std::string error;
    if (!Arguments::ReadMapReference(request, map, given, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    std::vector<std::string> notes;
    if (!Editor::LiveGates::Refresh(notes, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);

    const Gates::GateTable table = Editor::LiveGates::Table();
    const std::vector<int> free = Gates::FreeNumbers(table);
    json result = MapJson(map);
    result["gates"] = GatesJson(table, Gates::GatesOnMap(table, map));
    result["ways_in"] = GatesJson(table, Gates::EnterGatesInto(table, map));
    result["free_numbers"] = free.size();
    result["next_free"] = free.empty() ? json(nullptr) : json(free.front());
    result["first_custom"] = Gates::FIRST_CUSTOM_GATE;
    if (!notes.empty())
        result["notes"] = notes;
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string GateAdd(const Request& request, std::unique_ptr<Act>&)
{
    Gates::NewGatePair pair;
    bool dryRun = false;
    std::string error;
    if (!Arguments::ReadDryRun(request, dryRun, error) || !ReadPair(request, pair, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    Editor::LiveGates::EditResult result;
    if (!Editor::LiveGates::AddPair(pair, dryRun, result, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);
    json answer = EditJson(result, dryRun);
    answer["enter"] = result.numbers.at(0);
    answer["arrival"] = result.numbers.at(1);
    if (!dryRun)
        answer["gates"] = GatesJson(Editor::LiveGates::Table(), result.numbers);
    return EncodeResult(request.EncodedId(), answer.dump());
}

std::string GateRemove(const Request& request, std::unique_ptr<Act>&)
{
    int number = -1;
    bool dryRun = false;
    std::string error;
    if (!Arguments::ReadDryRun(request, dryRun, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    if (!request.GetInt("number", number))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`number` is the gate's number (gate-list)");

    Editor::LiveGates::EditResult result;
    if (!Editor::LiveGates::Remove(number, dryRun, result, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);
    json answer = EditJson(result, dryRun);
    answer["removed"] = result.numbers;
    return EncodeResult(request.EncodedId(), answer.dump());
}

std::string GateShow(const Request& request, std::unique_ptr<Act>&)
{
    int number = -1;
    bool look = true;
    if (!request.GetInt("number", number) || !Gates::IsValidNumber(number))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`number` is the gate's number (gate-list)");
    if (request.Has("look") && !request.GetBool("look", look))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`look` is true or false");

    const Gates::GateTable table = Editor::LiveGates::Table();
    const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
    if (Gates::IsFree(record))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, "gate " + std::to_string(number) + " is free");
    const int loaded = Editor::LiveMap::Identity().map;
    if (record.map != loaded)
        return EncodeError(request.EncodedId(), ErrorCode::Failed,
                           "gate " + std::to_string(number) + " lies on map " + std::to_string(record.map) +
                               "; open that map first (map-open with world " +
                               std::to_string(Numbers::FolderOf(record.map)) + ")");

    g_MapEditorUI.ShowGate(number);
    json answer = {{"shown", true}, {"tab", "Gates"}, {"gate", GateJson(table, number)}};
    Editor::Camera::Pose pose;
    const Gates::TileRect area = Gates::AreaOf(record);
    answer["looked"] = look && Editor::Camera::FrameGateArea(area.x1, area.y1, area.x2, area.y2, pose);
    return EncodeResult(request.EncodedId(), answer.dump());
}
} // namespace App::Control::Commands

#endif // _EDITOR
