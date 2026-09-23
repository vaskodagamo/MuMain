#include "stdafx.h"

#ifdef _EDITOR

#include "ItemBrowseTab.h"

#include "ItemBrowseDetails.h"
#include "ItemThumbnailView.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h"
#include "Core/MuEditorCore.h"
#include "UI/MapEditor/ObjectThumbnail.h"

#include "imgui.h"

#include <algorithm>
#include <iterator>

namespace
{
using Editor::Items::BrowseRow;
using Editor::Items::ItemStatus;
using Editor::Items::SortKey;

// Layout, in pixels at 100% editor UI scale.
constexpr float FILTER_PANEL_WIDTH = 235.0f;
constexpr float DETAILS_PANEL_WIDTH = 380.0f;
constexpr float LIST_THUMB_SIZE = 40.0f;
constexpr float GRID_TILE_SIZE = 112.0f;
constexpr float CLASS_BUTTON_WIDTH = 46.0f;
constexpr int CLASS_BUTTONS_PER_LINE = 4;
constexpr float STATUS_DOT_RADIUS = 5.0f;
constexpr float NAME_COLUMN_WEIGHT = 3.0f;
constexpr float CLASSES_COLUMN_WEIGHT = 2.0f;
constexpr int LIST_COLUMN_COUNT = 8;
constexpr float KEY_COLUMN_WIDTH = 56.0f; // "13-127"
constexpr float SORT_COMBO_WIDTH = 170.0f;
constexpr float SCROLL_TARGET_RATIO = 0.3f; // a selected item scrolled to sits in the upper third
constexpr float TIER_DRAG_SPEED = 0.05f;    // tiers per pixel of mouse drag

constexpr int CLASS_COUNT = MAX_CLASS;
constexpr int STAGE_COUNT = 3;
// Class names per stage (1, 2, 3); MG, DL and RF have no second class.
constexpr const char* STAGE_NAMES[CLASS_COUNT][STAGE_COUNT] = {
    {"Dark Wizard", "Soul Master", "Grand Master"},
    {"Dark Knight", "Blade Knight", "Blade Master"},
    {"Fairy Elf", "Muse Elf", "High Elf"},
    {"Magic Gladiator", "", "Duel Master"},
    {"Dark Lord", "", "Lord Emperor"},
    {"Summoner", "Bloody Summoner", "Dimension Master"},
    {"Rage Fighter", "", "Fist Master"},
};

struct StatusChoice
{
    const char* label;
    std::optional<ItemStatus> status;
};
const StatusChoice STATUS_CHOICES[] = {
    {"any status", std::nullopt},
    {"original", ItemStatus::Original},
    {"changed", ItemStatus::Changed},
    {"at Codex", ItemStatus::AtCodex},
    {"unknown", ItemStatus::Unknown},
};

struct SortChoice
{
    const char* label;
    SortKey key;
};
constexpr SortChoice SORT_CHOICES[] = {
    {"Tier (basic -> rare)", SortKey::Tier}, {"Group / index", SortKey::GroupIndex},
    {"Name", SortKey::Name},                 {"Drop level", SortKey::DropLevel},
    {"Required level", SortKey::RequireLevel}, {"Status", SortKey::Status},
};

constexpr ImU32 STATUS_COLORS[] = {
    IM_COL32(90, 200, 110, 255),  // original
    IM_COL32(240, 170, 60, 255),  // changed
    IM_COL32(90, 150, 255, 255),  // at Codex
    IM_COL32(120, 120, 120, 255), // unknown
};
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 WARNING_COLOR{1.0f, 0.8f, 0.4f, 1.0f};
constexpr ImVec4 ACTIVE_BUTTON_COLOR{0.2f, 0.5f, 0.9f, 1.0f};
constexpr ImVec4 SELECTED_TILE_COLOR{0.3f, 0.5f, 0.8f, 0.45f};

float Scaled(float pixels)
{
    return pixels * g_MuEditorCore.GetUIScale();
}

void DrawStatusDot(ItemStatus status)
{
    const float radius = Scaled(STATUS_DOT_RADIUS);
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const ImVec2 center(corner.x + radius, corner.y + ImGui::GetTextLineHeight() * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, STATUS_COLORS[static_cast<int>(status)]);
    ImGui::Dummy(ImVec2(radius * 2.0f, ImGui::GetTextLineHeight()));
    ImGui::SameLine();
    ImGui::TextUnformatted(Editor::Items::StatusLabel(status));
}

std::string TierLabel(const BrowseRow& row)
{
    return row.tier > 0 ? "T" + std::to_string(row.tier) : "-";
}

const char* DisplayName(const BrowseRow& row)
{
    return row.name.empty() ? "(no name)" : row.name.c_str();
}

// A toggle button that shows whether it is on.
bool ToggleButton(const char* label, bool on, float width)
{
    if (on)
        ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_BUTTON_COLOR);
    const bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    if (on)
        ImGui::PopStyleColor();
    return clicked;
}
} // namespace

