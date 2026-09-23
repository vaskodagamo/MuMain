#include "ObjectOpParser.h"

#ifdef _EDITOR

#include "ShapeParser.h"

#include "MapInspect/TerrainView.h" // MAP_TILES

#include <cmath>
#include <limits>

namespace Editor::MapScript::Parse
{
namespace
{
using nlohmann::json;

// Objects stand inside the object grid, which ends at the map's far edge.
constexpr float OBJECT_GRID_END = static_cast<float>(Editor::MapInspect::MAP_TILES);
constexpr float MAX_MOVE = static_cast<float>(Editor::MapInspect::MAP_TILES);
constexpr float MAX_WEIGHT = 1000.0f;
constexpr float MAX_SLOPE_DEGREES = 90.0f;
constexpr int MAX_SEED = std::numeric_limits<int>::max();
constexpr int LARGEST_ID = std::numeric_limits<int>::max();
constexpr const char* SELECT_KEY = "select";
constexpr std::size_t MOVE_VALUES = 2;

bool InObjectGrid(const ScriptReader& reader, std::string_view key, Point point)
{
    if (point.x < OBJECT_GRID_END && point.y < OBJECT_GRID_END)
        return true;
    return reader.Fail(key, "an object stands on a tile of the map: x and y below " + FormatNumber(OBJECT_GRID_END));
}

bool ObjectHeightFromText(const ScriptReader& reader, std::string_view key, bool allowKeep, ObjectHeight& height,
                          const std::string& expected)
{
    const std::string mode = reader.At(key).get<std::string>();
    if (mode == "ground")
        height = ObjectHeight{HeightMode::Ground, 0.0f};
    else if (mode == "keep" && allowKeep)
        height = ObjectHeight{HeightMode::Keep, 0.0f};
    else
        return reader.Fail(key, expected);
    return true;
}

// "ground", {"offset": n} or {"absolute": n}; "keep" too when `allowKeep`.
bool ObjectHeightAt(const ScriptReader& reader, std::string_view key, bool allowKeep, ObjectHeight& height)
{
    if (!reader.Has(key))
        return true;
    const std::string expected = allowKeep ? "is \"keep\", \"ground\", {\"offset\": n} or {\"absolute\": n}"
                                           : "is \"ground\", {\"offset\": n} or {\"absolute\": n}";
    if (reader.At(key).is_string())
        return ObjectHeightFromText(reader, key, allowKeep, height, expected);
    const ScriptReader nested = reader.Nested(key);
    if (!nested.IsObject() || nested.Has("offset") == nested.Has("absolute"))
        return reader.Fail(key, expected);
    const bool offset = nested.Has("offset");
    height.mode = offset ? HeightMode::Offset : HeightMode::Absolute;
    return nested.OnlyKeys({"offset", "absolute"}) &&
           nested.Number(offset ? "offset" : "absolute", -MAX_HEIGHT_VALUE, MAX_HEIGHT_VALUE, height.value);
}

bool AngleAt(const ScriptReader& reader, float (&angle)[3])
{
    if (!reader.Has("angle"))
        return true;
    double yaw = 0.0;
    if (!ScriptReader::ReadNumber(reader.At("angle"), yaw))
        return reader.Triple("angle", -ANGLE_LIMIT, ANGLE_LIMIT, angle);
    if (std::fabs(yaw) > ANGLE_LIMIT)
        return reader.Fail("angle", "is a heading in degrees (up to " + FormatNumber(ANGLE_LIMIT) + ")");
    angle[2] = static_cast<float>(yaw);
    return true;
}

bool ModelChoiceAt(const ScriptReader& reader, std::size_t index, ModelChoice& model)
{
    const ScriptReader entry = reader.Element("models", index);
    if (!entry.IsObject())
        return ModelFromJson(reader, "models[" + std::to_string(index) + "]", reader.At("models")[index], model);
    return entry.OnlyKeys({"model", "weight"}) && ModelFromJson(entry, "model", entry.At("model"), model) &&
           entry.OptionalNumber("weight", 0.0f, MAX_WEIGHT, model.weight);
}

// "model": one model, or "models": a list of names, types or {"model", "weight"}.
bool ModelChoices(const ScriptReader& reader, bool required, std::vector<ModelChoice>& models)
{
    if (reader.Has("model") && reader.Has("models"))
        return reader.Fail("models", "give \"model\" or \"models\", not both");
    if (reader.Has("model"))
    {
        ModelChoice model;
        if (!ModelFromJson(reader, "model", reader.At("model"), model))
            return false;
        models.push_back(model);
        return true;
    }
    if (!reader.Has("models"))
        return !required || reader.Fail("model", "is missing: name the model");
    const json& list = reader.At("models");
    if (!list.is_array() || list.empty() || list.size() > MAX_MODEL_CHOICES)
        return reader.Fail("models", "is a list of 1 to " + std::to_string(MAX_MODEL_CHOICES) + " models");
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        ModelChoice model;
        if (!ModelChoiceAt(reader, i, model))
            return false;
        models.push_back(model);
    }
    return true;
}

// The list at `key` (up to MAX_AVOID_ENTRIES entries); null when it is absent.
bool OptionalList(const ScriptReader& reader, std::string_view key, const json*& list)
{
    list = nullptr;
    if (!reader.Has(key))
        return true;
    list = &reader.At(key);
    if (list->is_array() && list->size() <= MAX_AVOID_ENTRIES)
        return true;
    return reader.Fail(key, "is a list of up to " + std::to_string(MAX_AVOID_ENTRIES) + " entries");
}

std::string ElementName(std::string_view key, std::size_t index)
{
    return std::string(key) + "[" + std::to_string(index) + "]";
}

bool AvoidAttributes(const ScriptReader& avoid, AvoidRules& rules)
{
    const json* list = nullptr;
    if (!OptionalList(avoid, "attributes", list))
        return false;
    for (std::size_t i = 0; list != nullptr && i < list->size(); ++i)
    {
        std::uint16_t value = 0;
        if (!AttributeFromJson(avoid, ElementName("attributes", i), (*list)[i], value))
            return false;
        rules.attributes.push_back(value);
    }
    return true;
}

bool AvoidTextures(const ScriptReader& avoid, AvoidRules& rules)
{
    const json* list = nullptr;
    if (!OptionalList(avoid, "textures", list))
        return false;
    for (std::size_t i = 0; list != nullptr && i < list->size(); ++i)
    {
        TileChoice tile;
        if (!TileFromJson(avoid, ElementName("textures", i), (*list)[i], tile))
            return false;
        rules.textures.push_back(tile);
    }
    return true;
}

bool AvoidAreas(const ScriptReader& avoid, AvoidRules& rules)
{
    const json* list = nullptr;
    if (!OptionalList(avoid, "areas", list))
        return false;
    for (std::size_t i = 0; list != nullptr && i < list->size(); ++i)
    {
        Shape area;
        if (!ShapeFrom(avoid.Element("areas", i), area))
            return false;
        rules.areas.push_back(std::move(area));
    }
    return true;
}

bool AvoidAt(const ScriptReader& reader, AvoidRules& rules)
{
    if (!reader.Has("avoid"))
        return true;
    const ScriptReader avoid = reader.Nested("avoid");
    float slope = 0.0f;
    float distance = 0.0f;
    const bool valid = avoid.OnlyKeys({"attributes", "textures", "slope", "objects", "areas"}) &&
                       AvoidAttributes(avoid, rules) && AvoidTextures(avoid, rules) && AvoidAreas(avoid, rules) &&
                       avoid.OptionalNumber("slope", 0.0f, MAX_SLOPE_DEGREES, slope) &&
                       avoid.OptionalNumber("objects", 0.0f, MAX_SPACING, distance);
    if (avoid.Has("slope"))
        rules.maxSlope = slope;
    if (avoid.Has("objects"))
        rules.objectDistance = distance;
    return valid;
}

bool ScatterAmount(const ScriptReader& reader, ScatterEdit& edit)
{
    if (reader.Has("count") == reader.Has("density"))
        return reader.Fail("count", "give \"count\" (objects) or \"density\" (objects per tile), one of them");
    if (reader.Has("count"))
        return reader.Integer("count", 1, MAX_SCATTER_OBJECTS, edit.count);
    return reader.Number("density", std::numeric_limits<float>::min(), MAX_DENSITY, edit.density);
}

bool ScatterAlign(const ScriptReader& reader)
{
    std::string align;
    if (!reader.OptionalText("align", align))
        return false;
    return align.empty() || align == "ground" ||
           reader.Fail("align", "is \"ground\" (objects stand upright on the ground)");
}

bool ScatterMark(const ScriptReader& reader, ScatterEdit& edit)
{
    if (!reader.Has("mark_attribute"))
        return true;
    std::uint16_t value = 0;
    if (!AttributeAt(reader, "mark_attribute", value))
        return false;
    edit.markAttribute = value;
    return true;
}

bool SelectorIds(const ScriptReader& select, ObjectSelector& selector)
{
    if (!select.Has("ids"))
        return true;
    const json& list = select.At("ids");
    if (!list.is_array() || list.empty() || list.size() > MAX_SELECTOR_IDS)
        return select.Fail("ids", "is a list of object ids (the \"index\" map-query and objects.json report)");
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        double number = 0.0;
        double whole = 0.0;
        if (!ScriptReader::ReadNumber(list[i], number) || std::modf(number, &whole) != 0.0 || number < 0 ||
            number > LARGEST_ID)
            return select.Fail(ElementName("ids", i), "is an object id, a whole number from 0");
        selector.ids.push_back(static_cast<int>(number));
    }
    return true;
}

} // namespace

