#include "stdafx.h"

#ifdef _EDITOR

#include "MapBrushControls.h"

#include "MapEditorShortcuts.h"

#include "Render/Terrain/TerrainBrushOutline.h"

#include "imgui.h"

#include <algorithm>

namespace Editor::BrushControls
{
namespace
{
constexpr float SLIDER_WIDTH = 200.0f;

void Step(float& value, int direction, const Range& range)
{
    if (direction == 0)
        return;
    value = std::clamp(value + static_cast<float>(direction) * range.step, range.min, range.max);
}
} // namespace

void RenderRadius(float& radius)
{
    ImGui::SetNextItemWidth(SLIDER_WIDTH);
    ImGui::SliderFloat("Radius", &radius, RADIUS_RANGE.min, RADIUS_RANGE.max, "%.1f tiles");
    ImGui::SameLine();
    ImGui::TextDisabled("[ ]");
}

void RenderStrength(float& strength, const Range& range, const char* format)
{
    ImGui::SetNextItemWidth(SLIDER_WIDTH);
    ImGui::SliderFloat("Strength", &strength, range.min, range.max, format);
    ImGui::SameLine();
    ImGui::TextDisabled("Shift+[ ]");
}

void ApplyKeys(float& radius, float* strength, const Range& strengthRange)
{
    Step(radius, Editor::Shortcuts::BrushRadiusStep(), RADIUS_RANGE);
    if (strength != nullptr)
        Step(*strength, Editor::Shortcuts::BrushStrengthStep(), strengthRange);
}

Editor::Editing::BrushCircle CircleAt(const TerrainBrushInput& input, float radius)
{
    return {input.groundX / TERRAIN_SCALE, input.groundY / TERRAIN_SCALE, radius};
}

void ShowOutline(const TerrainBrushInput& input, float radius, bool soft, std::uint32_t color)
{
    if (!input.onGround)
        return;
    Render::Terrain::BrushOutline::Circle circle;
    circle.centerX = input.groundX;
    circle.centerY = input.groundY;
    circle.radius = radius * TERRAIN_SCALE;
    circle.innerRadius = soft ? circle.radius * Editor::Editing::FALLOFF_START : 0.0f;
    circle.color = color;
    Render::Terrain::BrushOutline::Show(circle);
}
} // namespace Editor::BrushControls

#endif // _EDITOR
