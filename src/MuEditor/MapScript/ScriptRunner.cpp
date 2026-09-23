#include "ScriptRunner.h"

#ifdef _EDITOR

#include "NameResolver.h"
#include "ObjectOps.h"
#include "ScriptCost.h"
#include "SurfaceOps.h"
#include "TerrainOps.h"

#include <variant>

namespace Editor::MapScript
{
namespace
{
bool ApplyOp(const Op& op, const MapContext& context, MapState& state, OpReport& report, std::string& error)
{
    if (const auto* terrain = std::get_if<TerrainEdit>(&op.data))
        return Ops::ApplyTerrain(op, *terrain, context, state, report, error);
    if (const auto* texture = std::get_if<TextureEdit>(&op.data))
        return Ops::ApplyTexture(op, *texture, state, report, error);
    if (const auto* attribute = std::get_if<AttributeEdit>(&op.data))
        return Ops::ApplyAttribute(op, *attribute, context, state, report, error);
    if (const auto* light = std::get_if<LightEdit>(&op.data))
        return Ops::ApplyLight(op, *light, state, report, error);
    if (const auto* place = std::get_if<PlaceEdit>(&op.data))
        return Ops::ApplyPlace(op, *place, context, state, report, error);
    if (const auto* scatter = std::get_if<ScatterEdit>(&op.data))
        return Ops::ApplyScatter(op, *scatter, context, state, report, error);
    if (const auto* edit = std::get_if<ObjectEdit>(&op.data))
        return Ops::ApplyObjectEdit(op, *edit, context, state, report, error);
    error = OpLabel(op) + ": has no fields";
    return false;
}

bool WithinObjectLimit(const Op& op, const MapContext& context, const MapState& state, std::string& error)
{
    if (context.maxObjects == 0 || state.objects.size() <= context.maxObjects)
        return true;
    error = OpLabel(op) + ": the map would hold " + std::to_string(state.objects.size()) +
            " objects; its object file holds at most " + std::to_string(context.maxObjects);
    return false;
}
} // namespace

bool RunScript(EditScript script, const MapContext& context, MapState& state, std::vector<OpReport>& reports,
               std::string& error)
{
    reports.clear();
    if (!CheckScriptCost(script, error) || !ResolveNames(script, context, error))
        return false;
    for (const Op& op : script.ops)
    {
        OpReport report;
        report.index = op.index;
        report.name = OpName(op.kind);
        if (!ApplyOp(op, context, state, report, error) || !WithinObjectLimit(op, context, state, error))
            return false;
        reports.push_back(std::move(report));
    }
    return true;
}
} // namespace Editor::MapScript

#endif // _EDITOR
