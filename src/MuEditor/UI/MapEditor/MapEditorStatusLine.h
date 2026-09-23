#pragma once

#ifdef _EDITOR

#include <string>

struct ImVec4;

// The result line under a Map Editor tab's buttons (last save, import, undo...).
namespace Editor::StatusLine
{
// Draws `text` in the status colour, wrapped to the panel width. Save results
// span several lines (one per absolute path written). Draws nothing when empty.
void Render(const std::string& text);

// The yellow the Map Editor and its dialogs draw warnings in.
const ImVec4& WarningColor();
} // namespace Editor::StatusLine

#endif // _EDITOR
