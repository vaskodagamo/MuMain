#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "ScriptReader.h"

// The parts of edit-script ops that several ops share: shapes, height targets, texture
// slots and attribute values.
namespace Editor::MapScript::Parse
{
// Limits a script is checked against before anything runs.
constexpr std::size_t MAX_SHAPE_POINTS = 256;
constexpr float MAX_RADIUS = 256.0f;
constexpr float MIN_SIZE = 0.1f; // the smallest radius or path width, in tiles
constexpr float MAX_PATH_WIDTH = 64.0f;
constexpr float MAX_FALLOFF = 128.0f;
// Heights and height changes, in world units (a map stores 0 to 382.5).
constexpr float MAX_HEIGHT_VALUE = 10000.0f;
// The seed of the random ops (noise, scatter) when the script gives none.
constexpr int DEFAULT_SEED = 1;

// `key` as a shape: {"type": "circle", "center": [x, y], "radius": r}, {"type": "rect",
// "rect": [x0, y0, x1, y1]}, {"type": "polygon", "points": [...]} or {"type": "path",
// "points": [...], "width": w}, each with an optional "falloff".
bool ShapeAt(const ScriptReader& reader, std::string_view key, Shape& shape);
// The same for the object `shapeReader` reads (an element of a list of shapes).
bool ShapeFrom(const ScriptReader& shapeReader, Shape& shape);
bool OptionalShapeAt(const ScriptReader& reader, std::string_view key, std::optional<Shape>& shape);

// `key` as a height target: a number (absolute), "average", "min", "max", "center"
// ("ground" too when `allowGround`), or {"of": <one of those>, "plus": n}.
bool HeightTargetAt(const ScriptReader& reader, std::string_view key, bool allowGround, HeightTarget& target);

// `key` as a texture slot number (0 to 29) or name.
bool TileAt(const ScriptReader& reader, std::string_view key, TileChoice& tile);
bool TileFromJson(const ScriptReader& reader, const std::string& where, const nlohmann::json& value, TileChoice& tile);

// `key` as a clean walkability value: a name or its number.
bool AttributeAt(const ScriptReader& reader, std::string_view key, std::uint16_t& value);
bool AttributeFromJson(const ScriptReader& reader, const std::string& where, const nlohmann::json& value,
                       std::uint16_t& attribute);

// A model given as a name (string) or a type (whole number).
bool ModelFromJson(const ScriptReader& reader, const std::string& where, const nlohmann::json& value,
                   ModelChoice& model);
} // namespace Editor::MapScript::Parse

#endif // _EDITOR
