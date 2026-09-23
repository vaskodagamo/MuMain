#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptGenerateDialog.h"

#include "ConceptJob.h"
#include "ConceptPlanView.h"
#include "ConceptTool.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h"
#include "Core/MuEditorCore.h"

#include "imgui.h"

#include <algorithm>

using Editor::Concepts::ConceptPlan;
using Editor::Concepts::FormatDollars;
using Editor::Concepts::PlanItem;
namespace Tool = Editor::ItemEditor::ConceptTool;
namespace View = Editor::ItemEditor::ConceptPlanView;

namespace
{
constexpr const char* POPUP_TITLE = "Generate concepts";
constexpr const char* PRICES_FILE = "tools/item_editor/image_prices.json";
constexpr float DIALOG_WIDTH = 820.0f;
constexpr float ITEM_TABLE_ROWS = 8.0f;
constexpr int ITEM_TABLE_COLUMNS = 4; // item, reference, note, estimate
constexpr float ITEM_COLUMN_WEIGHT = 2.0f;
constexpr float NOTE_COLUMN_WEIGHT = 3.0f;
constexpr float VARIANTS_SLIDER_WIDTH = 200.0f;
constexpr int MIN_VARIANTS = 1;
constexpr int MAX_VARIANTS = 8;
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 OK_COLOR{0.45f, 0.85f, 0.45f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};

float Scaled(float pixels)
{
    return pixels * g_MuEditorCore.GetUIScale();
}
} // namespace

CConceptGenerateDialog& CConceptGenerateDialog::GetInstance()
{
    static CConceptGenerateDialog instance;
    return instance;
}

void CConceptGenerateDialog::Open(std::vector<Item> items)
{
    if (items.empty() || Tool::Repo().empty())
        return;
    m_items = std::move(items);
    m_itemNotes.clear();
    m_note.fill('\0');
    m_sheet = false;
    m_error.clear();
    m_plan.Reset();
    ReadPresets();
    m_jobsSeen = g_ConceptJob.EndedVersion();
    m_open = true;
    m_openPending = true;
}

void CConceptGenerateDialog::ReadPresets()
{
    const std::vector<unsigned char> bytes = Editor::Files::ReadWholeFile(Tool::Repo() / PRICES_FILE);
    m_presets = Editor::Concepts::ParseConceptPresets(std::string(bytes.begin(), bytes.end()));
    const auto preferred = std::find_if(m_presets.presets.begin(), m_presets.presets.end(),
                                        [this](const auto& preset) { return preset.name == m_presets.defaultPreset; });
    ChoosePreset(preferred != m_presets.presets.end() ? static_cast<int>(preferred - m_presets.presets.begin()) : 0);
}

void CConceptGenerateDialog::ChoosePreset(int index)
{
    m_preset = index;
    const bool known = index >= 0 && index < static_cast<int>(m_presets.presets.size());
    m_variants = known ? m_presets.presets[index].variants : 0;
}

std::vector<std::string> CConceptGenerateDialog::Keys() const
{
    std::vector<std::string> keys;
    for (const Item& item : m_items)
        keys.push_back(item.key);
    return keys;
}

Editor::Concepts::GenerateSettings CConceptGenerateDialog::Settings() const
{
    Editor::Concepts::GenerateSettings settings;
    const bool known = m_preset >= 0 && m_preset < static_cast<int>(m_presets.presets.size());
    if (known)
        settings.preset = m_presets.presets[m_preset].name;
    const int presetVariants = known ? m_presets.presets[m_preset].variants : 0;
    settings.variants = m_variants != presetVariants ? m_variants : 0;
    settings.sheet = m_sheet;
    settings.note = m_note.data();
    for (const auto& [key, note] : m_itemNotes)
        settings.itemNotes[key] = note.data();
    return settings;
}

const PlanItem* CConceptGenerateDialog::PlanItemOf(const ConceptPlan* plan, const std::string& key) const
{
    if (plan == nullptr)
        return nullptr;
    const auto it = std::find_if(plan->items.begin(), plan->items.end(), [&key](const PlanItem& item) {
        return item.key == key || std::find(item.keys.begin(), item.keys.end(), key) != item.keys.end();
    });
    return it != plan->items.end() ? &*it : nullptr;
}

