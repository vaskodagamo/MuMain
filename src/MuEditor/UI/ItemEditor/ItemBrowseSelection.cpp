#include "stdafx.h"

#ifdef _EDITOR

#include "ItemBrowseSelection.h"

#include "ConceptGenerateDialog.h"

#include "Core/MuEditorCore.h"

#include "imgui.h"

#include <algorithm>

namespace Editor::ItemEditor::BrowseSelection
{
namespace
{
using Items::BrowseRow;

constexpr std::size_t MAX_CHIPS = 6;
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 CHIP_COLOR{0.25f, 0.4f, 0.65f, 1.0f};

// The selected items the concepts tool knows (a catalog entry: key and model).
std::vector<CConceptGenerateDialog::Item> ConceptItems(const Editor::Editing::ItemSelection& selection,
                                                       const std::vector<BrowseRow>& rows)
{
    std::vector<CConceptGenerateDialog::Item> items;
    for (const int type : selection.Types())
    {
        const BrowseRow* row = FindRow(rows, type);
        if (row != nullptr && row->catalog != nullptr)
            items.push_back({row->key, row->name.empty() ? row->key : row->name});
    }
    return items;
}

void RenderChips(Editor::Editing::ItemSelection& selection, const std::vector<BrowseRow>& rows, int& selectedType)
{
    const std::vector<int> types = selection.Types(); // a chip click changes the selection
    ImGui::PushStyleColor(ImGuiCol_Button, CHIP_COLOR);
    for (std::size_t i = 0; i < std::min(types.size(), MAX_CHIPS); ++i)
    {
        const BrowseRow* row = FindRow(rows, types[i]);
        const std::string name = row != nullptr && !row->name.empty() ? row->name : std::to_string(types[i]);
        ImGui::PushID(types[i]);
        ImGui::SameLine();
        if (ImGui::SmallButton((name + "  x").c_str()))
        {
            selection.Toggle(types[i]);
            selectedType = selection.Primary();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Take %s out of the selection", name.c_str());
        ImGui::PopID();
    }
    ImGui::PopStyleColor();
    if (types.size() > MAX_CHIPS)
    {
        ImGui::SameLine();
        ImGui::TextColored(NOTE_COLOR, "+%d more", static_cast<int>(types.size() - MAX_CHIPS));
    }
}
} // namespace

const BrowseRow* FindRow(const std::vector<BrowseRow>& rows, int type)
{
    const auto it = std::lower_bound(rows.begin(), rows.end(), type,
                                     [](const BrowseRow& row, int value) { return row.type < value; });
    return it != rows.end() && it->type == type ? &*it : nullptr;
}

void RenderBar(Editor::Editing::ItemSelection& selection, const std::vector<BrowseRow>& rows,
               const std::vector<int>& shownTypes, int& selectedType)
{
    const std::vector<CConceptGenerateDialog::Item> items = ConceptItems(selection, rows);
    ImGui::BeginDisabled(items.empty());
    const std::string label = "Generate concepts (" + std::to_string(items.size()) + ")...";
    if (ImGui::Button(label.c_str()))
        g_ConceptGenerateDialog.Open(items);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Concept images for the selected items (Cmd-click or Shift-click to select several).");
    ImGui::SameLine();
    if (ImGui::Button("Select all shown"))
    {
        selection.SelectAll(shownTypes);
        selectedType = selection.Primary();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(selection.Count() == 0);
    if (ImGui::Button("Clear"))
        selection.Clear();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "%d selected", static_cast<int>(selection.Count()));
    RenderChips(selection, rows, selectedType);
}
} // namespace Editor::ItemEditor::BrowseSelection

#endif // _EDITOR
