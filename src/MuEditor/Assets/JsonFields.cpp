#include "JsonFields.h"

#ifdef _EDITOR

#include "EditorText.h"

#include <fstream>
#include <system_error>

namespace Editor::Assets::Json
{
namespace
{
constexpr const char* TEMP_SUFFIX = ".tmp";
} // namespace

std::string Text(const json& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::optional<std::string> OptionalText(const json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
        return std::nullopt;
    return it->get<std::string>();
}

std::vector<std::string> TextList(const json& object, const char* key)
{
    std::vector<std::string> values;
    for (const json& value : Array(object, key))
    {
        if (value.is_string())
            values.push_back(value.get<std::string>());
    }
    return values;
}

int Int(const json& object, const char* key, int fallback)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_number_integer() ? it->get<int>() : fallback;
}

double Number(const json& object, const char* key, double fallback)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_number() ? it->get<double>() : fallback;
}

bool Bool(const json& object, const char* key, bool fallback)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

const json& Member(const json& object, const char* key)
{
    static const json empty = json::object();
    const auto it = object.find(key);
    return it != object.end() && it->is_object() ? *it : empty;
}

const json& Array(const json& object, const char* key)
{
    static const json empty = json::array();
    const auto it = object.find(key);
    return it != object.end() && it->is_array() ? *it : empty;
}
bool ReplaceFileText(const std::filesystem::path& file, const std::string& text, std::string& error)
{
    std::filesystem::path temp = file;
    temp += TEMP_SUFFIX;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    {
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        stream << text;
        if (!stream)
        {
            error = "cannot write " + Editor::Text::PathToUtf8(temp);
            return false;
        }
    }
    std::filesystem::rename(temp, file, ec);
    if (ec)
    {
        error = "cannot replace " + Editor::Text::PathToUtf8(file) + ": " + ec.message();
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}
} // namespace Editor::Assets::Json

#endif // _EDITOR
