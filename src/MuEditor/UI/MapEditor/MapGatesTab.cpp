#include "stdafx.h"

#ifdef _EDITOR

#include "MapGatesTab.h"

#include "MapEditorStatusLine.h"

#include "Core/EditorCamera.h"
#include "Core/LiveGates.h"
#include "Core/NewMapFiles.h"
#include "Core/ServerExportFiles.h"
#include "Gates/GateChecks.h"
#include "Gates/GateDirection.h"
#include "Gates/GateQueries.h"
#include "MapInspect/TileArea.h" // TileOf

#include "Core/Globals/_define.h"        // EDIT_WALL
#include "Render/Renderer/RenderUtils.h" // mu::PackABGR
#include "Render/Terrain/TerrainGroundRects.h"
#include "Render/Terrain/ZzzLodTerrain.h" // TERRAIN_SIZE
#include "World/MapInfra/MapManager.h"
#include "World/MapInfra/MapNumbers.h"

#include "imgui.h"

#include <algorithm>

namespace
{
namespace Gates = Editor::Gates;
namespace GroundRects = Render::Terrain::GroundRects;

constexpr int LAST_TILE = TERRAIN_SIZE - 1;
constexpr int AREA_VALUES = 4;
constexpr float TABLE_HEIGHT_ROWS = 9.0f;

const ImVec4 HEADER_COLOR(0.9f, 0.9f, 0.6f, 1.0f);
const ImVec4 NOTE_COLOR(0.7f, 0.9f, 1.0f, 1.0f);
const std::uint32_t ENTER_FILL = mu::PackABGR(1.0f, 0.55f, 0.1f, 0.30f);
const std::uint32_t ENTER_OUTLINE = mu::PackABGR(1.0f, 0.6f, 0.1f, 0.95f);
const std::uint32_t ARRIVAL_FILL = mu::PackABGR(0.2f, 0.8f, 1.0f, 0.28f);
const std::uint32_t ARRIVAL_OUTLINE = mu::PackABGR(0.3f, 0.85f, 1.0f, 0.95f);
const std::uint32_t SELECTED_OUTLINE = mu::PackABGR(1.0f, 1.0f, 1.0f, 1.0f);
const std::uint32_t DRAWN_FILL = mu::PackABGR(1.0f, 0.95f, 0.2f, 0.35f);
const std::uint32_t DRAWN_OUTLINE = mu::PackABGR(1.0f, 0.95f, 0.15f, 1.0f);

std::string AreaText(const Gates::TileRect& area)
{
    return "[" + std::to_string(area.x1) + ", " + std::to_string(area.y1) + ", " + std::to_string(area.x2) + ", " +
           std::to_string(area.y2) + "]";
}

std::string MapLabel(int map)
{
    return std::to_string(map) + " " + Editor::LiveGates::MapName(map) + " (World" +
           std::to_string(World::MapNumbers::FolderOf(map)) + ")";
}

std::string GateSummary(const Gates::GateTable& table, int number)
{
    const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
    if (record.flag != Gates::FLAG_ENTER)
        return std::string("faces ") + Gates::DirectionName(record.direction);
    const int targetMap = Gates::TargetMap(table, number);
    if (targetMap < 0)
        return "to gate " + std::to_string(record.target) + " (not an arrival)";
    return "to gate " + std::to_string(record.target) + " on " + MapLabel(targetMap) + " " +
           AreaText(Gates::AreaOf(table[record.target]));
}

void ClampArea(Gates::TileRect& area)
{
    for (int* corner : {&area.x1, &area.y1, &area.x2, &area.y2})
        *corner = std::clamp(*corner, 0, LAST_TILE);
}
} // namespace

int CMapGatesTab::Render(const TerrainBrushInput& input)
{
    const int map = gMapManager.WorldActive;
    if (map != m_map)
        ResetForMap(map);

    const Gates::GateTable table = Editor::LiveGates::Table();
    RenderHeader(map);
    RenderGateTable(table, map);
    RenderWaysIn(table, map);
    RenderSelected(table);
    ImGui::Separator();
    RenderDrawing(input);
    RenderNewGate(map);
    ImGui::Separator();
    RenderExport(map);
    Editor::StatusLine::Render(m_status);
    for (const std::string& warning : m_warnings)
        ImGui::TextColored(Editor::StatusLine::WarningColor(), "! %s", warning.c_str());

    ShowOnGround(table, map);
    return m_drawing ? EDIT_WALL : EDIT_NONE;
}

void CMapGatesTab::Select(int number)
{
    if (gMapManager.WorldActive != m_map)
        ResetForMap(gMapManager.WorldActive);
    m_selected = number;
    m_scrollToSelected = true;
}

void CMapGatesTab::ResetForMap(int map)
{
    m_map = map;
    m_selected = -1;
    m_drawing = false;
    m_dragging = false;
    m_hasDrawn = false;
    m_warnings.clear();
    RefreshMaps();
    if (std::none_of(m_maps.begin(), m_maps.end(),
                     [this](const MapChoice& choice) { return choice.map == m_targetMap; }))
        m_targetMap = map;
}