void CItemBrowseTab::OnShown()
{
    m_rowsBuilt = false; // names and classes may have changed in the Stats table
    m_scrollToSelected = true;
}

void CItemBrowseTab::EnsureCatalog()
{
    if (m_catalogLoaded)
        return;
    m_catalogLoaded = true;
    const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
    if (repo.empty())
    {
        m_repoNote = "no repository checkout found above the game folder (set MU_EDITOR_REPO_ROOT)";
        return;
    }
    m_load = Editor::Assets::LoadItemCatalog(repo);
    if (!m_load.catalog)
        return;
    // Families in item group order: swords first, potions and skill books last.
    for (const Editor::Assets::ItemCatalogEntry& item : m_load.catalog->items)
    {
        if (std::find(m_families.begin(), m_families.end(), item.family) == m_families.end())
            m_families.push_back(item.family);
    }
}

std::string CItemBrowseTab::CatalogNote() const
{
    if (!m_repoNote.empty())
        return m_repoNote;
    if (!m_load.catalog)
        return m_load.error + " (build it with tools/item_editor/build_item_catalog.py)";
    return "the catalog has no entry for this item (no model loaded for it)";
}

void CItemBrowseTab::RebuildRows()
{
    const Editor::Assets::ItemCatalog* catalog = m_load.catalog ? &*m_load.catalog : nullptr;
    m_rows = Editor::ItemEditor::BuildBrowseRows(catalog);
    RefreshStatuses();
    m_rowsBuilt = true;
    m_shownDirty = true;
}

void CItemBrowseTab::RefreshStatuses()
{
    Editor::ItemEditor::ApplyStatuses(m_rows, Editor::Files::RepoRoot().root, m_digests);
}

void CItemBrowseTab::UpdateShown()
{
    m_filter.search = m_search;
    m_filter.status = STATUS_CHOICES[m_statusChoice].status;
    if (!m_shownDirty && m_filter == m_shownFilter && m_order == m_shownOrder)
        return;
    m_shown = Editor::Items::FilterAndSort(m_rows, m_filter, m_order);
    m_familyCounts = Editor::Items::CountFamilies(m_rows, m_filter);
    m_shownFilter = m_filter;
    m_shownOrder = m_order;
    m_shownDirty = false;
}

const BrowseRow* CItemBrowseTab::RowOfType(int type) const
{
    const auto it = std::lower_bound(m_rows.begin(), m_rows.end(), type,
                                     [](const BrowseRow& row, int value) { return row.type < value; });
    return it != m_rows.end() && it->type == type ? &*it : nullptr;
}

void CItemBrowseTab::Select(int type, int& selectedType)
{
    selectedType = type;
    m_lastSelected = type;
}

