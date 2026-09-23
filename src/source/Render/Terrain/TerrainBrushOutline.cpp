#include "stdafx.h"

#ifdef _EDITOR

#include "TerrainBrushOutline.h"

#include "Render/Terrain/TerrainOverlayState.h"
#include "Render/Terrain/ZzzLodTerrain.h"

#include <algorithm>
#include <cmath>

namespace Render::Terrain::BrushOutline
{
namespace
{
constexpr float FULL_TURN = 2.0f * Q_PI;
// Straight pieces of an outline are at most this long, so they follow the ground's
// slopes instead of cutting through hills (a quarter tile).
constexpr float MAX_SEGMENT_LENGTH = TERRAIN_SCALE * 0.25f;
constexpr int MIN_CIRCLE_SEGMENTS = 24;
constexpr int MAX_CIRCLE_SEGMENTS = 512;
// Above the ground (and the attribute overlay, 3 units) so the line does not flicker
// into the terrain.
constexpr float OUTLINE_LIFT = 5.0f;
constexpr float OUTLINE_WIDTH_PIXELS = 2.0f;
constexpr float INNER_RING_WIDTH_PIXELS = 1.0f;
// The inner ring is drawn at this share of the outline's opacity.
constexpr float INNER_RING_ALPHA_SCALE = 0.45f;

bool s_visible = false;
Circle s_circle;

mu::Vertex3D GroundVertex(float x, float y, float lift, std::uint32_t color)
{
    return {x, y, RequestTerrainHeight(x, y) + lift, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, color};
}

std::uint32_t ScaleAlpha(std::uint32_t color, float scale)
{
    constexpr std::uint32_t ALPHA_SHIFT = 24;
    constexpr std::uint32_t RGB_MASK = 0x00FFFFFFu;
    const auto alpha = static_cast<float>(color >> ALPHA_SHIFT) * scale;
    return (color & RGB_MASK) | (static_cast<std::uint32_t>(alpha) << ALPHA_SHIFT);
}

void AppendGroundCircle(std::vector<mu::Vertex3D>& lines, float centerX, float centerY, float radius,
                        std::uint32_t color)
{
    const auto wanted = static_cast<int>(std::ceil(FULL_TURN * radius / MAX_SEGMENT_LENGTH));
    const int segments = std::clamp(wanted, MIN_CIRCLE_SEGMENTS, MAX_CIRCLE_SEGMENTS);
    mu::Vertex3D previous = GroundVertex(centerX + radius, centerY, OUTLINE_LIFT, color);
    for (int i = 1; i <= segments; ++i)
    {
        const float angle = FULL_TURN * static_cast<float>(i) / static_cast<float>(segments);
        const mu::Vertex3D next =
            GroundVertex(centerX + radius * std::cos(angle), centerY + radius * std::sin(angle), OUTLINE_LIFT, color);
        lines.push_back(previous);
        lines.push_back(next);
        previous = next;
    }
}
} // namespace

void Show(const Circle& circle)
{
    s_circle = circle;
    s_visible = circle.radius > 0.0f;
}

void Hide()
{
    s_visible = false;
}

void Render()
{
    if (!s_visible)
        return;

    thread_local std::vector<mu::Vertex3D> outline;
    thread_local std::vector<mu::Vertex3D> innerRing;
    outline.clear();
    innerRing.clear();
    AppendGroundCircle(outline, s_circle.centerX, s_circle.centerY, s_circle.radius, s_circle.color);
    if (s_circle.innerRadius > 0.0f)
        AppendGroundCircle(innerRing, s_circle.centerX, s_circle.centerY, s_circle.innerRadius,
                           ScaleAlpha(s_circle.color, INNER_RING_ALPHA_SCALE));

    const TerrainOverlayState overlayState;
    mu::GetRenderer().RenderScreenLines(outline, OUTLINE_WIDTH_PIXELS);
    if (!innerRing.empty())
        mu::GetRenderer().RenderScreenLines(innerRing, INNER_RING_WIDTH_PIXELS);
}

void AppendGroundLine(std::vector<mu::Vertex3D>& lines, float x0, float y0, float x1, float y1, float lift,
                      std::uint32_t color)
{
    const float length = std::hypot(x1 - x0, y1 - y0);
    const int segments = std::max(1, static_cast<int>(std::ceil(length / MAX_SEGMENT_LENGTH)));
    mu::Vertex3D previous = GroundVertex(x0, y0, lift, color);
    for (int i = 1; i <= segments; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const mu::Vertex3D next = GroundVertex(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, lift, color);
        lines.push_back(previous);
        lines.push_back(next);
        previous = next;
    }
}
} // namespace Render::Terrain::BrushOutline

#endif // _EDITOR