void CMapGatesTab::RefreshMaps()
{
    m_maps.clear();
    for (const int world : Editor::NewMapFiles::WorldFolders())
    {
        const int other = World::MapNumbers::MapOfFolder(world);
        m_maps.push_back({other, MapLabel(other)});
    }
}

void CMapGatesTab::RenderHeader(int map)
{
    ImGui::TextColored(HEADER_COLOR, "Map %d, %s: client folder Data/World%d, server map Number %d", map,
                       Editor::LiveGates::MapName(map).c_str(), World::MapNumbers::FolderOf(map), map);
    ImGui::PushStyleColor(ImGuiCol_Text, NOTE_COLOR);
    ImGui::TextWrapped("Orange: enter gates (walk in to warp). Blue: arrivals. The walkability overlay is on. Gates "
                       "%d and up are yours to change; changes save Gate.bmd at once.",
                       Gates::FIRST_CUSTOM_GATE);
    ImGui::PopStyleColor();
}

void CMapGatesTab::RenderGateTable(const Gates::GateTable& table, int map)
{
    const std::vector<int> numbers = Gates::GatesOnMap(table, map);
    if (numbers.empty())
    {
        ImGui::TextDisabled("No gates on this map.");
        return;
    }
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    const float height = ImGui::GetTextLineHeightWithSpacing() * TABLE_HEIGHT_ROWS;
    if (!ImGui::BeginTable("##gates", 4, flags, ImVec2(0.0f, height)))
        return;
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Area", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Leads to / faces");
    ImGui::TableHeadersRow();
    for (const int number : numbers)
    {
        const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const std::string label = std::to_string(number) + (Gates::IsCustomNumber(number) ? " *" : "");
        if (ImGui::Selectable(label.c_str(), m_selected == number, ImGuiSelectableFlags_SpanAllColumns))
            m_selected = number;
        if (m_scrollToSelected && m_selected == number)
            ImGui::SetScrollHereY();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(Gates::KindName(record));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(AreaText(Gates::AreaOf(record)).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(GateSummary(table, number).c_str());
    }
    ImGui::EndTable();
    m_scrollToSelected = false;
    ImGui::TextDisabled("* added with the editor");
}

void CMapGatesTab::RenderWaysIn(const Gates::GateTable& table, int map)
{
    for (const int number : Gates::EnterGatesInto(table, map))
    {
        const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
        ImGui::BulletText("Way in: gate %d on %s %s lands on gate %d", number, MapLabel(record.map).c_str(),
                          AreaText(Gates::AreaOf(record)).c_str(), record.target);
    }
}

