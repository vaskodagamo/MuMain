#pragma once

#ifdef _EDITOR

#include <json.hpp>

#include <filesystem>
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

// Replaces `file` with `text`: writes a file next to it first and renames that
// over it, so a failed write never leaves half a file (the owner files under
// assets-work: client-review.json, a request's owner-decision.json).
bool ReplaceFileText(const std::filesystem::path& file, const std::string& text, std::string& error);
} // namespace Editor::Assets::Json

#endif // _EDITOR
