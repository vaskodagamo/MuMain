#include "JsonFields.h"

#ifdef _EDITOR

namespace Editor::Assets::Json
{
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
} // namespace Editor::Assets::Json

#endif // _EDITOR