void CItemBrowseTab::Render(int& selectedType)
{
    EnsureCatalog();
    if (!m_rowsBuilt)
        RebuildRows();
    if (selectedType != m_lastSelected)
    {
        m_lastSelected = selectedType; // picked in the Stats table
        m_scrollToSelected = true;
    }
    g_ObjectThumbnail.BeginFrame();

    const float filterWidth = Scaled(FILTER_PANEL_WIDTH);
    const float detailsWidth = Scaled(DETAILS_PANEL_WIDTH);
    ImGui::BeginChild("BrowseFilters", ImVec2(filterWidth, 0.0f), ImGuiChildFlags_Borders);
    RenderFilterPanel();
    ImGui::EndChild();
    UpdateShown();

    ImGui::SameLine();
    const float middleWidth = ImGui::GetContentRegionAvail().x - detailsWidth - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginChild("BrowseItems", ImVec2(middleWidth, 0.0f), ImGuiChildFlags_Borders);
    RenderResultBar();
    if (m_gridView)
        RenderGrid(selectedType);
    else
        RenderList(selectedType);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("BrowseDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    RenderDetailsPanel(selectedType);
    ImGui::EndChild();
    m_scrollToSelected = false;
}

void CItemBrowseTab::RenderFilterPanel()
{
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##BrowseSearch", "Search name or key (0-19)", m_search, sizeof(m_search));
    RenderClassFilter();
    RenderTierAndStatusFilter();
    ImGui::Checkbox("Only items with a model", &m_filter.onlyWithModel);
    RenderFamilyList();
}

void CItemBrowseTab::RenderClassFilter()
{
    ImGui::SeparatorText("Class");
    const float width = Scaled(CLASS_BUTTON_WIDTH);
    if (ToggleButton("All", m_filter.baseClass == Editor::Items::ANY_CLASS, width))
        m_filter.baseClass = Editor::Items::ANY_CLASS;
    for (int baseClass = 0; baseClass < CLASS_COUNT; ++baseClass)
    {
        if ((baseClass + 1) % CLASS_BUTTONS_PER_LINE != 0)
            ImGui::SameLine();
        if (ToggleButton(Editor::Items::ClassLabel(baseClass), m_filter.baseClass == baseClass, width))
            m_filter.baseClass = baseClass;
    }
    RenderStageChoice();
}

void CItemBrowseTab::RenderStageChoice()
{
    const int baseClass = m_filter.baseClass;
    if (baseClass == Editor::Items::ANY_CLASS)
    {
        ImGui::TextColored(NOTE_COLOR, "Pick a class to see what it can equip.");
        return;
    }
    const std::span<const std::uint8_t> stages = Editor::Items::ClassStages(baseClass);
    if (std::find(stages.begin(), stages.end(), m_filter.classStage) == stages.end())
        m_filter.classStage = stages.front(); // e.g. stage 2 picked for DK, then MG chosen
    const char* current = STAGE_NAMES[baseClass][m_filter.classStage - 1];
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (!ImGui::BeginCombo("##Stage", current))
        return;
    for (const std::uint8_t stage : stages)
    {
        const std::string label = std::to_string(stage) + "  " + STAGE_NAMES[baseClass][stage - 1];
        if (ImGui::Selectable(label.c_str(), m_filter.classStage == stage))
            m_filter.classStage = stage;
    }
    ImGui::EndCombo();
}

void CItemBrowseTab::RenderTierAndStatusFilter()
{
    ImGui::SeparatorText("Tier and status");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::DragIntRange2("##Tiers", &m_filter.tierMin, &m_filter.tierMax, TIER_DRAG_SPEED, Editor::Items::LOWEST_TIER,
                         Editor::Items::HIGHEST_TIER, "from T%d", "to T%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Status", STATUS_CHOICES[m_statusChoice].label))
    {
        for (int i = 0; i < static_cast<int>(std::size(STATUS_CHOICES)); ++i)
        {
            if (ImGui::Selectable(STATUS_CHOICES[i].label, m_statusChoice == i))
                m_statusChoice = i;
        }
        ImGui::EndCombo();
    }
}