bool SelectorAt(const ScriptReader& reader, const char* key, ObjectSelector& selector)
{
    if (!reader.Has(key))
        return reader.Fail(key, "is missing: say which objects, e.g. {\"model\": \"Tree01\", \"inside\": shape}");
    const ScriptReader select = reader.Nested(key);
    int placedBy = -1;
    const bool valid = select.OnlyKeys({"ids", "model", "models", "inside", "placed_by"}) &&
                       SelectorIds(select, selector) && ModelChoices(select, false, selector.models) &&
                       OptionalShapeAt(select, "inside", selector.inside) &&
                       select.OptionalInteger("placed_by", 0, LARGEST_ID, placedBy);
    if (!valid)
        return false;
    if (select.Has("placed_by"))
        selector.placedBy = placedBy;
    const bool anyCriterion =
        !selector.ids.empty() || !selector.models.empty() || selector.inside.has_value() || selector.placedBy;
    return anyCriterion ||
           select.Fail({}, "needs at least one of \"ids\", \"model\", \"models\", \"inside\", \"placed_by\"");
}

namespace
{

// "by": [dx, dy] in tiles or "to": [x, y] on the map, exactly one of them.
bool MoveTarget(const ScriptReader& reader, ObjectEdit& edit)
{
    if (reader.Has("by") == reader.Has("to"))
        return reader.Fail("by", "give \"by\": [dx, dy] or \"to\": [x, y], one of them");
    if (reader.Has("to"))
    {
        Point to;
        if (!reader.MapPoint("to", to) || !InObjectGrid(reader, "to", to))
            return false;
        edit.to = to;
        return true;
    }
    float by[MOVE_VALUES] = {};
    const json& value = reader.At("by");
    for (std::size_t i = 0; i < MOVE_VALUES; ++i)
    {
        double number = 0.0;
        if (!value.is_array() || value.size() != MOVE_VALUES || !ScriptReader::ReadNumber(value[i], number) ||
            std::fabs(number) > MAX_MOVE)
            return reader.Fail("by", "is [dx, dy] in tiles, each up to " + FormatNumber(MAX_MOVE));
        by[i] = static_cast<float>(number);
    }
    edit.by = Point{by[0], by[1]};
    return true;
}

// "by" (a change) or "to" (the new value) within [min, max], exactly one of them.
bool ChangeOrValue(const ScriptReader& reader, float min, float max, float& value, bool& absolute)
{
    if (reader.Has("by") == reader.Has("to"))
        return reader.Fail("by", "give \"by\" (a change) or \"to\" (the new value), one of them");
    absolute = reader.Has("to");
    return reader.Number(absolute ? "to" : "by", min, max, value);
}

bool RotateOp(const ScriptReader& reader, ObjectEdit& edit)
{
    std::string pivot;
    if (!reader.OnlyKeys({"op", "select", "by", "to", "pivot"}) || !SelectorAt(reader, SELECT_KEY, edit.select) ||
        !ChangeOrValue(reader, -ANGLE_LIMIT, ANGLE_LIMIT, edit.degrees, edit.absolute) ||
        !reader.OptionalText("pivot", pivot))
        return false;
    if (pivot.empty() || pivot == "each")
        return true;
    if (pivot != "center")
        return reader.Fail("pivot", "is \"each\" (every object on its spot) or \"center\" (the group together)");
    if (edit.absolute)
        return reader.Fail("pivot", "a group turns around its centre by an amount (\"by\"), not to a heading");
    edit.pivot = RotatePivot::Center;
    return true;
}
} // namespace

