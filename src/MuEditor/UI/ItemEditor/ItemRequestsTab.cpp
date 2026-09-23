#include "stdafx.h"

#ifdef _EDITOR

#include "ItemRequestsTab.h"

#include "ItemAbCompare.h"
#include "ItemRequestDialog.h"
#include "ItemRequestWatch.h"

#include "Assets/EditorText.h"
#include "Assets/ItemRequestDecision.h"
#include "Assets/RequestNaming.h"
#include "Core/EditorFiles.h"
#include "Core/MuEditorCore.h"
#include "UI/Console/MuEditorConsoleUI.h"

#include "imgui.h"

#include <chrono>

using namespace Editor::Assets;
namespace fs = std::filesystem;

namespace
{
constexpr int COLUMN_COUNT = 8;
constexpr std::size_t DATE_TIME_CHARS = 16; // "2026-09-23T10:15"
constexpr std::size_t DATE_CHARS = 10;
constexpr float NOTES_ROWS = 3.0f;
constexpr float TABLE_HEIGHT_RATIO = 0.55f; // of the tab; the selected request's actions below
// Fixed columns, in pixels at 100% editor UI scale: wide enough for their longest value.
constexpr float KIND_COLUMN_WIDTH = 140.0f; // "set (redesign)"
constexpr float STATUS_COLUMN_WIDTH = 80.0f;
constexpr float VERDICT_COLUMN_WIDTH = 90.0f;
constexpr float CREATED_COLUMN_WIDTH = 120.0f;
constexpr float DELIVERY_COLUMN_WIDTH = 60.0f;
constexpr const char* VALIDATOR_SCRIPT = "validate_request.py";
constexpr const char* REQUEST_FILE = "request.json";
constexpr const char* OWNER_DECISION_FILE = "owner-decision.json";

constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};

struct StatusColor
{
    const char* status;
    ImVec4 color;
};
constexpr StatusColor STATUS_COLORS[] = {
    {"open", {0.35f, 0.6f, 1.0f, 1.0f}},      {"claimed", {0.35f, 0.8f, 0.95f, 1.0f}},
    {"delivered", {0.8f, 0.5f, 1.0f, 1.0f}},  {"accepted", {0.45f, 0.85f, 0.45f, 1.0f}},
    {"rejected", {1.0f, 0.55f, 0.4f, 1.0f}},  {"withdrawn", {0.6f, 0.6f, 0.6f, 1.0f}},
};

float Scaled(float pixels)
{
    return pixels * g_MuEditorCore.GetUIScale();
}

ImVec4 ColorOf(const std::string& status)
{
    for (const StatusColor& entry : STATUS_COLORS)
    {
        if (status == entry.status)
            return entry.color;
    }
    return ERROR_COLOR;
}

std::string ItemLabel(const ItemRequestSummary& request, const ItemCatalog* catalog)
{
    if (request.targetKeys.empty())
        return "-";
    const std::string& key = request.targetKeys.front();
    const ItemCatalogEntry* item = catalog != nullptr ? catalog->FindByKey(key) : nullptr;
    std::string label = key;
    if (item != nullptr && !item->name.empty())
        label += " " + item->name;
    if (request.targetKeys.size() > 1)
        label += " +" + std::to_string(request.targetKeys.size() - 1);
    return label;
}

std::string OwnerVerdictLabel(const ItemRequestSummary& request)
{
    return request.ownerDecision ? request.ownerDecision->verdict : "-";
}

std::string CreatedLabel(const std::string& created)
{
    std::string text = created.substr(0, std::min(created.size(), DATE_TIME_CHARS));
    if (text.size() > DATE_CHARS)
        text[DATE_CHARS] = ' ';
    return text;
}

bool WasRejected(const ItemRequestSummary& request)
{
    return request.status == "rejected" ||
           (request.ownerDecision && request.ownerDecision->verdict == OWNER_VERDICT_REJECT);
}

// The notes of a rejection: the owner's, else the coordinator's reason.
std::string RejectionNotes(const ItemRequestSummary& request)
{
    if (request.ownerDecision && !request.ownerDecision->notes.empty())
        return request.ownerDecision->notes;
    return request.decisionReason;
}

