#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#ifdef _EDITOR

#include "App/Control/ControlMapArguments.h"
#include "Core/LiveMap.h"
#include "MapInspect/AreaStats.h"
#include "MapInspect/AttributePalette.h"
#include "MapInspect/MapExport.h"
#include "MapInspect/MapObjectList.h"
#include "MapInspect/TilePalette.h"

#include "json.hpp"

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
namespace Inspect = Editor::MapInspect;

constexpr const char* EXPORT_FOLDER = "map-exports";
// map-query lists at most this many objects; `objects_in_area` still counts them all.
constexpr std::size_t MAX_LISTED_OBJECTS = 200;

// map-export's and map-query's area: `rect`, `tile` (map-query), or the whole map.
bool ReadQueryArea(const Request& request, bool allowTile, Inspect::CellRect& area, std::string& error)
{
    if (request.Has("rect") && request.Has("tile"))
    {
        error = "give `rect` or `tile`, not both";
        return false;
    }
    if (request.Has("rect"))
        return Arguments::ReadArea(request, "rect", area, error);
    if (allowTile && request.Has("tile"))
    {
        int x = 0;
        int y = 0;
        if (!Arguments::ReadTile(request, "tile", x, y, error))
            return false;
        area = Inspect::CellRect{x, y, x, y};
        return true;
    }
    area = Inspect::WholeMap();
    return true;
}

std::filesystem::path ExportFolder(const Request& request, int world, std::string& error)
{
    std::string requested;
    if (request.Has("out") && (!request.GetString("out", requested) || requested.empty()))
    {
        error = "`out` is a folder path; omit it for one under the repository's out/map-exports";
        return {};
    }
    if (!requested.empty())
        return Arguments::ResolvePath(requested);
    return Arguments::DefaultOutput(EXPORT_FOLDER, "World" + std::to_string(world) + "-" + Arguments::UniqueStamp());
}

json LayerNamesJson(const std::vector<Inspect::MapLayer>& layers)
{
    json names = json::array();
    for (Inspect::MapLayer layer : layers)
        names.push_back(std::string(Inspect::MapLayerName(layer)));
    return names;
}

json AttributeHistogramJson(const std::map<std::uint16_t, int>& histogram)
{
    json values = json::array();
    for (const auto& [value, tiles] : histogram)
        values.push_back({{"value", value}, {"flags", Inspect::AttributeFlagNames(value)}, {"tiles", tiles}});
    return values;
}

json TileHistogramJson(const std::map<int, int>& histogram)
{
    json slots = json::array();
    for (const auto& [slot, tiles] : histogram)
        slots.push_back({{"slot", slot}, {"name", Inspect::TileSlotName(slot)}, {"tiles", tiles}});
    return slots;
}

json ObjectsInsideJson(const std::vector<Inspect::MapObjectRecord>& objects, const Inspect::CellRect& area,
                       std::size_t& inside)
{
    const std::vector<std::size_t> indices = Inspect::ObjectsInside(objects, area);
    inside = indices.size();
    json listed = json::array();
    for (std::size_t i = 0; i < indices.size() && i < MAX_LISTED_OBJECTS; ++i)
        listed.push_back(Inspect::ObjectToJson(objects[indices[i]], indices[i]));
    return listed;
}

json StatsJson(const Inspect::AreaStats& stats)
{
    json result;
    result["area"] = Arguments::AreaJson(stats.area);
    result["tiles"] = stats.cells;
    result["height"] = {{"min", stats.height.min}, {"max", stats.height.max}, {"mean", stats.height.mean}};
    result["attributes"] = AttributeHistogramJson(stats.attributes);
    result["texture1"] = TileHistogramJson(stats.baseTiles);
    result["texture2"] = TileHistogramJson(stats.overlayTiles);
    result["alpha_mean"] = stats.overlayAlphaMean;
    result["light_mean"] = json::array({stats.lightMean[0], stats.lightMean[1], stats.lightMean[2]});
    return result;
}
} // namespace

namespace App::Control::Commands
{
std::string MapExport(const Request& request, std::unique_ptr<Act>&)
{
    Inspect::ExportRequest exportRequest;
    exportRequest.identity = Editor::LiveMap::Identity();
    std::string error;
    if (!Arguments::ReadLayers(request, exportRequest.layers, error) ||
        !ReadQueryArea(request, false, exportRequest.area, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    exportRequest.folder = ExportFolder(request, exportRequest.identity.world, error);
    if (exportRequest.folder.empty())
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    Inspect::ExportResult exported;
    if (!Inspect::ExportMap(Editor::LiveMap::Terrain(), Editor::LiveMap::Objects(), Editor::LiveMap::TileSlots(),
                            exportRequest, exported, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);

    json files = json::array();
    for (const std::filesystem::path& file : exported.files)
        files.push_back(Arguments::PathJson(file));
    json result;
    result["folder"] = Arguments::PathJson(exportRequest.folder);
    result["area"] = Arguments::AreaJson(exportRequest.area);
    result["layers"] = LayerNamesJson(exportRequest.layers);
    // ExportMap writes the legend last.
    result["legend"] = exported.files.empty() ? std::string() : Arguments::PathJson(exported.files.back());
    result["files"] = std::move(files);
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string MapQuery(const Request& request, std::unique_ptr<Act>&)
{
    Inspect::CellRect area;
    std::string error;
    if (!request.Has("rect") && !request.Has("tile"))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`map-query` needs `rect` or `tile`");
    if (!ReadQueryArea(request, true, area, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    json result = StatsJson(Inspect::MeasureArea(Editor::LiveMap::Terrain(), area));
    std::size_t inside = 0;
    result["objects"] = ObjectsInsideJson(Editor::LiveMap::Objects(), area, inside);
    result["objects_in_area"] = inside;
    return EncodeResult(request.EncodedId(), result.dump());
}
} // namespace App::Control::Commands

#endif // _EDITOR
