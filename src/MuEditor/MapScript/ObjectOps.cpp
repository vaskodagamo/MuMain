#include "ObjectOps.h"

#ifdef _EDITOR

#include "PoissonScatter.h"
#include "ScatterRules.h"
#include "SurfaceOps.h" // SentinelAllows

#include "Editing/GizmoMath.h"
#include "Editing/ObjectTransform.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>
#include <utility>

namespace Editor::MapScript::Ops
{
namespace
{
namespace Transform = Editor::Editing::Transform;
using Editor::Editing::ObjectState;

// Objects live in the object grid, which ends at the map's far edge.
constexpr float OBJECT_GRID_WORLD = static_cast<float>(Editor::MapInspect::MAP_TILES) * TILE_WORLD;
constexpr int YAW = 2;
constexpr const char* SELECT_FIELD = "select";
constexpr const char* UNDER_FIELD = "under";
constexpr const char* MATCHES_NOTHING = ": matches no object (map-query lists the objects of an area)";

// The tile an object stands on.
std::pair<int, int> TileUnder(const ObjectState& object)
{
    const int last = Editor::MapInspect::MAP_TILES - 1;
    return {std::clamp(static_cast<int>(std::floor(object.position[0] / TILE_WORLD)), 0, last),
            std::clamp(static_cast<int>(std::floor(object.position[1] / TILE_WORLD)), 0, last)};
}

float Ground(const MapState& state, const MapContext& context, float worldX, float worldY)
{
    return GroundHeight(state.terrain, worldX, worldY, context.specialHeight);
}

float HeightFor(const ObjectHeight& height, float ground, float aboveGround)
{
    switch (height.mode)
    {
    case HeightMode::Ground:
        return ground;
    case HeightMode::Offset:
        return ground + height.value;
    case HeightMode::Absolute:
        return height.value;
    case HeightMode::Keep:
        return ground + aboveGround;
    }
    return ground;
}

bool OnObjectGrid(float worldX, float worldY)
{
    return worldX >= 0.0f && worldY >= 0.0f && worldX < OBJECT_GRID_WORLD && worldY < OBJECT_GRID_WORLD;
}

Point TilePoint(const ObjectState& state)
{
    return Point{state.position[0] / TILE_WORLD, state.position[1] / TILE_WORLD};
}

MapObject& AddObject(MapState& state, const Op& op, const ObjectState& objectState)
{
    MapObject object;
    object.key = NEW_OBJECT;
    object.id = state.nextId++;
    object.state = objectState;
    object.placedByOp = op.index;
    state.objects.push_back(object);
    return state.objects.back();
}

int PickModel(const std::vector<ModelChoice>& models, float totalWeight, Random& random)
{
    float pick = random.Uniform() * totalWeight;
    for (const ModelChoice& model : models)
    {
        if (pick < model.weight)
            return model.type;
        pick -= model.weight;
    }
    return models.back().type;
}

bool ScatterCount(const Op& op, const ScatterEdit& edit, int& count, std::string& error)
{
    if (edit.count > 0)
    {
        count = edit.count;
        return true;
    }
    const long wanted = std::lround(edit.density * static_cast<float>(TileArea(edit.shape)));
    if (wanted > MAX_SCATTER_OBJECTS)
    {
        error = OpField(op, "density") + ": gives " + std::to_string(wanted) +
                " objects on this shape, more than the " + std::to_string(MAX_SCATTER_OBJECTS) + " one scatter places";
        return false;
    }
    count = static_cast<int>(wanted);
    return true;
}

void PlaceScattered(const Op& op, const ScatterEdit& edit, const MapContext& context, const std::vector<Point>& points,
                    Random& random, MapState& state, OpReport& report)
{
    const float totalWeight = std::accumulate(edit.models.begin(), edit.models.end(), 0.0f,
                                              [](float sum, const ModelChoice& model) { return sum + model.weight; });
    for (const Point& point : points)
    {
        ObjectState objectState;
        objectState.type = PickModel(edit.models, totalWeight, random);
        objectState.scale = random.Range(edit.scaleRange[0], edit.scaleRange[1]);
        objectState.angle[YAW] = random.Range(edit.yawRange[0], edit.yawRange[1]);
        objectState.position[0] = point.x * TILE_WORLD;
        objectState.position[1] = point.y * TILE_WORLD;
        objectState.position[2] = Ground(state, context, objectState.position[0], objectState.position[1]);
        report.ids.push_back(AddObject(state, op, objectState).id);
        if (!edit.markAttribute)
            continue;
        const auto x = static_cast<int>(std::floor(point.x));
        const auto y = static_cast<int>(std::floor(point.y));
        state.terrain.attribute[Editor::MapInspect::CellIndex(x, y)] = *edit.markAttribute;
    }
}

bool CheckIds(const Op& op, const char* field, const ObjectSelector& selector, const MapState& state,
              std::string& error)
{
    std::unordered_set<int> ids;
    for (const MapObject& object : state.objects)
        ids.insert(object.id);
    for (int id : selector.ids)
    {
        if (ids.count(id) != 0)
            continue;
        error = OpField(op, std::string(field) + ".ids") + ": no object has id " + std::to_string(id) +
                " (ids run from 0 to " + std::to_string(state.nextId - 1) +
                "; an object deleted earlier in the script is gone)";
        return false;
    }
    return true;
}

bool Move(const Op& op, const ObjectEdit& edit, const std::vector<std::size_t>& picked, const MapContext& context,
          MapState& state, std::string& error)
{
    float dx = edit.by ? edit.by->x * TILE_WORLD : 0.0f;
    float dy = edit.by ? edit.by->y * TILE_WORLD : 0.0f;
    if (edit.to)
    {
        std::vector<ObjectState> states;
        for (std::size_t index : picked)
            states.push_back(state.objects[index].state);
        float centre[3] = {};
        Transform::Pivot(states, centre);
        dx = edit.to->x * TILE_WORLD - centre[0];
        dy = edit.to->y * TILE_WORLD - centre[1];
    }
    for (std::size_t index : picked)
    {
        ObjectState& object = state.objects[index].state;
        const float x = object.position[0] + dx;
        const float y = object.position[1] + dy;
        if (!OnObjectGrid(x, y))
        {
            error = OpLabel(op) + ": object " + std::to_string(state.objects[index].id) + " would leave the map";
            return false;
        }
        const float aboveGround = object.position[2] - Ground(state, context, object.position[0], object.position[1]);
        object.position[0] = x;
        object.position[1] = y;
        object.position[2] = HeightFor(edit.height, Ground(state, context, x, y), aboveGround);
    }
    return true;
}

bool Rotate(const Op& op, const ObjectEdit& edit, const std::vector<std::size_t>& picked, MapState& state,
            std::string& error)
{
    Transform::Delta turn;
    turn.kind = Transform::Kind::Rotate;
    turn.axis = Editor::Gizmo::AXIS_Z;
    turn.degrees = edit.degrees;
    std::vector<ObjectState> states;
    for (std::size_t index : picked)
        states.push_back(state.objects[index].state);
    float pivot[3] = {};
    Transform::Pivot(states, pivot);
    for (std::size_t i = 0; i < picked.size(); ++i)
    {
        ObjectState& object = state.objects[picked[i]].state;
        if (edit.absolute)
        {
            object.angle[YAW] = edit.degrees;
            continue;
        }
        const float* around = edit.pivot == RotatePivot::Center ? pivot : object.position;
        const ObjectState turned = Transform::Apply({object}, around, turn).front();
        if (!OnObjectGrid(turned.position[0], turned.position[1]))
        {
            error = OpLabel(op) + ": object " + std::to_string(state.objects[picked[i]].id) + " would leave the map";
            return false;
        }
        object = turned;
    }
    return true;
}

void Scale(const ObjectEdit& edit, const std::vector<std::size_t>& picked, MapState& state)
{
    for (std::size_t index : picked)
    {
        ObjectState& object = state.objects[index].state;
        const float scaled = edit.absolute ? edit.factor : object.scale * edit.factor;
        object.scale = std::max(Transform::MIN_OBJECT_SCALE, scaled);
    }
}

void Delete(const std::vector<std::size_t>& picked, MapState& state)
{
    std::vector<bool> doomed(state.objects.size(), false);
    for (std::size_t index : picked)
        doomed[index] = true;
    std::size_t kept = 0;
    for (std::size_t i = 0; i < state.objects.size(); ++i)
    {
        if (!doomed[i])
            state.objects[kept++] = state.objects[i];
    }
    state.objects.resize(kept);
}

void DropToGround(const std::vector<std::size_t>& picked, const MapContext& context, MapState& state)
{
    for (std::size_t index : picked)
    {
        ObjectState& object = state.objects[index].state;
        object.position[2] = Ground(state, context, object.position[0], object.position[1]);
    }
}

bool Matches(const MapObject& object, const ObjectSelector& selector, const std::unordered_set<int>& ids)
{
    if (!selector.ids.empty() && ids.count(object.id) == 0)
        return false;
    const bool modelListed = selector.models.empty() || std::any_of(selector.models.begin(), selector.models.end(),
                                                                    [&object](const ModelChoice& model)
                                                                    { return model.type == object.state.type; });
    if (!modelListed)
        return false;
    if (selector.inside && InsideDistance(*selector.inside, TilePoint(object.state)) < 0.0f)
        return false;
    return !selector.placedBy || object.placedByOp == *selector.placedBy;
}
} // namespace

bool ApplyPlace(const Op& op, const PlaceEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                std::string&)
{
    ObjectState objectState;
    objectState.type = edit.model.type;
    objectState.position[0] = edit.at.x * TILE_WORLD;
    objectState.position[1] = edit.at.y * TILE_WORLD;
    const float ground = Ground(state, context, objectState.position[0], objectState.position[1]);
    objectState.position[2] = HeightFor(edit.height, ground, 0.0f);
    std::copy(std::begin(edit.angle), std::end(edit.angle), std::begin(objectState.angle));
    objectState.scale = edit.scale;
    report.placed = 1;
    report.ids.push_back(AddObject(state, op, objectState).id);
    return true;
}

bool ApplyScatter(const Op& op, const ScatterEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                  std::string& error)
{
    int count = 0;
    if (!ScatterCount(op, edit, count, error))
        return false;
    report.requested = count;
    report.placed = 0;
    const bool anyWeight = std::any_of(edit.models.begin(), edit.models.end(),
                                       [](const ModelChoice& model) { return model.weight > 0.0f; });
    if (!anyWeight)
    {
        error = OpField(op, "models") + ": every weight is 0; give at least one model a weight above 0";
        return false;
    }
    const ScatterRules rules(op, edit, context, state);
    ScatterSettings settings;
    settings.count = count;
    settings.minSpacing = edit.minSpacing;
    settings.bounds = ShapeBounds(edit.shape);
    settings.maxAttempts = count * ATTEMPTS_PER_OBJECT;
    Random random(edit.seed);
    const std::vector<Point> points =
        ScatterPoints(settings, random, [&rules](Point point) { return rules.KeepChance(point); });
    PlaceScattered(op, edit, context, points, random, state, report);
    report.placed = static_cast<int>(points.size());
    if (report.placed < count)
        report.warnings.push_back("placed " + std::to_string(report.placed) + " of " + std::to_string(count) +
                                  ": the shape has no more room at this min_spacing with these avoid rules");
    return true;
}

std::vector<std::size_t> Select(const ObjectSelector& selector, const MapState& state)
{
    const std::unordered_set<int> ids(selector.ids.begin(), selector.ids.end());
    std::vector<std::size_t> picked;
    for (std::size_t i = 0; i < state.objects.size(); ++i)
    {
        if (Matches(state.objects[i], selector, ids))
            picked.push_back(i);
    }
    return picked;
}

bool ApplyObjectEdit(const Op& op, const ObjectEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                     std::string& error)
{
    if (!CheckIds(op, SELECT_FIELD, edit.select, state, error))
        return false;
    const std::vector<std::size_t> picked = Select(edit.select, state);
    if (picked.empty())
    {
        error = OpField(op, SELECT_FIELD) + MATCHES_NOTHING;
        return false;
    }
    report.selected = static_cast<int>(picked.size());
    for (std::size_t index : picked)
        report.ids.push_back(state.objects[index].id);
    switch (op.kind)
    {
    case OpKind::ObjectMove:
        return Move(op, edit, picked, context, state, error);
    case OpKind::ObjectRotate:
        return Rotate(op, edit, picked, state, error);
    case OpKind::ObjectScale:
        Scale(edit, picked, state);
        return true;
    case OpKind::ObjectDelete:
        Delete(picked, state);
        return true;
    case OpKind::ObjectDropToGround:
        DropToGround(picked, context, state);
        return true;
    default:
        error = OpLabel(op) + ": is not an object edit";
        return false;
    }
}

bool MarkUnderObjects(const Op& op, const AttributeEdit& edit, const MapContext& context, MapState& state,
                      OpReport& report, std::string& error)
{
    if (!CheckIds(op, UNDER_FIELD, *edit.under, state, error))
        return false;
    const std::vector<std::size_t> picked = Select(*edit.under, state);
    if (picked.empty())
    {
        error = OpField(op, UNDER_FIELD) + MATCHES_NOTHING;
        return false;
    }
    std::vector<std::pair<int, int>> tiles;
    for (std::size_t index : picked)
    {
        const std::pair<int, int> tile = TileUnder(state.objects[index].state);
        if (!SentinelAllows(op, context, tile.first, tile.second, edit.value, error))
            return false;
        tiles.push_back(tile);
    }
    CellRect area{tiles.front().first, tiles.front().second, tiles.front().first, tiles.front().second};
    for (const auto& [x, y] : tiles)
    {
        state.terrain.attribute[Editor::MapInspect::CellIndex(x, y)] = edit.value;
        area = CellRect{std::min(area.minX, x), std::min(area.minY, y), std::max(area.maxX, x), std::max(area.maxY, y)};
    }
    report.selected = static_cast<int>(picked.size());
    for (std::size_t index : picked)
        report.ids.push_back(state.objects[index].id);
    report.area = area;
    return true;
}
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