bool PlaceOp(const ScriptReader& reader, PlaceEdit& edit)
{
    return reader.OnlyKeys({"op", "model", "tile", "height", "angle", "scale"}) &&
           ModelFromJson(reader, "model", reader.At("model"), edit.model) && reader.MapPoint("tile", edit.at) &&
           InObjectGrid(reader, "tile", edit.at) && ObjectHeightAt(reader, "height", false, edit.height) &&
           AngleAt(reader, edit.angle) &&
           reader.OptionalNumber("scale", MIN_OBJECT_SCALE, MAX_OBJECT_SCALE, edit.scale);
}

bool ScatterOp(const ScriptReader& reader, ScatterEdit& edit)
{
    edit.minSpacing = DEFAULT_MIN_SPACING;
    edit.yawRange[0] = 0.0f;
    edit.yawRange[1] = FULL_TURN;
    int seed = DEFAULT_SEED;
    const bool valid =
        reader.OnlyKeys({"op", "model", "models", "shape", "count", "density", "min_spacing", "seed", "scale_range",
                         "yaw_range", "align", "avoid", "mark_attribute"}) &&
        ModelChoices(reader, true, edit.models) && ShapeAt(reader, "shape", edit.shape) &&
        ScatterAmount(reader, edit) && reader.OptionalNumber("min_spacing", 0.0f, MAX_SPACING, edit.minSpacing) &&
        reader.OptionalInteger("seed", 0, MAX_SEED, seed) &&
        (!reader.Has("scale_range") ||
         reader.Range("scale_range", MIN_OBJECT_SCALE, MAX_OBJECT_SCALE, edit.scaleRange)) &&
        (!reader.Has("yaw_range") || reader.Range("yaw_range", -ANGLE_LIMIT, ANGLE_LIMIT, edit.yawRange)) &&
        ScatterAlign(reader) && AvoidAt(reader, edit.avoid) && ScatterMark(reader, edit);
    edit.seed = static_cast<std::uint64_t>(seed);
    return valid;
}

bool ObjectEditOp(const ScriptReader& reader, OpKind kind, ObjectEdit& edit)
{
    switch (kind)
    {
    case OpKind::ObjectMove:
        return reader.OnlyKeys({"op", "select", "by", "to", "height"}) && SelectorAt(reader, SELECT_KEY, edit.select) &&
               MoveTarget(reader, edit) && ObjectHeightAt(reader, "height", true, edit.height);
    case OpKind::ObjectRotate:
        return RotateOp(reader, edit);
    case OpKind::ObjectScale:
        return reader.OnlyKeys({"op", "select", "by", "to"}) && SelectorAt(reader, SELECT_KEY, edit.select) &&
               ChangeOrValue(reader, MIN_OBJECT_SCALE, MAX_OBJECT_SCALE, edit.factor, edit.absolute);
    case OpKind::ObjectDelete:
    case OpKind::ObjectDropToGround:
        return reader.OnlyKeys({"op", "select"}) && SelectorAt(reader, SELECT_KEY, edit.select);
    default:
        return reader.Fail("op", "is not an object edit");
    }
}
} // namespace Editor::MapScript::Parse

#endif // _EDITOR
