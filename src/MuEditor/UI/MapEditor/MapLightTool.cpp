#include "stdafx.h"

#ifdef _EDITOR

#include "MapLightTool.h"

#include "MapEditHistory.h"
#include "MapLightSave.h"
#include "MapTerrainLayers.h"

#include "Editing/FieldBrush.h"
#include "Render/Renderer/RenderUtils.h"  // mu::PackABGR
#include "Render/Terrain/ZzzLodTerrain.h" // TerrainLight

#include "imgui.h"

#include <vector>

using Editor::Editing::BrushCircle;
using Editor::Editing::CellRect;
using Editor::Editing::FloatField;

namespace
{
constexpr int LIGHT_CHANNELS = 3;
// How much one frame adds or takes away at the brush's core, as a share of the colour.
constexpr Editor::BrushControls::Range INTENSITY_RANGE = {0.01f, 1.0f, 0.01f};
const std::uint32_t OUTLINE_COLOR = mu::PackABGR(0.55f, 0.9f, 1.0f, 0.95f);
const std::vector<int> LIGHT_LAYERS = {MAP_LAYER_LIGHT};

FloatField LightMap()
{
    return {&TerrainLight[0][0], TERRAIN_SIZE, TERRAIN_SIZE, LIGHT_CHANNELS};
}

const char* StrokeLabel(CMapLightTool::Mode mode)
{
    switch (mode)
    {
    case CMapLightTool::Mode::Subtract:
        return "Darken light";
    case CMapLightTool::Mode::Tint:
        return "Tint light";
    case CMapLightTool::Mode::Smooth:
        return "Smooth light";
    default:
        return "Add light";
    }
}
} // namespace

void CMapLightTool::RenderControls()
{
    int mode = static_cast<int>(m_mode);
    ImGui::RadioButton("Add", &mode, static_cast<int>(Mode::Add));
    ImGui::SameLine();
    ImGui::RadioButton("Subtract", &mode, static_cast<int>(Mode::Subtract));
    ImGui::SameLine();
    ImGui::RadioButton("Tint", &mode, static_cast<int>(Mode::Tint));
    ImGui::SameLine();
    ImGui::RadioButton("Smooth", &mode, static_cast<int>(Mode::Smooth));
    m_mode = static_cast<Mode>(mode);
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Left-click paints in this mode, right-click smooths.");

    ImGui::ColorEdit3("Colour", m_color, ImGuiColorEditFlags_NoInputs);
    Editor::BrushControls::RenderStrength(m_intensity, INTENSITY_RANGE, "%.2f");
    Editor::BrushControls::RenderRadius(m_radius);
}

void CMapLightTool::Apply(const TerrainBrushInput& input)
{
    Editor::BrushControls::ApplyKeys(m_radius, &m_intensity, INTENSITY_RANGE);
    if (!input.onGround)
    {
        FinishStroke();
        return;
    }
    Editor::BrushControls::ShowOutline(input, m_radius, true, OUTLINE_COLOR);
    if (!input.leftDown && !input.rightDown)
    {
        FinishStroke();
        return;
    }

    const Mode mode = input.rightDown ? Mode::Smooth : m_mode;
    if (!m_stroke.IsActive())
        m_stroke.Begin(g_MapEditHistory.Terrain(), LIGHT_LAYERS, StrokeLabel(mode));

    const CellRect changed = Paint(Editor::BrushControls::CircleAt(input, m_radius), mode);
    Editor::Editing::ClampField(LightMap(), changed, 0.0f, 1.0f);
    CMapTerrainLayers::RelightCells(changed);
}

CellRect CMapLightTool::Paint(const BrushCircle& circle, Mode mode)
{
    if (mode == Mode::Smooth)
        return Editor::Editing::SmoothField(LightMap(), circle, m_intensity);
    if (mode == Mode::Tint)
        return Editor::Editing::MoveFieldToward(LightMap(), circle, m_color, m_intensity);

    const float sign = (mode == Mode::Subtract) ? -1.0f : 1.0f;
    float amount[LIGHT_CHANNELS];
    for (int c = 0; c < LIGHT_CHANNELS; ++c)
        amount[c] = m_color[c] * m_intensity * sign;
    return Editor::Editing::AddToField(LightMap(), circle, amount);
}

void CMapLightTool::Save(int world)
{
    FinishStroke();
    Editor::LightSave::Save(world, m_status);
}

void CMapLightTool::FinishStroke()
{
    g_MapEditHistory.Push(m_stroke.Finish());
}

void CMapLightTool::CancelStroke()
{
    m_stroke.Cancel();
}

bool CMapLightTool::IsStrokeActive() const
{
    return m_stroke.IsActive();
}

#endif // _EDITOR
