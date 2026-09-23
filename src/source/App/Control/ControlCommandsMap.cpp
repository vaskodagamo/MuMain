#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#ifdef _EDITOR

#include "App/Control/ControlMapArguments.h"
#include "Assets/EditorText.h"
#include "Core/EditorCamera.h"
#include "Core/LiveMap.h"
#include "Core/LiveMapEdit.h"
#include "Core/OfflineWorld.h"
#include "MapInspect/CameraFraming.h"
#include "UI/MapEditor/MapEditorUI.h"
#include "World/MapInfra/MapNumbers.h"

#include "json.hpp"

#include <algorithm>
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
namespace Inspect = Editor::MapInspect;

constexpr int FIRST_WORLD_FOLDER = World::MapNumbers::FIRST_FOLDER;
constexpr int LAST_WORLD_FOLDER = World::MapNumbers::LAST_FOLDER;
// Any yaw works (it wraps); the bound only keeps the number sane.
constexpr float YAW_LIMIT = 3600.0f;

json Triple(const float (&values)[3])
{
    return json::array({values[0], values[1], values[2]});
}

json IdentityJson()
{
    const Inspect::MapIdentity identity = Editor::LiveMap::Identity();
    json result;
    result["world"] = identity.world;
    result["map"] = identity.map;
    result["name"] = identity.name;
    result["tiles"] = json::array({Inspect::MAP_TILES, Inspect::MAP_TILES});
    result["tile_world_size"] = Inspect::TILE_WORLD_SIZE;
    result["offline"] = Editor::OfflineWorld::IsActive();
    return result;
}

json CatalogJson()
{
    const Editor::LiveMap::CatalogStatus catalog = Editor::LiveMap::Catalog();
    json result;
    result["available"] = catalog.available;
    result["models"] = catalog.models;
    if (!catalog.file.empty())
        result["file"] = Arguments::PathJson(catalog.file);
    if (!catalog.problem.empty())
        result["problem"] = catalog.problem;
    return result;
}

// Every model the map has, by type: the names a script may use for it.
json ModelsJson()
{
    std::vector<Editor::MapScript::ModelInfo> models = Editor::LiveMapEdit::LoadedModels();
    std::sort(models.begin(), models.end(), [](const auto& a, const auto& b) { return a.type < b.type; });
    json list = json::array();
    for (const Editor::MapScript::ModelInfo& model : models)
    {
        json entry = {{"type", model.type}, {"name", model.modelName}};
        if (!model.catalogName.empty())
            entry["catalog_name"] = model.catalogName;
        list.push_back(std::move(entry));
    }
    return list;
}

const char* GateKindName(int kind)
{
    if (kind == Editor::LiveMap::GATE_ENTER)
        return "enter";
    if (kind == Editor::LiveMap::GATE_ARRIVAL)
        return "arrival";
    return "other";
}

json GatesJson()
{
    json gates = json::array();
    for (const Editor::LiveMap::Gate& gate : Editor::LiveMap::Gates())
    {
        json entry;
        entry["number"] = gate.number;
        entry["kind"] = GateKindName(gate.kind);
        entry["flag"] = gate.kind;
        entry["area"] = json::array({gate.x1, gate.y1, gate.x2, gate.y2});
        entry["level"] = gate.level;
        if (gate.kind == Editor::LiveMap::GATE_ENTER)
        {
            entry["target"] = gate.target;
            entry["target_map"] = gate.targetMap;
        }
        gates.push_back(std::move(entry));
    }
    return gates;
}

json PoseJson(const Editor::Camera::Pose& pose)
{
    json result;
    result["target"] = Triple(pose.target);
    result["position"] = Triple(pose.position);
    result["target_tile"] = json::array({Inspect::TileOf(pose.target[0]), Inspect::TileOf(pose.target[1])});
    result["yaw"] = pose.yaw;
    result["pitch"] = pose.pitch;
    result["distance"] = pose.distance;
    result["view_range"] = pose.viewRange;
    result["vertical_fov"] = pose.verticalFov;
    return result;
}