void CConceptGenerateDialog::Render()
{
    if (!m_open)
        return;
    if (m_jobsSeen != g_ConceptJob.EndedVersion())
    {
        m_jobsSeen = g_ConceptJob.EndedVersion();
        m_plan.Refresh(); // references were rendered, or a run changed what exists
    }
    m_plan.Want(Editor::Concepts::PlanArgs(Keys(), Settings(), Tool::Repo()));
    m_plan.Poll();
    if (m_openPending)
    {
        ImGui::OpenPopup(POPUP_TITLE);
        m_openPending = false;
    }
    ImGui::SetNextWindowSize(ImVec2(Scaled(DIALOG_WIDTH), 0.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal(POPUP_TITLE, nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        m_open = false;
        return;
    }
    g_MuEditorCore.SetHoveringUI(true);
    const ConceptPlan* plan = m_plan.Current();
    RenderSettings();
    RenderItems(plan);
    RenderReferences(plan);
    RenderEstimate(plan);
    ImGui::Separator();
    RenderButtons(plan);
    ImGui::EndPopup();
}

void CConceptGenerateDialog::RenderSettings()
{
    ImGui::Text("Concept images for %d item%s. Nothing is spent until you press Generate.",
                static_cast<int>(m_items.size()), m_items.size() == 1 ? "" : "s");
    for (int i = 0; i < static_cast<int>(m_presets.presets.size()); ++i)
    {
        const Editor::Concepts::ConceptPreset& preset = m_presets.presets[i];
        const std::string label = preset.name + ": " + preset.model + ", " + preset.quality + ", " +
                                  std::to_string(preset.variants) + " per item, reference " +
                                  std::to_string(preset.referenceSize) + " px";
        if (ImGui::RadioButton(label.c_str(), m_preset == i))
            ChoosePreset(i);
    }
    ImGui::SetNextItemWidth(Scaled(VARIANTS_SLIDER_WIDTH));
    ImGui::SliderInt("Variants per item", &m_variants, MIN_VARIANTS, MAX_VARIANTS);
    ImGui::Checkbox("Turnaround sheet (front and side on one 1536 x 1024 canvas)", &m_sheet);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##AllNote", "Note for every item, e.g. darker steel, less gold", m_note.data(),
                             m_note.size());
}

void CConceptGenerateDialog::RenderItems(const ConceptPlan* plan)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    const float height = ImGui::GetFrameHeightWithSpacing() * std::min<float>(ITEM_TABLE_ROWS, m_items.size() + 1.0f);
    if (!ImGui::BeginTable("ConceptItems", ITEM_TABLE_COLUMNS, flags, ImVec2(0.0f, height)))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, ITEM_COLUMN_WEIGHT);
    ImGui::TableSetupColumn("Reference", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Note for this item", ImGuiTableColumnFlags_WidthStretch, NOTE_COLUMN_WEIGHT);
    ImGui::TableSetupColumn("Estimate", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    for (const Item& item : m_items)
        RenderItemRow(item, plan);
    ImGui::EndTable();
}

void CConceptGenerateDialog::RenderItemRow(const Item& item, const ConceptPlan* plan)
{
    ImGui::PushID(item.key.c_str());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s (%s)", item.name.c_str(), item.key.c_str());
    const PlanItem* planned = PlanItemOf(plan, item.key);
    ImGui::TableNextColumn();
    if (planned == nullptr)
        ImGui::TextColored(NOTE_COLOR, "-");
    else if (planned->referenceExists)
        ImGui::TextColored(OK_COLOR, "ready");
    else
        ImGui::TextColored(ERROR_COLOR, "missing");
    ImGui::TableNextColumn();
    NoteBuffer& note = m_itemNotes[item.key];
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##ItemNote", note.data(), note.size());
    ImGui::TableNextColumn();
    if (planned != nullptr)
        ImGui::Text("%s (%d)", FormatDollars(planned->estimate.total).c_str(), planned->images);
    ImGui::PopID();
}

void CConceptGenerateDialog::RenderReferences(const ConceptPlan* plan)
{
    if (plan == nullptr || !plan->ok)
        return;
    const std::vector<std::string> missing = plan->MissingReferences();
    if (missing.empty())
        return;
    ImGui::TextColored(ERROR_COLOR, "No reference render (the item's current look) for: %s",
                       Editor::Text::Join(missing, ", ").c_str());
    const bool busy = g_ConceptJob.IsRunning();
    ImGui::BeginDisabled(busy);
    const std::string label = "Render missing references (" + std::to_string(missing.size()) + ")";
    if (ImGui::Button(label.c_str()))
    {
        std::vector<CConceptJob::Item> items;
        for (const std::string& key : missing)
            items.emplace_back(key, key);
        if (!g_ConceptJob.Start(CConceptJob::Kind::References, "Render reference images",
                                Editor::Concepts::RefsArgs(missing, Tool::Repo()), items, m_error))
            return;
        m_error.clear();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "offline, with Blender (free); the estimate updates when they are done");
}

void CConceptGenerateDialog::RenderEstimate(const ConceptPlan* plan)
{
    ImGui::SeparatorText("Cost");
    View::RenderPlanState(plan, m_plan.IsWorking(), m_plan.Problem());
    if (plan == nullptr || !plan->ok)
        return;
    View::RenderTotals(*plan);
    View::RenderApiKey(*plan);
    View::RenderRefusal(*plan);
    View::RenderTestServerNote();
}

void CConceptGenerateDialog::RenderButtons(const ConceptPlan* plan)
{
    const bool busy = g_ConceptJob.IsRunning();
    const bool ready = plan != nullptr && plan->ok && !plan->wouldRefuse && !busy;
    const std::string label =
        plan != nullptr && plan->ok ? "Generate for " + FormatDollars(plan->total.total) : std::string("Generate");
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button(label.c_str()))
        StartRun(*plan);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
        Close();
    if (busy)
        ImGui::TextColored(NOTE_COLOR, "A concepts job is running; its window shows the progress.");
    if (!m_error.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", m_error.c_str());
}

void CConceptGenerateDialog::StartRun(const ConceptPlan& plan)
{
    std::vector<CConceptJob::Item> items;
    for (const PlanItem& item : plan.items)
        items.emplace_back(item.key, item.name);
    const std::string title = "Generate concepts for " + std::to_string(plan.items.size()) + " item" +
                              (plan.items.size() == 1 ? "" : "s") + " (estimate " + FormatDollars(plan.total.total) +
                              ")";
    std::vector<std::string> arguments =
        Editor::Concepts::RunArgs(Keys(), Settings(), Tool::Repo(), Tool::TestApiBase());
    if (!g_ConceptJob.Start(CConceptJob::Kind::Generate, title, std::move(arguments), items, m_error))
        return;
    Close();
}

void CConceptGenerateDialog::Close()
{
    m_open = false;
    m_plan.Reset();
    ImGui::CloseCurrentPopup();
}

#endif // _EDITOR
