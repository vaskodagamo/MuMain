#include "ScriptReader.h"

#ifdef _EDITOR

#include "MapInspect/TerrainView.h" // MAP_TILES

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Editor::MapScript
{
namespace
{
using nlohmann::json;

constexpr float MAP_EXTENT = static_cast<float>(Editor::MapInspect::MAP_TILES);
constexpr std::size_t POINT_VALUES = 2;
constexpr std::size_t TRIPLE_VALUES = 3;
constexpr std::size_t RANGE_VALUES = 2;
constexpr int NUMBER_TEXT_CHARS = 32;

std::string RangeText(double min, double max)
{
    return "a number from " + FormatNumber(min) + " to " + FormatNumber(max);
}

// `count` numbers within [min, max] from a JSON list.
bool NumberList(const json& value, std::size_t count, float min, float max, float* out)
{
    if (!value.is_array() || value.size() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        double number = 0.0;
        if (!ScriptReader::ReadNumber(value[i], number) || number < min || number > max)
            return false;
        out[i] = static_cast<float>(number);
    }
    return true;
}
} // namespace

std::string FormatNumber(double value)
{
    char text[NUMBER_TEXT_CHARS];
    std::snprintf(text, sizeof(text), "%g", value);
    return text;
}

ScriptReader::ScriptReader(const json& value, std::string path, std::string& error)
    : m_value(value), m_path(std::move(path)), m_error(error)
{
}

const json& ScriptReader::Missing()
{
    static const json missing;
    return missing;
}

bool ScriptReader::Has(std::string_view key) const
{
    return m_value.is_object() && m_value.contains(key) && !m_value.at(key).is_null();
}

const json& ScriptReader::At(std::string_view key) const
{
    return Has(key) ? m_value.at(key) : Missing();
}

std::string ScriptReader::PathOf(std::string_view key) const
{
    if (key.empty())
        return m_path;
    return m_path.empty() ? std::string(key) : m_path + "." + std::string(key);
}

bool ScriptReader::Fail(std::string_view key, const std::string& message) const
{
    if (m_error.empty())
    {
        const std::string where = PathOf(key);
        m_error = where.empty() ? message : where + ": " + message;
    }
    return false;
}

bool ScriptReader::OnlyKeys(std::initializer_list<std::string_view> known) const
{
    if (!m_value.is_object())
        return Fail({}, "is not an object");
    for (const auto& [key, value] : m_value.items())
    {
        if (std::find(known.begin(), known.end(), key) != known.end())
            continue;
        std::string list;
        for (std::string_view name : known)
            list += (list.empty() ? "" : ", ") + std::string(name);
        return Fail(key, "is not a field here; known: " + list);
    }
    return true;
}

bool ScriptReader::ReadNumber(const json& value, double& out)
{
    if (!value.is_number())
        return false;
    out = value.get<double>();
    return std::isfinite(out);
}

bool ScriptReader::Number(std::string_view key, float min, float max, float& out) const
{
    double number = 0.0;
    if (!ReadNumber(At(key), number) || number < min || number > max)
        return Fail(key, "is " + RangeText(min, max));
    out = static_cast<float>(number);
    return true;
}

bool ScriptReader::Integer(std::string_view key, int min, int max, int& out) const
{
    double number = 0.0;
    double whole = 0.0;
    if (!ReadNumber(At(key), number) || std::modf(number, &whole) != 0.0 || number < min || number > max)
        return Fail(key, "is a whole number from " + std::to_string(min) + " to " + std::to_string(max));
    out = static_cast<int>(number);
    return true;
}

bool ScriptReader::Text(std::string_view key, std::string& out) const
{
    const json& value = At(key);
    if (!value.is_string() || value.get<std::string>().empty())
        return Fail(key, "is a non-empty string");
    out = value.get<std::string>();
    return true;
}

bool ScriptReader::MapPoint(std::string_view key, Point& out) const
{
    float values[POINT_VALUES] = {};
    if (!NumberList(At(key), POINT_VALUES, 0.0f, MAP_EXTENT, values))
        return Fail(key, "is a point [x, y] in tiles, each from 0 to " + FormatNumber(MAP_EXTENT));
    out = Point{values[0], values[1]};
    return true;
}

bool ScriptReader::MapPoints(std::string_view key, std::size_t min, std::size_t max, std::vector<Point>& out) const
{
    const json& value = At(key);
    if (!value.is_array() || value.size() < min || value.size() > max)
        return Fail(key, "is a list of " + std::to_string(min) + " to " + std::to_string(max) + " points [x, y]");
    out.clear();
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        float values[POINT_VALUES] = {};
        if (!NumberList(value[i], POINT_VALUES, 0.0f, MAP_EXTENT, values))
            return Fail(std::string(key) + "[" + std::to_string(i) + "]",
                        "is a point [x, y] in tiles, each from 0 to " + FormatNumber(MAP_EXTENT));
        out.push_back(Point{values[0], values[1]});
    }
    return true;
}

bool ScriptReader::Range(std::string_view key, float min, float max, float (&out)[2]) const
{
    float values[RANGE_VALUES] = {};
    if (!NumberList(At(key), RANGE_VALUES, min, max, values) || values[0] > values[1])
        return Fail(key, "is [low, high] with " + RangeText(min, max) + " each, low <= high");
    out[0] = values[0];
    out[1] = values[1];
    return true;
}

bool ScriptReader::Triple(std::string_view key, float min, float max, float (&out)[3]) const
{
    if (!NumberList(At(key), TRIPLE_VALUES, min, max, out))
        return Fail(key, "is a list of three numbers, each " + RangeText(min, max));
    return true;
}

bool ScriptReader::OptionalNumber(std::string_view key, float min, float max, float& out) const
{
    return !Has(key) || Number(key, min, max, out);
}

bool ScriptReader::OptionalInteger(std::string_view key, int min, int max, int& out) const
{
    return !Has(key) || Integer(key, min, max, out);
}

bool ScriptReader::OptionalBool(std::string_view key, bool& out) const
{
    if (!Has(key))
        return true;
    const json& value = At(key);
    if (!value.is_boolean())
        return Fail(key, "is true or false");
    out = value.get<bool>();
    return true;
}

bool ScriptReader::OptionalText(std::string_view key, std::string& out) const
{
    return !Has(key) || Text(key, out);
}

ScriptReader ScriptReader::Nested(std::string_view key) const
{
    return ScriptReader(At(key), PathOf(key), m_error);
}

ScriptReader ScriptReader::Element(std::string_view key, std::size_t index) const
{
    const json& list = At(key);
    const json& value = (list.is_array() && index < list.size()) ? list[index] : Missing();
    return ScriptReader(value, PathOf(key) + "[" + std::to_string(index) + "]", m_error);
}
} // namespace Editor::MapScript

#endif // _EDITOR
