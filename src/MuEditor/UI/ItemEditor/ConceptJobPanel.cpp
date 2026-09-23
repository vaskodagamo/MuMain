#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptJobPanel.h"

#include "ConceptJob.h"
#include "ConceptPlanView.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h"
#include "Core/MuEditorCore.h"
#include "UI/MapEditor/MapEditorStatusLine.h"

#include "imgui.h"

using Editor::Concepts::ConceptJobState;
using Editor::Concepts::FormatDollars;
using Editor::Concepts::JobItem;
using Editor::Concepts::JobItemState;
using Editor::Concepts::JobPhase;

namespace Editor::ItemEditor::ConceptJobPanel
{
namespace
{
constexpr const char* WINDOW_TITLE = "Concepts job";
constexpr float WINDOW_WIDTH = 520.0f;
constexpr float WINDOW_HEIGHT = 360.0f;
constexpr float WINDOW_MARGIN = 24.0f;
constexpr float LOG_ROWS = 6.0f;
constexpr int ITEM_TABLE_COLUMNS = 3; // item, state, message
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 OK_COLOR{0.45f, 0.85f, 0.45f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};
constexpr ImVec4 BUSY_COLOR{0.55f, 0.75f, 1.0f, 1.0f};

float Scaled(float pixels)
{
    return pixels * g_MuEditorCore.GetUIScale();
}

ImVec4 StateColor(JobItemState state)
{
    switch (state)
    {
    case JobItemState::Done:
        return OK_COLOR;
    case JobItemState::Failed:
        return ERROR_COLOR;
    case JobItemState::Generating:
    case JobItemState::Retrying:
        return BUSY_COLOR;
    default:
        return NOTE_COLOR;
    }
}

std::string StateText(const JobItem& item, double now)
{
    if (item.state == JobItemState::Retrying)
    {
        const int seconds = static_cast<int>(Editor::Concepts::RetrySecondsLeft(item, now) + 0.5);
        return seconds > 0 ? "retry in " + std::to_string(seconds) + " s" : std::string("retrying");
    }
    std::string text = Editor::Concepts::JobItemStateLabel(item.state);
    if (item.state == JobItemState::Done && item.images > 0)
        text += " (" + std::to_string(item.images) + (item.images == 1 ? " image)" : " images)");
    return text;
}

void RenderItems(const ConceptJobState& state)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
    if (!ImGui::BeginTable("ConceptJobItems", ITEM_TABLE_COLUMNS, flags))
        return;
    ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableHeadersRow();
    const double now = g_ConceptJob.Now();
    for (const JobItem& item : state.items)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (item.name.empty() || item.name == item.key)
            ImGui::TextUnformatted(item.key.c_str());
        else
            ImGui::Text("%s (%s)", item.name.c_str(), item.key.c_str());
        ImGui::TableNextColumn();
        ImGui::TextColored(StateColor(item.state), "%s", StateText(item, now).c_str());
        ImGui::TableNextColumn();
        const bool showMessage = item.state == JobItemState::Failed || item.state == JobItemState::Retrying;
        if (showMessage && !item.message.empty())
            ImGui::TextWrapped("%s", item.message.c_str());
    }
    ImGui::EndTable();
}

void RenderCost(const ConceptJobState& state)
{
    if (g_ConceptJob.JobKind() == CConceptJob::Kind::References)
    {
        if (Editor::Concepts::IsFinal(state.phase))
            ImGui::Text("References: %d rendered, %d already current.", state.rendered, state.cached);
        return;
    }
    ImGui::Text("Estimated: %s", FormatDollars(state.estimated.total).c_str());
    if (!state.actual)
        return;
    ImGui::SameLine();
    ImGui::TextColored(OK_COLOR, "  actual: %s", FormatDollars(state.actual->total).c_str());
    ImGui::TextColored(NOTE_COLOR, "actual %s", ConceptPlanView::CostSplit(*state.actual).c_str());
}

void RenderStatus(const ConceptJobState& state)
{
    const bool running = g_ConceptJob.IsRunning();
    if (running && state.cancelRequested)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
        ImGui::TextWrapped("Cancelling: no new requests start; requests already sent finish (they are paid for) "
                           "and are kept.");
        ImGui::PopStyleColor();
    }
    else if (running)
    {
        ImGui::ProgressBar(state.Progress(), ImVec2(-FLT_MIN, 0.0f), Editor::Concepts::JobPhaseLabel(state.phase));
    }
    else
    {
        const bool good = state.phase == JobPhase::Finished;
        ImGui::TextColored(good ? OK_COLOR : ERROR_COLOR, "%s", Editor::Concepts::JobPhaseLabel(state.phase));
    }
    if (!state.message.empty())
        ImGui::TextWrapped("%s", state.message.c_str());
    for (const Editor::Concepts::PlanReason& reason : state.reasons)
        ImGui::BulletText("%s", reason.message.c_str());
}

void RenderButtons(const ConceptJobState& state)
{
    static std::string error;
    if (g_ConceptJob.IsRunning())
    {
        ImGui::BeginDisabled(state.cancelRequested);
        if (ImGui::Button("Cancel"))
            g_ConceptJob.Cancel();
        ImGui::EndDisabled();
    }
    else
    {
        if (state.CanResume() && ImGui::Button("Resume"))
            g_ConceptJob.Resume(error);
        if (state.CanResume())
            ImGui::SameLine();
        if (!state.sheet.empty() && ImGui::Button("Open contact sheet"))
            Editor::Files::OpenWithSystem(Editor::Text::Utf8Path(state.sheet), error);
        if (!state.sheet.empty())
            ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            error.clear();
            g_ConceptJob.Dismiss();
        }
    }
    if (!error.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", error.c_str());
}

void RenderLog()
{
    const std::string& log = g_ConceptJob.LogTail();
    if (log.empty() || !ImGui::CollapsingHeader("Log (concepts.py)"))
        return;
    ImGui::InputTextMultiline("##ConceptLog", const_cast<char*>(log.c_str()), log.size() + 1,
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * LOG_ROWS), ImGuiInputTextFlags_ReadOnly);
}
} // namespace

void Render()
{
    if (!g_ConceptJob.HasJob())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - WINDOW_MARGIN, io.DisplaySize.y - WINDOW_MARGIN),
                            ImGuiCond_FirstUseEver, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(Scaled(WINDOW_WIDTH), Scaled(WINDOW_HEIGHT)), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(WINDOW_TITLE, nullptr, ImGuiWindowFlags_NoCollapse))
    {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
            g_MuEditorCore.SetHoveringUI(true);
        const ConceptJobState& state = g_ConceptJob.State();
        ImGui::TextWrapped("%s", g_ConceptJob.Title().c_str());
        if (!state.batch.empty())
            ImGui::TextColored(NOTE_COLOR, "batch %s", state.batch.c_str());
        RenderStatus(state);
        RenderItems(state);
        RenderCost(state);
        RenderButtons(state);
        RenderLog();
    }
    ImGui::End();
}
} // namespace Editor::ItemEditor::ConceptJobPanel

#endif // _EDITOR
