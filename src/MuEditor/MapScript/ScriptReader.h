#pragma once

#ifdef _EDITOR

#include "ScriptShape.h" // Point

#include "json.hpp"

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace Editor::MapScript
{
// Reads one JSON object of an edit script and names the exact field in every error,
// e.g. "ops[2].shape.radius: is a number from 0.1 to 256". Readers of nested objects
// share the error of the reader they came from; the first error wins.
class ScriptReader
{
public:
    ScriptReader(const nlohmann::json& value, std::string path, std::string& error);

    const std::string& Path() const
    {
        return m_path;
    }
    bool IsObject() const
    {
        return m_value.is_object();
    }
    bool Has(std::string_view key) const;
    const nlohmann::json& At(std::string_view key) const;
    // "<path>.<key>".
    std::string PathOf(std::string_view key) const;

    // Records "<path>.<key>: <message>" (or "<path>: <message>" for an empty key) and
    // returns false.
    bool Fail(std::string_view key, const std::string& message) const;

    // False, naming the first field it does not know (a misspelt "radus"), when the
    // object has keys outside `known`. Also false when the value is not an object.
    bool OnlyKeys(std::initializer_list<std::string_view> known) const;

    // Required fields: false with an error when missing or out of range.
    bool Number(std::string_view key, float min, float max, float& out) const;
    bool Integer(std::string_view key, int min, int max, int& out) const;
    bool Text(std::string_view key, std::string& out) const;
    // A point [x, y] in tiles on the map (0 to 256).
    bool MapPoint(std::string_view key, Point& out) const;
    // A list of `min` to `max` map points.
    bool MapPoints(std::string_view key, std::size_t min, std::size_t max, std::vector<Point>& out) const;
    // A list [a, b] of numbers within [min, max] with a <= b.
    bool Range(std::string_view key, float min, float max, float (&out)[2]) const;
    // Three numbers within [min, max].
    bool Triple(std::string_view key, float min, float max, float (&out)[3]) const;

    // Optional fields: `out` keeps its value when the key is absent.
    bool OptionalNumber(std::string_view key, float min, float max, float& out) const;
    bool OptionalInteger(std::string_view key, int min, int max, int& out) const;
    bool OptionalBool(std::string_view key, bool& out) const;
    bool OptionalText(std::string_view key, std::string& out) const;

    // A number read as a plain JSON value (e.g. one element of a list).
    static bool ReadNumber(const nlohmann::json& value, double& out);

    // A reader for the object at `key` (the value may be anything; OnlyKeys checks it).
    ScriptReader Nested(std::string_view key) const;
    // A reader for element `index` of the list at `key`.
    ScriptReader Element(std::string_view key, std::size_t index) const;

private:
    static const nlohmann::json& Missing();

    const nlohmann::json& m_value;
    std::string m_path;
    std::string& m_error;
};

// "0.1", "256": numbers in error messages without trailing zeros.
std::string FormatNumber(double value);
} // namespace Editor::MapScript

#endif // _EDITOR
