#include "ScriptReport.h"

#ifdef _EDITOR

#include "NameResolver.h"

#include <cmath>
#include <string>

namespace Editor::MapScript
{
namespace
{
using Editor::Editing::ObjectState;
using nlohmann::json;

json AreaJson(const CellRect& area)
{
    if (area.IsEmpty())
        return nullptr;
    return json::array({area.minX, area.minY, area.maxX, area.maxY});
}

json LayerJson(const LayerChange& change)
{
    return json{{"cells", change.cells}, {"area", AreaJson(change.area)}};
}

json Triple(const float (&values)[3])
{
    return json::array({values[0], values[1], values[2]});
}

json StateJson(const ObjectState& state, const MapContext& context)
{
    json entry;
    entry["model"] = ModelDisplayName(context, state.type);
    entry["type"] = state.type;
    entry["tile"] = json::array({static_cast<int>(std::floor(state.position[0] / TILE_WORLD)),
                                 static_cast<int>(std::floor(state.position[1] / TILE_WORLD))});
    entry["position"] = Triple(state.position);
    entry["angle"] = Triple(state.angle);
    entry["scale"] = state.scale;
    return entry;
}

const char* ChangeName(ObjectChangeKind kind)
{
    switch (kind)
    {
    case ObjectChangeKind::Added:
        return "added";
    case ObjectChangeKind::Removed:
        return "removed";
    case ObjectChangeKind::Changed:
        return "changed";
    }
    return "changed";
}

json ObjectDiffJson(const ObjectDiff& diff, const MapContext& context)
{
    json entry = StateJson(diff.after ? *diff.after : *diff.before, context);
    entry["change"] = ChangeName(diff.kind);
    entry["id"] = diff.id;
    if (diff.kind == ObjectChangeKind::Changed)
        entry["before"] = StateJson(*diff.before, context);
    return entry;
}

json ObjectsJson(const MapChanges& changes, const MapContext& context)
{
    json list = json::array();
    for (std::size_t i = 0; i < changes.objects.size() && i < LISTED_OBJECTS; ++i)
        list.push_back(ObjectDiffJson(changes.objects[i], context));
    json objects;
    objects["added"] = changes.Count(ObjectChangeKind::Added);
    objects["removed"] = changes.Count(ObjectChangeKind::Removed);
    objects["changed"] = changes.Count(ObjectChangeKind::Changed);
    objects["listed"] = list.size();
    objects["list"] = std::move(list);
    return objects;
}

json OpJson(const OpReport& report)
{
    json entry;
    entry["op"] = report.index;
    entry["name"] = report.name;
    entry["area"] = AreaJson(report.area);
    if (report.requested >= 0)
        entry["requested"] = report.requested;
    if (report.placed >= 0)
        entry["placed"] = report.placed;
    if (report.selected >= 0)
        entry["selected"] = report.selected;
    if (!report.ids.empty())
    {
        json ids = json::array();
        for (std::size_t i = 0; i < report.ids.size() && i < LISTED_OBJECTS; ++i)
            ids.push_back(report.ids[i]);
        entry["ids"] = std::move(ids);
    }
    if (!report.warnings.empty())
        entry["warnings"] = report.warnings;
    return entry;
}
} // namespace

json ReportJson(const std::vector<OpReport>& reports, const MapChanges& changes, const MapContext& context)
{
    json ops = json::array();
    json warnings = json::array();
    for (const OpReport& report : reports)
    {
        ops.push_back(OpJson(report));
        for (const std::string& warning : report.warnings)
            warnings.push_back("ops[" + std::to_string(report.index) + "] (" + report.name + "): " + warning);
    }
    json layers;
    layers["height"] = LayerJson(changes.height);
    layers["texture1"] = LayerJson(changes.texture1);
    layers["texture2"] = LayerJson(changes.texture2);
    layers["attribute"] = LayerJson(changes.attribute);
    layers["light"] = LayerJson(changes.light);
    layers["objects"] = ObjectsJson(changes, context);

    json result;
    result["ops"] = std::move(ops);
    result["changes"] = std::move(layers);
    result["warnings"] = std::move(warnings);
    return result;
}
} // namespace Editor::MapScript

#endif // _EDITOR
