#pragma once

#ifdef _EDITOR

#include "EditScript.h"

#include "json.hpp"

#include <string>

namespace Editor::MapScript
{
// A script holds at most this many ops and a label of at most this many characters.
constexpr std::size_t MAX_OPS = 256;
constexpr std::size_t MAX_LABEL_CHARS = 120;

// Reads an edit script from its JSON text or document. Every field is checked against
// what its op accepts (types, ranges, known keys, points on the map); the first problem
// comes back in `error` with the field's path ("ops[2].shape.radius: ..."), and then
// nothing of the script is used. Names of models and textures are checked against the
// loaded map later (ResolveNames).
bool ParseScript(const std::string& text, EditScript& script, std::string& error);
bool ParseScript(const nlohmann::json& document, EditScript& script, std::string& error);
} // namespace Editor::MapScript

#endif // _EDITOR
