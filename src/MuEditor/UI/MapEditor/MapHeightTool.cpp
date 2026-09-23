#include "stdafx.h"

#ifdef _EDITOR

#include "MapHeightTool.h"

#include "MapEditHistory.h"
#include "MapObjectPlace.h"
#include "MapTerrainLayers.h"

#include "Editing/EditCommandGroup.h"
#include "Editing/FieldBrush.h"
#include "Render/Renderer/RenderUtils.h" // mu::PackABGR
#include "Render/Terrain/ZzzLodTerrain.h"
#include "World/MapInfra/MapManager.h" // gMapManager.WorldActive

#include "imgui.h"

#include <memory>
#include <vector>

extern float BackTerrainHeight[];

using Editor::Editing::BrushCircle;
using Editor::Editing::CellRect;
using Editor::Editing::FloatField;

namespace
{
// Raise/Lower adds up to this much height per frame at the brush's core (world units).
constexpr Editor::BrushControls::Range STRENGTH_RANGE = {1.0f, 40.0f, 1.0f};
// TerrainHeight.OZB stores one byte per corner as height / factor, so nothing above
// 255 * factor survives a save (factor 3 on the login scene, 1.5 elsewhere).
constexpr float HEIGHT_BYTE_MAX = 255.0f;
constexpr float HEIGHT_FACTOR = 1.5f;
constexpr float LOGIN_SCENE_HEIGHT_FACTOR = 3.0f;
constexpr float TARGET_SLIDER_WIDTH = 200.0f;

const std::uint32_t OUTLINE_COLOR = mu::PackABGR(1.0f, 0.85f, 0.3f, 0.95f);
const std::vector<int> HEIGHT_LAYERS = {MAP_LAYER_HEIGHT};

float MaxHeight()
{
    const float factor = (gMapManager.WorldActive == WD_55LOGINSCENE) ? LOGIN_SCENE_HEIGHT_FACTOR : HEIGHT_FACTOR;
    return HEIGHT_BYTE_MAX * factor;
}

FloatField Heights()
{
    return {BackTerrainHeight, TERRAIN_SIZE, TERRAIN_SIZE, 1};
}

const char* StrokeLabel(CMapHeightTool::Tool tool, bool lower)
{
    switch (tool)
    {
    case CMapHeightTool::Tool::Flatten:
        return "Flatten ground";
    case CMapHeightTool::Tool::Smooth:
        return "Smooth ground";
    case CMapHeightTool::Tool::SetHeight:
        return "Set ground height";
    default:
        return lower ? "Lower ground" : "Raise ground";
    }
}

const char* ToolHint(CMapHeightTool::Tool tool)
{
    switch (tool)
    {
    case CMapHeightTool::Tool::Flatten:
        return "Left-click drags the ground towards its height where you pressed.";
    case CMapHeightTool::Tool::Smooth:
        return "Left-click evens out bumps and steps.";
    case CMapHeightTool::Tool::SetHeight:
        return "Left-click brings the ground to the target height. Alt-click takes the target from the ground.";
    default:
        return "Left-click raises the ground, right-click lowers it.";
    }
}
} // namespace

void CMapHeightTool::RenderControls()
{
    int tool = static_cast<int>(m_tool);
    ImGui::RadioButton("Raise / lower", &tool, static_cast<int>(Tool::RaiseLower));
    ImGui::SameLine();
    ImGui::RadioButton("Flatten", &tool, static_cast<int>(Tool::Flatten));
    ImGui::SameLine();
    ImGui::RadioButton("Smooth", &tool, static_cast<int>(Tool::Smooth));
    ImGui::SameLine();
    ImGui::RadioButton("Set height", &tool, static_cast<int>(Tool::SetHeight));
    m_tool = static_cast<Tool>(tool);
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", ToolHint(m_tool));

    Editor::BrushControls::RenderRadius(m_radius);
    Editor::BrushControls::RenderStrength(m_strength, STRENGTH_RANGE, "%.0f");
    if (m_tool == Tool::SetHeight)
    {
        ImGui::SetNextItemWidth(TARGET_SLIDER_WIDTH);
        ImGui::SliderFloat("Target height", &m_targetHeight, 0.0f, MaxHeight(), "%.0f");
    }
    ImGui::Checkbox("Objects follow terrain", &m_followObjects);
    ImGui::SetItemTooltip("Objects standing where the ground moves keep their height above it.");
}