std::string RepoRelative(const fs::path& path)
{
    return Editor::Text::GenericPathToUtf8(path.lexically_relative(Editor::Files::RepoRoot().root));
}
} // namespace

void CItemRequestsTab::Render(int& selectedType, bool& showInBrowse, const ItemCatalog* catalog)
{
    if (m_rescanPending)
    {
        m_rescanPending = false;
        g_ItemRequestWatch.Refresh();
    }
    PollValidation();
    RenderToolbar();
    if (g_ItemRequestWatch.RequestsDir().empty())
    {
        ImGui::TextColored(NOTE_COLOR, "No repository checkout found above the game folder (set MU_EDITOR_REPO_ROOT).");
        return;
    }
    const float tableHeight = ImGui::GetContentRegionAvail().y * TABLE_HEIGHT_RATIO;
    ImGui::BeginChild("RequestList", ImVec2(0.0f, tableHeight), ImGuiChildFlags_Borders);
    RenderTable(selectedType, catalog);
    ImGui::EndChild();
    ImGui::BeginChild("RequestActions", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    RenderSelected(showInBrowse, catalog);
    ImGui::EndChild();
}

void CItemRequestsTab::RenderToolbar()
{
    const std::vector<ItemRequestSummary>& requests = g_ItemRequestWatch.Requests();
    int live = 0;
    for (const ItemRequestSummary& request : requests)
        live += IsLiveRequestStatus(request.status) ? 1 : 0;
    ImGui::Text("%d requests, %d still open, claimed or delivered", static_cast<int>(requests.size()), live);
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        g_ItemRequestWatch.Refresh();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Read every request folder again (the tab also follows changes by itself).");
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "%s", RepoRelative(g_ItemRequestWatch.RequestsDir()).c_str());
}

void CItemRequestsTab::RenderTable(int& selectedType, const ItemCatalog* catalog)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("ItemRequestList", COLUMN_COUNT, flags))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Request", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, Scaled(KIND_COLUMN_WIDTH));
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, Scaled(STATUS_COLUMN_WIDTH));
    ImGui::TableSetupColumn("Your verdict", ImGuiTableColumnFlags_WidthFixed, Scaled(VERDICT_COLUMN_WIDTH));
    ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthFixed, Scaled(CREATED_COLUMN_WIDTH));
    ImGui::TableSetupColumn("Branch", ImGuiTableColumnFlags_WidthStretch, 2.5f);
    ImGui::TableSetupColumn("Delivery", ImGuiTableColumnFlags_WidthFixed, Scaled(DELIVERY_COLUMN_WIDTH));
    ImGui::TableHeadersRow();
    for (const ItemRequestSummary& request : g_ItemRequestWatch.Requests())
        RenderRow(request, selectedType, catalog);
    ImGui::EndTable();
    if (g_ItemRequestWatch.Requests().empty())
        ImGui::TextColored(NOTE_COLOR, "No item requests yet: select an item in Browse and press Ask Codex...");
}

