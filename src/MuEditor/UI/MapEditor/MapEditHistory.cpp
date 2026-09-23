#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditHistory.h"

#include "MapEditorShortcuts.h"
#include "MapEditorStatusLine.h"

#include "UI/Console/MuEditorConsoleUI.h"

#include "imgui.h"

#include <utility>

using Editor::Editing::StepResult;

namespace
{
constexpr const char* HISTORY_MISMATCH =
    "The undo history no longer matched the map (an object it names is gone) and was cleared.";

const char* ShortcutText(bool undo)
{
    const bool mac = ImGui::GetIO().ConfigMacOSXBehaviors;
    if (undo)
        return mac ? "Cmd+Z" : "Ctrl+Z";
    return mac ? "Cmd+Shift+Z" : "Ctrl+Shift+Z or Ctrl+Y";
}
} // namespace

CMapEditHistory& CMapEditHistory::GetInstance()
{
    static CMapEditHistory instance;
    return instance;
}

void CMapEditHistory::Push(std::unique_ptr<Editor::Editing::EditCommand> command)
{
    if (!command)
        return;
    m_failure.clear();
    m_stack.Push(std::move(command));
}

void CMapEditHistory::RenderButton(bool undo, bool enabled, bool& clicked)
{
    const std::string& next = undo ? m_stack.UndoLabel() : m_stack.RedoLabel();
    const char* verb = undo ? "Undo" : "Redo";
    const std::string label = next.empty() ? std::string(verb) + (undo ? "###undo" : "###redo")
                                           : std::string(verb) + ": " + next + (undo ? "###undo" : "###redo");
    ImGui::BeginDisabled(!enabled);
    clicked = ImGui::Button(label.c_str());
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s (%s)", verb, ShortcutText(undo));
}

HistoryStep CMapEditHistory::Render(bool editInProgress)
{
    bool undoClicked = false;
    bool redoClicked = false;
    RenderButton(true, !editInProgress && m_stack.CanUndo(), undoClicked);
    ImGui::SameLine();
    RenderButton(false, !editInProgress && m_stack.CanRedo(), redoClicked);
    if (!m_failure.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
        ImGui::TextWrapped("%s", m_failure.c_str());
        ImGui::PopStyleColor();
    }

    if (editInProgress)
        return HistoryStep::None;
    if ((undoClicked || Editor::Shortcuts::UndoPressed()) && Undo())
        return HistoryStep::Undone;
    if ((redoClicked || Editor::Shortcuts::RedoPressed()) && Redo())
        return HistoryStep::Redone;
    return HistoryStep::None;
}

bool CMapEditHistory::Step(bool undo)
{
    const std::string label = undo ? m_stack.UndoLabel() : m_stack.RedoLabel();
    const StepResult result = undo ? m_stack.Undo() : m_stack.Redo();
    if (result == StepResult::Nothing)
        return false;
    if (result == StepResult::Failed)
    {
        m_failure = HISTORY_MISMATCH;
        g_MuEditorConsoleUI.LogEditor(std::string("[MapEditor] ") + HISTORY_MISMATCH);
        return true;
    }
    m_failure.clear();
    g_MuEditorConsoleUI.LogEditor(std::string("[MapEditor] ") + (undo ? "Undo: " : "Redo: ") + label);
    return true;
}

bool CMapEditHistory::Undo()
{
    return Step(true);
}

bool CMapEditHistory::Redo()
{
    return Step(false);
}

void CMapEditHistory::Forget()
{
    m_stack.Clear();
    m_objects.Reset();
    m_failure.clear();
}

#endif // _EDITOR