float CMapHeightTool::Rate() const
{
    return m_strength / STRENGTH_RANGE.max;
}

bool CMapHeightTool::SamplesTarget(const TerrainBrushInput& input)
{
    if (m_tool != Tool::SetHeight || !input.altDown)
        return false;
    if (input.leftDown)
        m_targetHeight = Editor::ObjectPlace::GroundHeightAt(input.groundX, input.groundY);
    return true;
}

void CMapHeightTool::Apply(const TerrainBrushInput& input)
{
    Editor::BrushControls::ApplyKeys(m_radius, &m_strength, STRENGTH_RANGE);
    if (!input.onGround)
    {
        FinishStroke();
        return;
    }
    Editor::BrushControls::ShowOutline(input, m_radius, true, OUTLINE_COLOR);

    // Alt-click in Set height reads the ground; it is not an edit.
    const bool lower = input.rightDown && m_tool == Tool::RaiseLower;
    if (SamplesTarget(input) || (!input.leftDown && !lower))
    {
        FinishStroke();
        return;
    }

    if (!m_stroke.IsActive())
        BeginStroke(input, lower);

    const BrushCircle circle = Editor::BrushControls::CircleAt(input, m_radius);
    if (m_followObjects)
        m_followers.BeforeBrush(Editor::Editing::Footprint(circle, TERRAIN_SIZE, TERRAIN_SIZE));
    const CellRect changed = Sculpt(circle, lower);
    Editor::Editing::ClampField(Heights(), changed, 0.0f, MaxHeight());
    CMapTerrainLayers::RelightHeights(changed);
    if (m_followObjects)
        m_followers.AfterBrush();
}

void CMapHeightTool::BeginStroke(const TerrainBrushInput& input, bool lower)
{
    m_strokeLabel = StrokeLabel(m_tool, lower);
    m_flattenHeight = Editor::ObjectPlace::GroundHeightAt(input.groundX, input.groundY);
    m_stroke.Begin(g_MapEditHistory.Terrain(), HEIGHT_LAYERS, m_strokeLabel);
}

CellRect CMapHeightTool::Sculpt(const BrushCircle& circle, bool lower)
{
    switch (m_tool)
    {
    case Tool::Flatten:
        return Editor::Editing::MoveFieldToward(Heights(), circle, &m_flattenHeight, Rate());
    case Tool::Smooth:
        return Editor::Editing::SmoothField(Heights(), circle, Rate());
    case Tool::SetHeight:
        return Editor::Editing::MoveFieldToward(Heights(), circle, &m_targetHeight, Rate());
    default:
    {
        const float amount = lower ? -m_strength : m_strength;
        return Editor::Editing::AddToField(Heights(), circle, &amount);
    }
    }
}

void CMapHeightTool::FinishStroke()
{
    if (!m_stroke.IsActive())
        return;
    std::vector<std::unique_ptr<Editor::Editing::EditCommand>> parts;
    parts.push_back(m_stroke.Finish());
    parts.push_back(m_followers.Finish());
    g_MapEditHistory.Push(Editor::Editing::GroupEdits(m_strokeLabel, std::move(parts)));
}

void CMapHeightTool::CancelStroke()
{
    m_stroke.Cancel();
    m_followers.Cancel();
}

bool CMapHeightTool::IsStrokeActive() const
{
    return m_stroke.IsActive();
}

#endif // _EDITOR