void CItemBrowseTab::RenderFamilyList()
{
    ImGui::SeparatorText("Family");
    if (!m_load.catalog)
    {
        ImGui::TextColored(WARNING_COLOR, "No families, tiers or status:");
        ImGui::TextWrapped("%s", CatalogNote().c_str());
        return;
    }
    ImGui::BeginChild("Families", ImVec2(0.0f, 0.0f));
    int total = 0;
    for (const auto& [family, count] : m_familyCounts)
        total += count;
    const std::string allLabel = "All families (" + std::to_string(total) + ")";
    if (ImGui::Selectable(allLabel.c_str(), m_filter.family.empty()))
        m_filter.family.clear();
    for (const std::string& family : m_families)
    {
        const auto count = m_familyCounts.find(family);
        const int shown = count != m_familyCounts.end() ? count->second : 0;
        const std::string label = family + " (" + std::to_string(shown) + ")";
        ImGui::BeginDisabled(shown == 0 && m_filter.family != family);
        if (ImGui::Selectable(label.c_str(), m_filter.family == family))
            m_filter.family = family;
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
}

void CItemBrowseTab::RenderResultBar()
{
    ImGui::Text("%d items", static_cast<int>(m_shown.size()));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(Scaled(SORT_COMBO_WIDTH));
    const auto current = std::find_if(std::begin(SORT_CHOICES), std::end(SORT_CHOICES),
                                      [this](const SortChoice& choice) { return choice.key == m_order.key; });
    if (ImGui::BeginCombo("##Sort", current->label))
    {
        for (const SortChoice& choice : SORT_CHOICES)
        {
            if (ImGui::Selectable(choice.label, choice.key == m_order.key))
                m_order.key = choice.key;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(m_order.descending ? "Descending" : "Ascending"))
        m_order.descending = !m_order.descending;
    ImGui::SameLine();
    if (ToggleButton("List", !m_gridView, 0.0f))
        m_gridView = false;
    ImGui::SameLine();
    if (ToggleButton("Grid", m_gridView, 0.0f))
        m_gridView = true;
    ImGui::SameLine();
    if (ImGui::Button("Rescan files"))
    {
        m_digests.clear(); // hash the model files again (after a pull or a delivery)
        RefreshStatuses();
        m_shownDirty = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compare every model file with the catalog's original again.");
}

void CItemBrowseTab::RenderList(int& selectedType)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("BrowseList", LIST_COLUMN_COUNT, flags))
        return;
    const float thumbSize = Scaled(LIST_THUMB_SIZE);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, thumbSize);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, NAME_COLUMN_WEIGHT);
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, Scaled(KEY_COLUMN_WIDTH));
    ImGui::TableSetupColumn("Tier", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Drop lvl", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Req lvl", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Classes", ImGuiTableColumnFlags_WidthStretch, CLASSES_COLUMN_WEIGHT);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_shown.size()), thumbSize + ImGui::GetStyle().CellPadding.y * 2.0f);
    const auto selectedAt = std::find_if(m_shown.begin(), m_shown.end(),
                                         [&](int row) { return m_rows[row].type == selectedType; });
    const int scrollTo = m_scrollToSelected && selectedAt != m_shown.end()
                             ? static_cast<int>(selectedAt - m_shown.begin())
                             : -1;
    if (scrollTo >= 0)
        clipper.IncludeItemByIndex(scrollTo);
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            RenderListRow(m_rows[m_shown[i]], selectedType, thumbSize);
            if (i == scrollTo)
                ImGui::SetScrollHereY(SCROLL_TARGET_RATIO);
        }
    }
    ImGui::EndTable();
}

