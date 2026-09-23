#include "stdafx.h"

#ifdef _EDITOR

#include "TerrainGroundRects.h"

#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/QuadTopology.h"
#include "Render/Terrain/TerrainBrushOutline.h" // AppendGroundLine
#include "Render/Terrain/TerrainOverlayState.h"
#include "Render/Terrain/ZzzLodTerrain.h"

#include <algorithm>
#include <span>

namespace Render::Terrain::GroundRects
{
namespace
{
// Above the attribute overlay (3 units) and below the brush outline (5).
constexpr float FILL_LIFT = 4.0f;
constexpr float OUTLINE_LIFT = 4.5f;
constexpr float OUTLINE_WIDTH_PIXELS = 2.5f;
constexpr int LAST_TILE = TERRAIN_SIZE - 1;

std::vector<Rect> s_rects;

mu::Vertex3D GroundVertex(float x, float y, std::uint32_t color)
{
    return {x, y, RequestTerrainHeight(x, y) + FILL_LIFT, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, color};
}

void AppendTile(std::vector<mu::Vertex3D>& quads, int xi, int yi, std::uint32_t color)
{
    const float sx = static_cast<float>(xi) * TERRAIN_SCALE;
    const float sy = static_cast<float>(yi) * TERRAIN_SCALE;
    quads.push_back(GroundVertex(sx, sy, color));
    quads.push_back(GroundVertex(sx + TERRAIN_SCALE, sy, color));
    quads.push_back(GroundVertex(sx + TERRAIN_SCALE, sy + TERRAIN_SCALE, color));
    quads.push_back(GroundVertex(sx, sy + TERRAIN_SCALE, color));
}

// The rectangle clipped to the map; false when nothing of it is on the map.
bool Clip(const Rect& rect, Rect& clipped)
{
    clipped = rect;
    clipped.x1 = std::max(std::min(rect.x1, rect.x2), 0);
    clipped.y1 = std::max(std::min(rect.y1, rect.y2), 0);
    clipped.x2 = std::min(std::max(rect.x1, rect.x2), LAST_TILE);
    clipped.y2 = std::min(std::max(rect.y1, rect.y2), LAST_TILE);
    return clipped.x1 <= clipped.x2 && clipped.y1 <= clipped.y2;
}

void AppendRect(const Rect& rect, std::vector<mu::Vertex3D>& quads, std::vector<mu::Vertex3D>& lines)
{
    for (int yi = rect.y1; yi <= rect.y2; ++yi)
        for (int xi = rect.x1; xi <= rect.x2; ++xi)
            AppendTile(quads, xi, yi, rect.fill);

    const float x0 = static_cast<float>(rect.x1) * TERRAIN_SCALE;
    const float y0 = static_cast<float>(rect.y1) * TERRAIN_SCALE;
    const float x1 = static_cast<float>(rect.x2 + 1) * TERRAIN_SCALE;
    const float y1 = static_cast<float>(rect.y2 + 1) * TERRAIN_SCALE;
    BrushOutline::AppendGroundLine(lines, x0, y0, x1, y0, OUTLINE_LIFT, rect.outline);
    BrushOutline::AppendGroundLine(lines, x1, y0, x1, y1, OUTLINE_LIFT, rect.outline);
    BrushOutline::AppendGroundLine(lines, x1, y1, x0, y1, OUTLINE_LIFT, rect.outline);
    BrushOutline::AppendGroundLine(lines, x0, y1, x0, y0, OUTLINE_LIFT, rect.outline);
}
} // namespace

void Show(const std::vector<Rect>& rects)
{
    s_rects = rects;
}

void Hide()
{
    s_rects.clear();
}

void Render()
{
    if (s_rects.empty())
        return;

    thread_local std::vector<mu::Vertex3D> quads;
    thread_local std::vector<mu::Vertex3D> lines;
    quads.clear();
    lines.clear();
    for (const Rect& rect : s_rects)
    {
        Rect clipped;
        if (Clip(rect, clipped))
            AppendRect(clipped, quads, lines);
    }

    const TerrainOverlayState overlayState;
    Render::Topology::ForEachQuadBatch(std::span<const mu::Vertex3D>(quads), Render::Topology::MAX_QUADS_PER_DRAW,
                                       [](std::span<const mu::Vertex3D> batch)
                                       { mu::GetRenderer().RenderQuad3D(batch, 0u); });
    mu::GetRenderer().RenderScreenLines(lines, OUTLINE_WIDTH_PIXELS);
}
} // namespace Render::Terrain::GroundRects

#endif // _EDITOR
