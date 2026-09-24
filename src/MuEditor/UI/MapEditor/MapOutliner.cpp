#include "stdafx.h"

#ifdef _EDITOR

#include "MapOutliner.h"

#include "MapAssetReview.h"
#include "MapObjectEditor.h"
#include "MapObjectPlace.h"

#include "Assets/EditorText.h"
#include "Core/EditorCamera.h"
#include "Core/MuEditorCore.h"
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
constexpr int ALL_TYPES = -1;

enum Column
{
    COLUMN_NAME,
    COLUMN_TYPE,
    COLUMN_TILE,
    COLUMN_HEIGHT,
    COLUMN_ANGLE,
    COLUMN_SCALE,
    COLUMN_COUNT,
};

const ImVec2 FIRST_USE_SIZE(600.0f, 460.0f);
constexpr float FIRST_USE_RIGHT_MARGIN = 620.0f; // from the right edge of the game window
constexpr float FIRST_USE_TOP = 60.0f;
constexpr float NAME_FILTER_WIDTH = 170.0f;
constexpr float TYPE_FILTER_WIDTH = 200.0f;
constexpr float TYPE_COLUMN_WIDTH = 40.0f;
constexpr float TILE_COLUMN_WIDTH = 90.0f;
constexpr float HEIGHT_COLUMN_WIDTH = 50.0f;
constexpr float ANGLE_COLUMN_WIDTH = 90.0f;
constexpr float SCALE_COLUMN_WIDTH = 45.0f;

const ImVec4 HINT_COLOR(0.7f, 0.9f, 1.0f, 1.0f);
} // namespace

CMapOutliner& CMapOutliner::GetInstance()
{
    static CMapOutliner instance;
    return instance;
}

bool CMapOutliner::Render(bool* open, int world)
{
    m_selected = false;
    if (open == nullptr || !*open)
        return false;
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(FIRST_USE_SIZE, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - FIRST_USE_RIGHT_MARGIN, FIRST_USE_TOP), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Outliner", open);
    // Clicks on the window must not reach the world (see CMuEditorCore).
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        g_MuEditorCore.SetHoveringUI(true);
    if (!visible)
    {
        ImGui::End();
        return false;
    }

    Editor::ObjectPlace::CountLiveObjects(m_typeCounts);
    RefreshNames(world);
    RefreshNameMatches();
    CollectRows();
    RenderFilters();
    RenderTable();
    ImGui::End();
    return m_selected;
}

void CMapOutliner::RefreshNames(int world)
{
    if (world == m_world && m_typeNames.size() >= m_typeCounts.size())
        return;
    m_world = world;
    m_typeNames.resize(m_typeCounts.size());
    for (std::size_t type = 0; type < m_typeNames.size(); ++type)
    {
        const std::string* catalogName = g_MapAssetReview.CatalogNameOf(world, static_cast<int>(type));
        m_typeNames[type] =
            catalogName != nullptr ? *catalogName : Editor::ObjectPlace::ModelName(static_cast<int>(type));
    }
    m_appliedFilter.clear();
    m_nameMatches.clear(); // rebuilt for the new names
}

void CMapOutliner::RefreshNameMatches()
{
    if (m_nameMatches.size() == m_typeNames.size() && m_appliedFilter == m_filter)
        return;
    m_appliedFilter = m_filter;
    m_nameMatches.resize(m_typeNames.size());
    for (std::size_t type = 0; type < m_typeNames.size(); ++type)
        m_nameMatches[type] = Editor::Text::ContainsIgnoringCase(m_typeNames[type], m_appliedFilter) ? 1 : 0;
}

void CMapOutliner::CollectRows()
{
    m_rows.clear();
    Editor::ObjectPlace::ForEachLiveObject(
        [this](OBJECT* o)
        {
            if (o->Type < 0 || static_cast<std::size_t>(o->Type) >= m_nameMatches.size())
                return;
            if (m_typeFilter != ALL_TYPES && o->Type != m_typeFilter)
                return;
            if (m_nameMatches[o->Type] != 0)
                m_rows.push_back(o);
        });
}

const std::string& CMapOutliner::NameOf(int type) const
{
    return m_typeNames[type];
}