void CItemRequestsTab::RenderRow(const ItemRequestSummary& request, int& selectedType, const ItemCatalog* catalog)
{
    ImGui::PushID(request.id.c_str());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    constexpr ImGuiSelectableFlags selectFlags = ImGuiSelectableFlags_SpanAllColumns;
    if (ImGui::Selectable(request.id.c_str(), request.id == m_selectedId, selectFlags))
        Select(request, selectedType, catalog);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(ItemLabel(request, catalog).c_str());
    ImGui::TableNextColumn();
    const std::string kind = request.setKind.empty() ? request.kind : request.kind + " (" + request.setKind + ")";
    ImGui::TextUnformatted(kind.c_str());
    ImGui::TableNextColumn();
    if (request.problem.empty())
        ImGui::TextColored(ColorOf(request.status), "%s", request.status.c_str());
    else
        ImGui::TextColored(ERROR_COLOR, "unreadable");
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(OwnerVerdictLabel(request).c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(CreatedLabel(request.created).c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(request.branch.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(request.deliveryPresent ? "yes" : "-");
    ImGui::PopID();
}

void CItemRequestsTab::Select(const ItemRequestSummary& request, int& selectedType, const ItemCatalog* catalog)
{
    if (m_selectedId != request.id)
    {
        m_notes[0] = '\0';
        m_confirmWithdraw = false;
    }
    m_selectedId = request.id;
    const ItemCatalogEntry* item =
        catalog != nullptr && !request.targetKeys.empty() ? catalog->FindByKey(request.targetKeys.front()) : nullptr;
    if (item != nullptr)
        selectedType = item->Type();
}

void CItemRequestsTab::RenderSelected(bool& showInBrowse, const ItemCatalog* catalog)
{
    const ItemRequestSummary* request = g_ItemRequestWatch.Find(m_selectedId);
    if (request == nullptr)
    {
        ImGui::TextColored(NOTE_COLOR, "Select a request to withdraw, accept or reject it.");
        RenderResult();
        return;
    }
    ImGui::Text("%s", request->id.c_str());
    if (!request->problem.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", request->problem.c_str());
    ImGui::TextWrapped("%s", request->summary.c_str());
    if (request->supersedes)
        ImGui::TextColored(NOTE_COLOR, "Supersedes %s", request->supersedes->c_str());
    if (!request->claimedBy.empty())
        ImGui::TextColored(NOTE_COLOR, "Claimed by %s", request->claimedBy.c_str());
    if (ImGui::Button("Show in Browse"))
        showInBrowse = true;
    ImGui::SameLine();
    if (ImGui::Button("Open folder"))
    {
        std::string error;
        if (!Editor::Files::OpenWithSystem(request->folder, error))
            m_error = error;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_validation.valid());
    if (ImGui::Button(m_validation.valid() ? "Validating..." : "Validate"))
        Validate(*request);
    ImGui::EndDisabled();
    RenderWithdraw(*request);
    RenderVerdict(*request, showInBrowse);
    RenderFollowUp(*request, showInBrowse, catalog);
    RenderResult();
}

void CItemRequestsTab::RenderWithdraw(const ItemRequestSummary& request)
{
    if (!IsWithdrawable(request.status))
        return;
    ImGui::SeparatorText("Withdraw");
    if (!m_confirmWithdraw)
    {
        if (ImGui::Button("Withdraw..."))
            m_confirmWithdraw = true;
        ImGui::SameLine();
        ImGui::TextColored(NOTE_COLOR, "Takes the request back before a decision (request.json: status, decision).");
        return;
    }
    ImGui::InputTextWithHint("##WithdrawReason", "why (optional)", m_notes, sizeof(m_notes));
    if (ImGui::Button("Withdraw the request"))
        Withdraw(request);
    ImGui::SameLine();
    if (ImGui::Button("Keep it"))
        m_confirmWithdraw = false;
}

void CItemRequestsTab::RenderVerdict(const ItemRequestSummary& request, bool& showInBrowse)
{
    if (request.status != "delivered")
        return;
    ImGui::SeparatorText("Your verdict on the delivery");
    if (request.ownerDecision)
        ImGui::TextColored(NOTE_COLOR, "Recorded: %s on %s%s%s", request.ownerDecision->verdict.c_str(),
                           request.ownerDecision->date.c_str(), request.ownerDecision->notes.empty() ? "" : ": ",
                           request.ownerDecision->notes.c_str());
    ImGui::InputTextMultiline("##VerdictNotes", m_notes, sizeof(m_notes),
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * NOTES_ROWS));
    if (ImGui::Button("Accept"))
        Decide(request, OWNER_VERDICT_ACCEPT);
    ImGui::SameLine();
    const bool hasNotes = !Editor::Text::Trim(m_notes).empty();
    ImGui::BeginDisabled(!hasNotes);
    if (ImGui::Button("Reject with notes"))
        Decide(request, OWNER_VERDICT_REJECT);
    ImGui::EndDisabled();
    if (!hasNotes)
    {
        ImGui::SameLine();
        ImGui::TextColored(NOTE_COLOR, "(write what is wrong to reject)");
    }
    ImGui::SameLine();
    RenderCompare(request, showInBrowse);
}

void CItemRequestsTab::RenderCompare(const ItemRequestSummary& request, bool& showInBrowse)
{
    ImGui::BeginDisabled(!request.deliveryPresent || request.targetKeys.empty());
    if (ImGui::Button("Compare"))
    {
        g_ItemAbCompare.OpenDeliveryCompare(request.id, request.targetKeys.front());
        showInBrowse = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Shows the item in Browse: your checkout's files and the delivery side by side, one camera "
                          "(no file changes).");
}

void CItemRequestsTab::RenderFollowUp(const ItemRequestSummary& request, bool& showInBrowse,
                                      const ItemCatalog* catalog)
{
    if (!WasRejected(request) || catalog == nullptr)
        return;
    ImGui::SeparatorText("Follow-up");
    const std::string notes = RejectionNotes(request);
    if (!notes.empty())
        ImGui::TextWrapped("Rejected: %s", notes.c_str());
    if (!ImGui::Button("Re-file with notes..."))
        return;
    showInBrowse = true; // the new request's captures come from the Browse preview
    g_ItemRequestDialog.OpenFollowUp(request, notes, *catalog);
}

void CItemRequestsTab::RenderResult()
{
    if (!m_error.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", m_error.c_str());
    if (!m_result.empty())
        ImGui::TextWrapped("%s", m_result.c_str());
    if (m_commands.empty())
        return;
    ImGui::TextDisabled("%s", m_commands.c_str());
    if (ImGui::Button("Copy commands"))
        ImGui::SetClipboardText(m_commands.c_str());
}

void CItemRequestsTab::Withdraw(const ItemRequestSummary& request)
{
    m_error.clear();
    m_result.clear();
    m_commands.clear();
    const std::string reason = Editor::Text::Trim(m_notes);
    if (!WithdrawRequest(request.folder, CurrentTimestamp().dateTime, reason, m_error))
        return;
    m_confirmWithdraw = false;
    m_result = "Withdrawn. Commit it on main and push, so Codex does not start on it:";
    m_commands = "git add " + RepoRelative(request.folder / REQUEST_FILE) + "\n" +
                 "git commit -m \"docs(assets): withdraw item request " + request.id + "\"\ngit push";
    g_MuEditorConsoleUI.LogEditor("[Items] Withdrew item request " + request.id);
    m_rescanPending = true;
}

void CItemRequestsTab::Decide(const ItemRequestSummary& request, const char* verdict)
{
    m_error.clear();
    m_result.clear();
    m_commands.clear();
    const OwnerDecision decision{verdict, Editor::Text::Trim(m_notes), CurrentTimestamp().date};
    if (!WriteOwnerDecision(request.folder, decision, m_error))
        return;
    m_result = std::string("Recorded '") + verdict + "' in owner-decision.json. Commit it on the worker branch "
               "and push it; the coordinator then accepts or rejects the request on main:";
    m_commands = "git add " + RepoRelative(request.folder / OWNER_DECISION_FILE) + "\n" +
                 "git commit -m \"docs(assets): owner verdict on item request " + request.id + "\"\n" +
                 "git push origin " + request.branch;
    g_MuEditorConsoleUI.LogEditor("[Items] " + std::string(verdict) + " for item request " + request.id);
    m_rescanPending = true;
}

void CItemRequestsTab::Validate(const ItemRequestSummary& request)
{
    m_error.clear();
    m_result.clear();
    m_commands.clear();
    m_validatingId = request.id;
    const fs::path script = g_ItemRequestWatch.RequestsDir() / VALIDATOR_SCRIPT;
    m_validation = Editor::PythonTool::RunInBackground(script, {Editor::Text::PathToUtf8(request.folder)});
}

void CItemRequestsTab::PollValidation()
{
    using namespace std::chrono_literals;
    if (!m_validation.valid() || m_validation.wait_for(0s) != std::future_status::ready)
        return;
    const Editor::PythonTool::Result result = m_validation.get();
    if (!result.started)
        m_error = "No python3 found to run " + std::string(VALIDATOR_SCRIPT) + ".";
    else
        m_result = result.output.empty() ? m_validatingId + ": exit code " + std::to_string(result.exitCode)
                                         : result.output;
}

#endif // _EDITOR
