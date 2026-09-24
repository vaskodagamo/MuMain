#include "MapExport.h"

#ifdef _EDITOR

#include "AreaStats.h"
#include "AttributePalette.h"
#include "MapObjectList.h"
#include "PngFile.h"
#include "TilePalette.h"

#include "Assets/EditorText.h"

#include <json.hpp>

#include <fstream>
#include <system_error>

namespace Editor::MapInspect
{
namespace
{
using nlohmann::json;

constexpr const char* LEGEND_FILE = "legend.json";
constexpr const char* OBJECTS_FILE = "objects.json";
constexpr const char* OBJECTS_KEY = "objects";
constexpr const char* PNG_EXTENSION = ".png";
constexpr const char* LEGEND_SCHEMA = "mu-map-export/1";
constexpr int JSON_INDENT = 2;

constexpr const char* ORIENTATION = "one pixel per tile; pixel (column, row) is tile (x0 + column, y1 - row): +x to "
                                    "the right, +y (north) up, as the top-down camera (yaw 0) shows the map";

json AreaJson(const CellRect& area)
{
    return json::array({area.minX, area.minY, area.maxX, area.maxY});
}

json ColorJson(const Rgb& color)
{
    return json::array({color.r, color.g, color.b});
}

bool WriteText(const std::filesystem::path& file, const std::string& text, std::string& error)
{
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
    out.close();
    if (!out)
    {
        error = "cannot write " + Editor::Text::PathToUtf8(file);
        return false;
    }
    return true;
}

// `document` with each top-level field on a line of its own and each entry of the
// array `listKey` on one line: thousands of objects stay readable line by line (and
// greppable) without spreading every number over its own line.
std::string OneEntryPerLine(const json& document, const std::string& listKey)
{
    std::string text = "{";
    bool firstField = true;
    for (const auto& [key, value] : document.items())
    {
        text += firstField ? "\n  " : ",\n  ";
        firstField = false;
        text += json(key).dump() + ": ";
        if (key != listKey || !value.is_array() || value.empty())
        {
            text += value.dump();
            continue;
        }
        text += "[";
        for (std::size_t i = 0; i < value.size(); ++i)
            text += (i == 0 ? "\n    " : ",\n    ") + value[i].dump();
        text += "\n  ]";
    }
    return text + "\n}\n";
}

json HeightLegend(const HeightRange& heights)
{
    json legend;
    legend["format"] = "grey8";
    legend["min"] = heights.min;
    legend["max"] = heights.max;
    legend["units_per_level"] = heights.UnitsPerLevel();
    legend["decode"] = "height = min + pixel * units_per_level (world units; one tile is 100)";
    return legend;
}

json AttributeLegend(const AreaStats& stats)
{
    json values = json::array();
    for (const auto& [value, tiles] : stats.attributes)
    {
        values.push_back({{"value", value},
                          {"flags", AttributeFlagNames(value)},
                          {"color", ColorJson(AttributeColor(value))},
                          {"tiles", tiles}});
    }
    json flags = json::object();
    for (const AttributeFlag& flag : AttributeFlags())
        flags[std::string(flag.name)] = flag.bit;
    json rules = json::array();
    for (const AttributeColorRule& rule : AttributeColorRules())
    {
        rules.push_back({{"flag", rule.flag.empty() ? "walkable" : std::string(rule.flag)},
                         {"color", ColorJson(rule.color)},
                         {"meaning", rule.meaning}});
    }

    json legend;
    legend["format"] = "rgb8";
    legend["values"] = std::move(values);
    legend["flags"] = std::move(flags);
    legend["color_rules"] = std::move(rules);
    legend["decode"] = "the strongest flag of a tile picks its colour (color_rules, first match); values lists "
                       "every attribute value in the area with its colour";
    return legend;
}

std::string SlotFile(const std::vector<TileSlotInfo>& slots, int slot)
{
    for (const TileSlotInfo& info : slots)
    {
        if (info.slot == slot)
            return info.file;
    }
    return {};
}

json TextureLegend(const std::map<int, int>& histogram, const std::vector<TileSlotInfo>& slots)
{
    json entries = json::array();
    for (const auto& [slot, tiles] : histogram)
    {
        entries.push_back({{"slot", slot},
                           {"name", TileSlotName(slot)},
                           {"file", SlotFile(slots, slot)},
                           {"color", ColorJson(TileSlotColor(slot))},
                           {"tiles", tiles}});
    }
    json legend;
    legend["format"] = "rgb8";
    legend["slots"] = std::move(entries);
    legend["decode"] = "each slot has its own colour (slots); slot 255 on layer 2 means no overlay texture";
    return legend;
}

json UnitLegend(const char* format, const char* decode)
{
    json legend;
    legend["format"] = format;
    legend["decode"] = decode;
    return legend;
}

json ObjectsLegend(std::size_t inside, std::size_t total)
{
    json legend;
    legend["format"] = "grey8";
    legend["objects_file"] = OBJECTS_FILE;
    legend["objects_in_area"] = inside;
    legend["objects_on_map"] = total;
    legend["levels"] = {{"0", 0}, {"1", OccupancyLevel(1)}, {"2", OccupancyLevel(2)}, {"3", OccupancyLevel(3)}};
    legend["decode"] = "black: no object on the tile; brighter: more objects (levels); objects.json lists them";
    return legend;
}

json LayerLegend(MapLayer layer, const AreaStats& stats, const HeightRange& heights,
                 const std::vector<TileSlotInfo>& slots, std::size_t objectsInside, std::size_t objectsTotal)
{
    switch (layer)
    {
    case MapLayer::Height:
        return HeightLegend(heights);
    case MapLayer::Attribute:
        return AttributeLegend(stats);
    case MapLayer::Texture1:
        return TextureLegend(stats.baseTiles, slots);
    case MapLayer::Texture2:
        return TextureLegend(stats.overlayTiles, slots);
    case MapLayer::Alpha:
        return UnitLegend("grey8", "layer 2 opacity = pixel / 255");
    case MapLayer::Light:
        return UnitLegend("rgb8", "light map colour = pixel / 255 per channel (1 = full light)");
    case MapLayer::Objects:
        return ObjectsLegend(objectsInside, objectsTotal);
    }
    return json::object();
}

json ObjectsDocument(const std::vector<MapObjectRecord>& objects, const std::vector<std::size_t>& inside,
                     const ExportRequest& request)
{
    json list = json::array();
    for (std::size_t index : inside)
        list.push_back(ObjectToJson(objects[index], index));

    json document;
    document["world"] = request.identity.world;
    document["map"] = request.identity.map;
    document["area"] = AreaJson(request.area);
    document["objects_on_map"] = objects.size();
    document[OBJECTS_KEY] = std::move(list);
    return document;
}

bool CreateFolder(const std::filesystem::path& folder, std::string& error)
{
    std::error_code failure;
    std::filesystem::create_directories(folder, failure);
    if (failure)
    {
        error = "cannot create " + Editor::Text::PathToUtf8(folder) + ": " + failure.message();
        return false;
    }
    return true;
}

// Everything one export writes, filled in as the layers go out.
struct ExportRun
{
    const TerrainView& terrain;
    const std::vector<MapObjectRecord>& objects;
    const ExportRequest& request;
    ExportResult& result;
    std::vector<std::size_t> objectsInside;
};

bool WriteLayerImage(ExportRun& run, MapLayer layer, const HeightRange& heights, std::string& error)
{
    const Image image = layer == MapLayer::Objects ? RenderObjectOccupancy(run.objects, run.request.area)
                                                   : RenderTerrainLayer(run.terrain, layer, run.request.area, heights);
    const std::filesystem::path file = run.request.folder / LayerFileName(layer);
    if (!WritePng(file, image, error))
        return false;
    run.result.files.push_back(file);
    return true;
}

bool WriteObjectList(ExportRun& run, std::string& error)
{
    const std::filesystem::path file = run.request.folder / OBJECTS_FILE;
    const json document = ObjectsDocument(run.objects, run.objectsInside, run.request);
    if (!WriteText(file, OneEntryPerLine(document, OBJECTS_KEY), error))
        return false;
    run.result.files.push_back(file);
    return true;
}
} // namespace

std::string LayerFileName(MapLayer layer)
{
    return std::string(MapLayerName(layer)) + PNG_EXTENSION;
}

bool ExportMap(const TerrainView& terrain, const std::vector<MapObjectRecord>& objects,
               const std::vector<TileSlotInfo>& slots, const ExportRequest& request, ExportResult& result,
               std::string& error)
{
    if (request.layers.empty())
    {
        error = "no layer to export";
        return false;
    }
    if (!CreateFolder(request.folder, error))
        return false;

    ExportRun run{terrain, objects, request, result, ObjectsInside(objects, request.area)};
    const AreaStats stats = MeasureArea(terrain, request.area);
    const HeightRange heights = MeasureHeightRange(terrain, request.area);

    json layers = json::object();
    for (MapLayer layer : request.layers)
    {
        if (!WriteLayerImage(run, layer, heights, error))
            return false;
        if (layer == MapLayer::Objects && !WriteObjectList(run, error))
            return false;
        json entry = LayerLegend(layer, stats, heights, slots, run.objectsInside.size(), objects.size());
        entry["file"] = LayerFileName(layer);
        layers[std::string(MapLayerName(layer))] = std::move(entry);
    }

    json legend;
    legend["schema"] = LEGEND_SCHEMA;
    legend["world"] = request.identity.world;
    legend["map"] = request.identity.map;
    legend["map_name"] = request.identity.name;
    legend["area"] = AreaJson(request.area);
    legend["size"] = json::array({request.area.Width(), request.area.Height()});
    legend["orientation"] = ORIENTATION;
    legend["tile_world_size"] = TILE_WORLD_SIZE;
    legend["layers"] = std::move(layers);

    const std::filesystem::path legendFile = request.folder / LEGEND_FILE;
    if (!WriteText(legendFile, legend.dump(JSON_INDENT) + "\n", error))
        return false;
    result.files.push_back(legendFile);
    return true;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
