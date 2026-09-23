#pragma once

#ifdef _EDITOR

// The Map Editor's keyboard shortcuts. None of them fires while the user types, in
// an ImGui field (the Pos/Angle boxes, the console) or in one of the game's own
// text boxes (chat). ImGui reports the Mac's Cmd key as its Ctrl modifier, so every
// "Ctrl" below is Cmd on a Mac.
namespace Editor::Shortcuts
{
bool IsTypingText();
// Ctrl+Z.
bool UndoPressed();
// Ctrl+Shift+Z, and Ctrl+Y on Windows and Linux.
bool RedoPressed();
// Delete, and on a Mac also Backspace (a Mac laptop has no Delete key).
bool DeletePressed();
// Ctrl+D.
bool DuplicatePressed();
// Escape, while no popup is open: with a menu, a combo's list or a picker open, Esc
// closes that instead (CMuEditorCore) and must not also clear the selection.
bool EscapePressed();
// An ImGui popup is open: a menu, a combo's list, the colour picker or a dialog.
bool IsPopupOpen();
// The round brushes' size: -1 for [, +1 for ], 0 otherwise (no modifier). The keys
// are taken by position, so on a German keyboard they are the two right of P.
int BrushRadiusStep();
// The brushes' strength: -1 for Shift+[, +1 for Shift+], 0 otherwise.
int BrushStrengthStep();
} // namespace Editor::Shortcuts

#endif // _EDITOR
