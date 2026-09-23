#pragma once

#ifdef _EDITOR

#include <json.hpp>

#include <optional>
#include <string>
#include <vector>

// Tolerant readers for the catalogs and owner files under assets-work: a field
// that is missing or of another type reads as empty (or the fallback) instead of
// throwing, so an older or hand-edited file still loads.
namespace Editor::Assets::Json
{
using nlohmann::json;

std::string Text(const json& object, const char* key);
std::optional<std::string> OptionalText(const json& object, const char* key);
// The string entries of an array field.
std::vector<std::string> TextList(const json& object, const char* key);
int Int(const json& object, const char* key, int fallback);
bool Bool(const json& object, const char* key, bool fallback);
// An object field, or an empty object.
const json& Member(const json& object, const char* key);
// An array field, or an empty array.
const json& Array(const json& object, const char* key);
} // namespace Editor::Assets::Json

#endif // _EDITOR
