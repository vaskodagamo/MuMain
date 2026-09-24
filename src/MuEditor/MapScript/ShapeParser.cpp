#include "ShapeParser.h"

#ifdef _EDITOR

#include "AttributeRules.h"
#include "MapInspect/TerrainView.h" // MAP_TILES
#include "MapInspect/TilePalette.h" // TILE_SLOT_COUNT

#include <algorithm>
#include <array>
#include <cmath>

namespace Editor::MapScript::Parse
{
namespace
{
using nlohmann::json;

constexpr std::size_t RECT_VALUES = 4;
constexpr int LAST_TILE = Editor::MapInspect::MAP_TILES - 1;
constexpr int LAST_TILE_SLOT = Editor::MapInspect::TILE_SLOT_COUNT - 1;
constexpr int LARGEST_MODEL_TYPE = 65535;
constexpr std::size_t POLYGON_MIN_POINTS = 3;
constexpr std::size_t PATH_MIN_POINTS = 2;

struct NamedReference
{
    const char* name;
    HeightReference reference;
};

constexpr std::array<NamedReference, 5> REFERENCES = {{
    {"average", HeightReference::Average},
    {"min", HeightReference::Lowest},
    {"max", HeightReference::Highest},
    {"center", HeightReference::Center},
    {"ground", HeightReference::Ground},
}};

bool IsWhole(double number)
{
    double whole = 0.0;
    return std::modf(number, &whole) == 0.0;
}

bool RectAt(const ScriptReader& reader, CellRect& tiles)
{
    const json& value = reader.At("rect");
    const std::string message =
        "is [x0, y0, x1, y1]: whole tiles from 0 to " + std::to_string(LAST_TILE) + ", both corners included";
    if (!value.is_array() || value.size() != RECT_VALUES)
        return reader.Fail("rect", message);
    int corners[RECT_VALUES] = {};
    for (std::size_t i = 0; i < RECT_VALUES; ++i)
    {
        double number = 0.0;
        if (!ScriptReader::ReadNumber(value[i], number) || !IsWhole(number) || number < 0 || number > LAST_TILE)
            return reader.Fail("rect", message);
        corners[i] = static_cast<int>(number);
    }
    tiles = CellRect{std::min(corners[0], corners[2]), std::min(corners[1], corners[3]),
                     std::max(corners[0], corners[2]), std::max(corners[1], corners[3])};
    return true;
}

bool ShapeBody(const ScriptReader& shapeReader, const std::string& type, Shape& shape)
{
    if (type == "circle")
    {
        shape.kind = ShapeKind::Circle;
        return shapeReader.OnlyKeys({"type", "center", "radius", "falloff"}) &&
               shapeReader.MapPoint("center", shape.center) &&
               shapeReader.Number("radius", MIN_SIZE, MAX_RADIUS, shape.radius);
    }
    if (type == "rect")
    {
        shape.kind = ShapeKind::Rect;
        return shapeReader.OnlyKeys({"type", "rect", "falloff"}) && RectAt(shapeReader, shape.tiles);
    }
    if (type == "polygon")
    {
        shape.kind = ShapeKind::Polygon;
        return shapeReader.OnlyKeys({"type", "points", "falloff"}) &&
               shapeReader.MapPoints("points", POLYGON_MIN_POINTS, MAX_SHAPE_POINTS, shape.points);
    }
    if (type == "path")
    {
        shape.kind = ShapeKind::Path;
        return shapeReader.OnlyKeys({"type", "points", "width", "falloff"}) &&
               shapeReader.MapPoints("points", PATH_MIN_POINTS, MAX_SHAPE_POINTS, shape.points) &&
               shapeReader.Number("width", MIN_SIZE, MAX_PATH_WIDTH, shape.width);
    }
    return shapeReader.Fail("type", "is \"circle\", \"rect\", \"polygon\" or \"path\"");
}

bool ReferenceFromName(const std::string& name, bool allowGround, HeightReference& reference)
{
    for (const NamedReference& named : REFERENCES)
    {
        if (name != named.name)
            continue;
        if (named.reference == HeightReference::Ground && !allowGround)
            return false;
        reference = named.reference;
        return true;
    }
    return false;
}

std::string ReferenceNames(bool allowGround)
{
    return allowGround ? "\"average\", \"min\", \"max\", \"center\" or \"ground\""
                       : "\"average\", \"min\", \"max\" or \"center\"";
}
} // namespace

bool ShapeAt(const ScriptReader& reader, std::string_view key, Shape& shape)
{
    if (!reader.Has(key))
        return reader.Fail(key, "is missing: give the shape the op acts on");
    return ShapeFrom(reader.Nested(key), shape);
}

bool ShapeFrom(const ScriptReader& shapeReader, Shape& shape)
{
    if (!shapeReader.IsObject())
        return shapeReader.Fail({}, "is a shape such as {\"type\": \"circle\", \"center\": [x, y], \"radius\": r}");
    std::string type;
    if (!shapeReader.Text("type", type) || !ShapeBody(shapeReader, type, shape))
        return false;
    if (!shapeReader.Has("falloff"))
        return true;
    float falloff = 0.0f;
    if (!shapeReader.Number("falloff", 0.0f, MAX_FALLOFF, falloff))
        return false;
    shape.falloff = falloff;
    return true;
}

bool OptionalShapeAt(const ScriptReader& reader, std::string_view key, std::optional<Shape>& shape)
{
    if (!reader.Has(key))
        return true;
    Shape parsed;
    if (!ShapeAt(reader, key, parsed))
        return false;
    shape = std::move(parsed);
    return true;
}

bool HeightTargetAt(const ScriptReader& reader, std::string_view key, bool allowGround, HeightTarget& target)
{
    const json& value = reader.At(key);
    double number = 0.0;
    if (ScriptReader::ReadNumber(value, number))
    {
        if (number < 0.0 || number > MAX_HEIGHT_VALUE)
            return reader.Fail(key, "is a height from 0 to " + FormatNumber(MAX_HEIGHT_VALUE));
        target = HeightTarget{HeightReference::Absolute, static_cast<float>(number)};
        return true;
    }
    const std::string expected = "is a height (a number), " + ReferenceNames(allowGround) +
                                 ", or {\"of\": " + ReferenceNames(allowGround) + ", \"plus\": n}";
    if (value.is_string())
    {
        if (!ReferenceFromName(value.get<std::string>(), allowGround, target.reference))
            return reader.Fail(key, expected);
        target.value = 0.0f;
        return true;
    }
    if (!value.is_object())
        return reader.Fail(key, expected);
    const ScriptReader nested = reader.Nested(key);
    std::string reference;
    if (!nested.OnlyKeys({"of", "plus"}) || !nested.Text("of", reference))
        return false;
    if (!ReferenceFromName(reference, allowGround, target.reference))
        return nested.Fail("of", "is " + ReferenceNames(allowGround));
    target.value = 0.0f;
    return nested.OptionalNumber("plus", -MAX_HEIGHT_VALUE, MAX_HEIGHT_VALUE, target.value);
}

bool TileFromJson(const ScriptReader& reader, const std::string& where, const json& value, TileChoice& tile)
{
    double number = 0.0;
    if (ScriptReader::ReadNumber(value, number))
    {
        if (!IsWhole(number) || number < 0 || number > LAST_TILE_SLOT)
            return reader.Fail(where, "is a texture slot from 0 to " + std::to_string(LAST_TILE_SLOT) + " or a name");
        tile = TileChoice{static_cast<int>(number), {}};
        return true;
    }
    if (!value.is_string() || value.get<std::string>().empty())
        return reader.Fail(where, "is a texture slot (0 to " + std::to_string(LAST_TILE_SLOT) +
                                      ") or a texture name such as \"TileGround01\"");
    tile = TileChoice{-1, value.get<std::string>()};
    return true;
}

bool TileAt(const ScriptReader& reader, std::string_view key, TileChoice& tile)
{
    return TileFromJson(reader, std::string(key), reader.At(key), tile);
}

bool AttributeFromJson(const ScriptReader& reader, const std::string& where, const json& value,
                       std::uint16_t& attribute)
{
    const std::string expected = std::string("is one of ") + Attributes::KnownNames();
    double number = 0.0;
    if (ScriptReader::ReadNumber(value, number))
    {
        if (!Attributes::IsCleanValue(number))
            return reader.Fail(where, expected + " (clean single values only, see MAP_EDITOR.md gotcha 12)");
        attribute = static_cast<std::uint16_t>(number);
        return true;
    }
    if (!value.is_string() || !Attributes::FromName(value.get<std::string>(), attribute))
        return reader.Fail(where, expected);
    return true;
}

bool AttributeAt(const ScriptReader& reader, std::string_view key, std::uint16_t& value)
{
    return AttributeFromJson(reader, std::string(key), reader.At(key), value);
}

bool ModelFromJson(const ScriptReader& reader, const std::string& where, const json& value, ModelChoice& model)
{
    double number = 0.0;
    if (ScriptReader::ReadNumber(value, number))
    {
        if (!IsWhole(number) || number < 0 || number > LARGEST_MODEL_TYPE)
            return reader.Fail(where, "is a model name or a model type (a whole number)");
        model.type = static_cast<int>(number);
        model.name.clear();
        return true;
    }
    if (!value.is_string() || value.get<std::string>().empty())
        return reader.Fail(where, "is a model name (as map-query lists it, e.g. \"Tree01\") or a model type");
    model.name = value.get<std::string>();
    model.type = -1;
    return true;
}
} // namespace Editor::MapScript::Parse

#endif // _EDITOR