void CItemBrowseTab::RenderListRow(const BrowseRow& row, int& selectedType, float thumbSize)
{
    ImGui::PushID(row.type);
    ImGui::TableNextRow(ImGuiTableRowFlags_None, thumbSize);
    ImGui::TableNextColumn();
    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
    constexpr ImGuiSelectableFlags selectFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
    if (ImGui::Selectable("##row", row.type == selectedType, selectFlags, ImVec2(0.0f, thumbSize)))
        Select(row.type, selectedType);
    ImGui::SetCursorScreenPos(cellStart);
    Editor::ItemEditor::DrawItemThumbnail(row, thumbSize);

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(DisplayName(row));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.key.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(TierLabel(row).c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%d", row.dropLevel);
    ImGui::TableNextColumn();
    ImGui::Text("%d", row.requireLevel);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Editor::Items::ClassSummary(row.requireClass).c_str());
    ImGui::TableNextColumn();
    DrawStatusDot(row.status);
    ImGui::PopID();
}

void CItemBrowseTab::RenderGrid(int& selectedType)
{
    ImGui::BeginChild("BrowseGrid");
    const float tileSize = Scaled(GRID_TILE_SIZE);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float pitchX = tileSize + style.ItemSpacing.x;
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + style.ItemSpacing.x) / pitchX));
    const int count = static_cast<int>(m_shown.size());
    const int lines = (count + columns - 1) / columns;
    const float lineHeight = tileSize + ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y;

    ImGuiListClipper clipper;
    clipper.Begin(lines, lineHeight);
    const auto selectedAt = std::find_if(m_shown.begin(), m_shown.end(),
                                         [&](int row) { return m_rows[row].type == selectedType; });
    const int scrollLine = m_scrollToSelected && selectedAt != m_shown.end()
                               ? static_cast<int>(selectedAt - m_shown.begin()) / columns
                               : -1;
    if (scrollLine >= 0)
        clipper.IncludeItemByIndex(scrollLine);
    while (clipper.Step())
    {
        for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
        {
            for (int column = 0; column < columns && line * columns + column < count; ++column)
            {
                if (column > 0)
                    ImGui::SameLine();
                RenderGridTile(m_rows[m_shown[line * columns + column]], selectedType, tileSize);
            }
            if (line == scrollLine)
                ImGui::SetScrollHereY(SCROLL_TARGET_RATIO);
        }
    }
    ImGui::EndChild();
}

void CItemBrowseTab::RenderGridTile(const BrowseRow& row, int& selectedType, float tileSize)
{
    ImGui::PushID(row.type);
    ImGui::BeginGroup();
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const float tileHeight = tileSize + ImGui::GetTextLineHeightWithSpacing();
    if (row.type == selectedType)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(corner, ImVec2(corner.x + tileSize, corner.y + tileHeight),
                                                  ImGui::GetColorU32(SELECTED_TILE_COLOR));
    }
    if (ImGui::InvisibleButton("##tile", ImVec2(tileSize, tileHeight)))
        Select(row.type, selectedType);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(corner);
    Editor::ItemEditor::DrawItemThumbnail(row, tileSize);
    // The caption is drawn, not laid out, so a long name cannot widen the tile.
    const std::string caption = TierLabel(row) + " " + DisplayName(row);
    const ImVec2 captionAt(corner.x, corner.y + tileSize);
    const ImVec4 clip(corner.x, corner.y, corner.x + tileSize, corner.y + tileHeight);
    ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, captionAt, ImGui::GetColorU32(ImGuiCol_Text), caption.c_str(),
                                        nullptr, 0.0f, &clip);
    ImGui::EndGroup();
    if (hovered)
        ImGui::SetTooltip("%s (%s)\n%s", DisplayName(row), row.key.c_str(), Editor::Items::StatusLabel(row.status));
    ImGui::PopID();
}

void CItemBrowseTab::RenderDetailsPanel(int selectedType)
{
    const BrowseRow* row = RowOfType(selectedType);
    if (row == nullptr)
    {
        ImGui::TextColored(NOTE_COLOR, "Select an item to see its facts.");
        return;
    }
    Editor::ItemEditor::RenderItemDetails(*row, CatalogNote());
}

#endif // _EDITOR