void CMapGatesTab::RenderSelected(const Gates::GateTable& table)
{
    if (m_selected < 0 || Gates::IsFree(table[static_cast<std::size_t>(m_selected)]))
        return;
    const Gates::GateRecord& record = table[static_cast<std::size_t>(m_selected)];
    ImGui::Text("Gate %d (%s)", m_selected, Gates::KindName(record));
    ImGui::SameLine();
    if (ImGui::SmallButton("Look at it"))
        LookAt(Gates::AreaOf(record));
    if (!Gates::IsCustomNumber(m_selected))
    {
        ImGui::TextDisabled("One of the game's own gates: shown, not changed.");
        return;
    }

    ImGui::BeginDisabled(!m_hasDrawn);
    if (ImGui::Button("Move it to the drawn area"))
    {
        Gates::GateChange change;
        change.area = m_drawn;
        Editor::LiveGates::EditResult result;
        std::string error;
        const bool ok = Editor::LiveGates::Change(m_selected, change, result, error);
        ShowResult("Moved gate " + std::to_string(m_selected), ok, error, result.report, result.warnings);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Remove it"))
    {
        Editor::LiveGates::EditResult result;
        std::string error;
        const bool ok = Editor::LiveGates::Remove(m_selected, false, result, error);
        ShowResult("Removed gate(s)", ok, error, result.report, result.warnings);
        if (ok)
            m_selected = -1;
    }
}

void CMapGatesTab::RenderDrawing(const TerrainBrushInput& input)
{
    ImGui::Checkbox("Draw an area on the ground (left-drag)", &m_drawing);
    if (m_drawing)
        TrackDrag(input);
    else
        m_dragging = false;

    int area[AREA_VALUES] = {m_drawn.x1, m_drawn.y1, m_drawn.x2, m_drawn.y2};
    if (ImGui::InputInt4("Area x1 y1 x2 y2", area))
    {
        m_drawn = {area[0], area[1], area[2], area[3]};
        ClampArea(m_drawn);
        m_hasDrawn = true;
    }
}

void CMapGatesTab::TrackDrag(const TerrainBrushInput& input)
{
    if (!input.onGround || !input.leftDown)
    {
        if (m_dragging)
            m_warnings = Gates::AreaWarnings(Editor::LiveGates::WalkMapOf(m_map), m_map, m_drawn, "the drawn area");
        m_dragging = false;
        return;
    }
    const int x = Editor::MapInspect::TileOf(input.groundX);
    const int y = Editor::MapInspect::TileOf(input.groundY);
    if (!m_dragging)
    {
        m_dragging = true;
        m_dragStartX = x;
        m_dragStartY = y;
    }
    m_drawn = {std::min(m_dragStartX, x), std::min(m_dragStartY, y), std::max(m_dragStartX, x),
               std::max(m_dragStartY, y)};
    m_hasDrawn = true;
}

void CMapGatesTab::RenderTargetMap()
{
    const auto current = std::find_if(m_maps.begin(), m_maps.end(),
                                      [this](const MapChoice& choice) { return choice.map == m_targetMap; });
    const std::string preview = current != m_maps.end() ? current->label : MapLabel(m_targetMap);
    if (!ImGui::BeginCombo("Arrives on map", preview.c_str()))
        return;
    if (ImGui::IsWindowAppearing())
        RefreshMaps(); // a map created since shows up
    for (const MapChoice& choice : m_maps)
    {
        if (ImGui::Selectable(choice.label.c_str(), choice.map == m_targetMap))
            m_targetMap = choice.map;
    }
    ImGui::EndCombo();
}

void CMapGatesTab::RenderNewGate(int map)
{
    ImGui::TextColored(HEADER_COLOR, "New gate: the area above leads to");
    RenderTargetMap();
    ImGui::InputInt4("Arrival x1 y1 x2 y2", m_targetArea);
    if (ImGui::BeginCombo("Faces on arrival", Gates::DirectionName(m_direction)))
    {
        for (int direction = 0; direction <= Gates::LAST_DIRECTION; ++direction)
        {
            if (ImGui::Selectable(Gates::DirectionName(direction), direction == m_direction))
                m_direction = direction;
        }
        ImGui::EndCombo();
    }
    ImGui::InputInt("Level needed", &m_level);
    m_level = std::clamp(m_level, 0, Gates::MAX_LEVEL_REQUIREMENT);

    ImGui::BeginDisabled(!m_hasDrawn);
    if (ImGui::Button("Add gate (saves Gate.bmd)", ImVec2(-1.0f, 0.0f)))
    {
        Gates::NewGatePair pair;
        pair.from = {map, m_drawn};
        pair.to = {m_targetMap, {m_targetArea[0], m_targetArea[1], m_targetArea[2], m_targetArea[3]}};
        pair.direction = m_direction;
        pair.level = m_level;
        Editor::LiveGates::EditResult result;
        std::string error;
        const bool ok = Editor::LiveGates::AddPair(pair, false, result, error);
        ShowResult(ok ? "Added gate " + std::to_string(result.numbers.at(0)) + " (arrival " +
                            std::to_string(result.numbers.at(1)) + ")"
                      : "Add gate",
                   ok, error, result.report, result.warnings);
        if (ok)
            m_selected = result.numbers.at(0);
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("A way back is a second gate, added on the other map.");
}

void CMapGatesTab::RenderExport(int map)
{
    ImGui::TextColored(HEADER_COLOR, "OpenMU");
    if (!ImGui::Button("Export this map for OpenMU (never applied)", ImVec2(-1.0f, 0.0f)))
        return;
    Editor::ServerExportFiles::ExportResult result;
    std::string error;
    const bool ok = Editor::ServerExportFiles::Export(map, Editor::ServerExportFiles::AUTO_SAFEZONE, {}, result, error);
    ShowResult("Export", ok, error, result.report, result.warnings);
}

void CMapGatesTab::ShowResult(const std::string& action, bool ok, const std::string& error, const std::string& report,
                              const std::vector<std::string>& warnings)
{
    m_status = ok ? action + (report.empty() ? "" : "\n" + report) : action + " FAILED: " + error;
    m_warnings = ok ? warnings : std::vector<std::string>{};
}

void CMapGatesTab::ShowOnGround(const Gates::GateTable& table, int map) const
{
    std::vector<GroundRects::Rect> rects;
    for (const int number : Gates::GatesOnMap(table, map))
    {
        const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
        const bool enter = record.flag == Gates::FLAG_ENTER;
        const Gates::TileRect area = Gates::AreaOf(record);
        std::uint32_t outline = enter ? ENTER_OUTLINE : ARRIVAL_OUTLINE;
        if (number == m_selected)
            outline = SELECTED_OUTLINE;
        rects.push_back({area.x1, area.y1, area.x2, area.y2, enter ? ENTER_FILL : ARRIVAL_FILL, outline});
    }
    if (m_hasDrawn)
        rects.push_back({m_drawn.x1, m_drawn.y1, m_drawn.x2, m_drawn.y2, DRAWN_FILL, DRAWN_OUTLINE});
    GroundRects::Show(rects);
}

void CMapGatesTab::LookAt(const Gates::TileRect& area)
{
    Editor::Camera::Pose pose;
    Editor::Camera::FrameGateArea(area.x1, area.y1, area.x2, area.y2, pose);
}

#endif // _EDITOR
