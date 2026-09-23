#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorShortcuts.h"

#include "UI/Legacy/UIControls.h" // CUITextInputBox: a game text box (chat) has focus

#include "imgui.h"

namespace Editor::Shortcuts
{
bool IsTypingText()
{
    return ImGui::GetIO().WantTextInput || CUITextInputBox::IsAnyInputBoxFocused();
}

bool UndoPressed()
{
    return !IsTypingText() && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z);
}

bool RedoPressed()
{
    if (IsTypingText())
        return false;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
        return true;
    return !ImGui::GetIO().ConfigMacOSXBehaviors && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y);
}

bool DeletePressed()
{
    if (IsTypingText())
        return false;
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        return true;
    return ImGui::GetIO().ConfigMacOSXBehaviors && ImGui::IsKeyPressed(ImGuiKey_Backspace, false);
}

bool DuplicatePressed()
{
    return !IsTypingText() && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D);
}

bool EscapePressed()
{
    if (IsTypingText() || IsPopupOpen())
        return false;
    return ImGui::IsKeyPressed(ImGuiKey_Escape, false);
}

bool IsPopupOpen()
{
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);
}

namespace
{
// -1 for `modifiers` + [, +1 for `modifiers` + ], 0 otherwise.
int BracketStep(ImGuiKeyChord modifiers)
{
    if (IsTypingText())
        return 0;
    if (ImGui::IsKeyChordPressed(modifiers | ImGuiKey_LeftBracket))
        return -1;
    if (ImGui::IsKeyChordPressed(modifiers | ImGuiKey_RightBracket))
        return 1;
    return 0;
}
} // namespace

int BrushRadiusStep()
{
    return BracketStep(ImGuiMod_None);
}

int BrushStrengthStep()
{
    return BracketStep(ImGuiMod_Shift);
}
} // namespace Editor::Shortcuts

#endif // _EDITOR