// map-camera with `topdown: {rect: [...]}`.
std::string FrameTopDown(const Request& request)
{
    std::string encoded;
    (void)request.GetStructured("topdown", encoded);
    const json topdown = json::parse(encoded, nullptr, false);
    Inspect::CellRect area;
    std::string error = "`topdown` is {\"rect\": [x0, y0, x1, y1]}";
    if (!topdown.is_object() || !topdown.contains("rect") || !Arguments::AreaFromJson(topdown.at("rect"), area, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    Editor::Camera::Pose pose;
    if (!Editor::Camera::FrameTopDown(area, pose))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, "the free-fly camera could not be switched on");

    json result;
    result["area"] = Arguments::AreaJson(area);
    result["pose"] = PoseJson(pose);
    return EncodeResult(request.EncodedId(), result.dump());
}

// The distance of a tile framing: `distance`, or the one that puts the camera `height`
// above the ground at the given pitch.
bool ReadFramingDistance(const Request& request, float pitch, float& distance, std::string& error)
{
    if (request.Has("distance") && request.Has("height"))
    {
        error = "give `distance` or `height`, not both";
        return false;
    }
    if (!request.Has("height"))
        return Arguments::ReadOptionalNumber(request, "distance", Inspect::CLOSEST_DISTANCE, Inspect::FARTHEST_DISTANCE,
                                             distance, error);

    float height = 0.0f;
    if (!Arguments::ReadOptionalNumber(request, "height", Inspect::CLOSEST_DISTANCE, Inspect::FARTHEST_DISTANCE, height,
                                       error))
        return false;
    distance = Inspect::DistanceForHeight(height, pitch);
    return true;
}

// map-camera with `tile: [x, y]`.
std::string FrameTile(const Request& request)
{
    int x = 0;
    int y = 0;
    float yaw = Inspect::START_YAW;
    float pitch = Inspect::START_PITCH;
    float distance = Inspect::START_DISTANCE;
    std::string error;
    const bool valid = Arguments::ReadTile(request, "tile", x, y, error) &&
                       Arguments::ReadOptionalNumber(request, "yaw", -YAW_LIMIT, YAW_LIMIT, yaw, error) &&
                       Arguments::ReadOptionalNumber(request, "pitch", Inspect::SHALLOWEST_PITCH,
                                                     Inspect::STEEPEST_PITCH, pitch, error) &&
                       ReadFramingDistance(request, pitch, distance, error);
    if (!valid)
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    Editor::Camera::Pose pose;
    if (!Editor::Camera::FrameTile(x, y, yaw, pitch, distance, pose))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, "the free-fly camera could not be switched on");

    json result;
    result["tile"] = json::array({x, y});
    result["pose"] = PoseJson(pose);
    return EncodeResult(request.EncodedId(), result.dump());
}
} // namespace

namespace App::Control::Commands
{
std::string MapOpen(const Request& request, std::unique_ptr<Act>&)
{
    int world = 0;
    if (!request.GetInt("world", world) || world < FIRST_WORLD_FOLDER || world > LAST_WORLD_FOLDER)
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`world` is the map folder number (Data/World{N}), from " +
                               std::to_string(FIRST_WORLD_FOLDER) + " to " + std::to_string(LAST_WORLD_FOLDER));
    }
    if (!Editor::OfflineWorld::IsActive())
    {
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed,
                           "map-open switches maps of an offline editor session; start the client with --editor "
                           "--world N");
    }

    std::string error;
    if (!Editor::OfflineWorld::Open(world, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);

    json result = IdentityJson();
    result["objects"] = Editor::LiveMap::Objects().size();
    result["catalog"] = CatalogJson();
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string MapInfo(const Request& request, std::unique_ptr<Act>&)
{
    bool listModels = false;
    if (request.Has("models") && !request.GetBool("models", listModels))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`models` is true or false");

    json result = IdentityJson();
    result["objects"] = Editor::LiveMap::Objects().size();
    result["catalog"] = CatalogJson();
    if (listModels)
        result["models"] = ModelsJson();
    result["gates"] = GatesJson();
    result["unsaved"] = Arguments::UnsavedJson();
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string MapCamera(const Request& request, std::unique_ptr<Act>&)
{
    const bool topdown = request.Has("topdown");
    if (topdown == request.Has("tile"))
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`map-camera` takes `tile: [x, y]` (with `yaw`, `pitch`, `distance` or `height`) or "
                           "`topdown: {\"rect\": [x0, y0, x1, y1]}`");
    }
    return topdown ? FrameTopDown(request) : FrameTile(request);
}

std::string MapTab(const Request& request, std::unique_ptr<Act>&)
{
    std::string tab;
    if (!request.GetString("tab", tab) || !g_MapEditorUI.ShowTab(tab))
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`tab` is one of the Map Editor's tabs: " +
                               Editor::Text::Join(CMapEditorUI::TabNames(), ", "));
    }
    json result;
    result["tab"] = tab;
    result["tabs"] = CMapEditorUI::TabNames();
    return EncodeResult(request.EncodedId(), result.dump());
}
} // namespace App::Control::Commands

#endif // _EDITOR