void CMapOutliner::RenderFilters()
{
    ImGui::TextColored(HINT_COLOR, "Click: select it and look at it. Shift/Cmd+click: add or remove it.");
    ImGui::TextColored(HINT_COLOR, "Right-click a row: select every object of its type.");
    ImGui::SetNextItemWidth(NAME_FILTER_WIDTH);
    ImGui::InputTextWithHint("##name", "Filter by name", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    RenderTypeFilter();

    OBJECT* primary = g_MapObjectEditor.Primary();
    char label[64] = "Select all of type";
    if (primary != nullptr)
        std::snprintf(label, sizeof(label), "Select all of type %d", primary->Type);
    ImGui::BeginDisabled(primary == nullptr);
    if (ImGui::Button(label))
        SelectAllOfType(primary->Type);
    ImGui::EndDisabled();
    ImGui::SameLine();
    int total = 0;
    for (int count : m_typeCounts)
        total += count;
    ImGui::Text("%zu of %d objects shown, %zu selected", m_rows.size(), total, g_MapObjectEditor.Selection().Count());
}

void CMapOutliner::RenderTypeFilter()
{
    char preview[96] = "All types";
    if (m_typeFilter != ALL_TYPES && static_cast<std::size_t>(m_typeFilter) < m_typeNames.size())
        std::snprintf(preview, sizeof(preview), "%d %s", m_typeFilter, NameOf(m_typeFilter).c_str());
    ImGui::SetNextItemWidth(TYPE_FILTER_WIDTH);
    if (!ImGui::BeginCombo("##type", preview))
        return;
    if (ImGui::Selectable("All types", m_typeFilter == ALL_TYPES))
        m_typeFilter = ALL_TYPES;
    for (std::size_t type = 0; type < m_typeCounts.size() && type < m_typeNames.size(); ++type)
    {
        if (m_typeCounts[type] == 0)
            continue;
        char item[128];
        std::snprintf(item, sizeof(item), "%zu %s (%d)", type, m_typeNames[type].c_str(), m_typeCounts[type]);
        if (ImGui::Selectable(item, m_typeFilter == static_cast<int>(type)))
            m_typeFilter = static_cast<int>(type);
    }
    ImGui::EndCombo();
}

void CMapOutliner::RenderTable()
{
    const ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("OutlinerObjects", COLUMN_COUNT, flags))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, TYPE_COLUMN_WIDTH);
    ImGui::TableSetupColumn("Tile x, y", ImGuiTableColumnFlags_WidthFixed, TILE_COLUMN_WIDTH);
    ImGui::TableSetupColumn("Height", ImGuiTableColumnFlags_WidthFixed, HEIGHT_COLUMN_WIDTH);
    ImGui::TableSetupColumn("Angle", ImGuiTableColumnFlags_WidthFixed, ANGLE_COLUMN_WIDTH);
    ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthFixed, SCALE_COLUMN_WIDTH);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_rows.size()));
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            RenderRow(m_rows[row]);
    }
    ImGui::EndTable();
}

void CMapOutliner::RenderRow(OBJECT* object)
{
    ImGui::PushID(object);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(COLUMN_NAME);
    const bool selected = g_MapObjectEditor.Selection().Contains(object);
    if (ImGui::Selectable(NameOf(object->Type).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
        OnRowClicked(object);
    if (ImGui::BeginPopupContextItem("row"))
    {
        if (ImGui::MenuItem("Select all of this type"))
            SelectAllOfType(object->Type);
        ImGui::EndPopup();
    }
    ImGui::TableSetColumnIndex(COLUMN_TYPE);
    ImGui::Text("%d", object->Type);
    ImGui::TableSetColumnIndex(COLUMN_TILE);
    ImGui::Text("%.1f, %.1f", object->Position[0] / TERRAIN_SCALE, object->Position[1] / TERRAIN_SCALE);
    ImGui::TableSetColumnIndex(COLUMN_HEIGHT);
    ImGui::Text("%.0f", object->Position[2]);
    ImGui::TableSetColumnIndex(COLUMN_ANGLE);
    ImGui::Text("%.0f, %.0f, %.0f", object->Angle[0], object->Angle[1], object->Angle[2]);
    ImGui::TableSetColumnIndex(COLUMN_SCALE);
    ImGui::Text("%.2f", object->Scale);
    ImGui::PopID();
}

void CMapOutliner::OnRowClicked(OBJECT* object)
{
    m_selected = true;
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyShift || io.KeyCtrl)
    {
        g_MapObjectEditor.Toggle(object);
        return;
    }
    g_MapObjectEditor.Select({object});
    Editor::Camera::FocusOn(object);
}

void CMapOutliner::SelectAllOfType(int type)
{
    m_selected = true;
    std::vector<OBJECT*> objects = Editor::ObjectPlace::LiveObjectsOfType(type);
    // The primary stays the primary when it is of that type.
    OBJECT* primary = g_MapObjectEditor.Primary();
    const auto found = std::find(objects.begin(), objects.end(), primary);
    if (found != objects.end())
        std::rotate(found, found + 1, objects.end());
    g_MapObjectEditor.Select(objects);
}

#endif // _EDITOR
