#pragma once

#ifdef _EDITOR

#include "Render/Renderer/MuRenderer.h" // mu::Vertex3D

#include <cstdint>
#include <vector>

// The outline of the Map Editor's round brushes on the ground: a circle that follows
// the terrain around the cursor, drawn in the terrain's edit pass.
namespace Render::Terrain::BrushOutline
{
// A round brush on the ground, in world units.
struct Circle
{
    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 0.0f;
    // Where the brush starts to fade towards its rim, drawn as a fainter ring;
    // 0 for a brush with a hard edge.
    float innerRadius = 0.0f;
    std::uint32_t color = 0; // packed ABGR
};

// Draws `circle` from the next terrain edit pass on, until Hide. The Map Editor sets
// it each frame its brush is over the ground.
void Show(const Circle& circle);
void Hide();

// Draws the circle shown now, if any (RenderTerrain's edit pass).
void Render();

// Appends line segments from (x0, y0) to (x1, y1) that follow the ground `lift` above
// it, one segment per quarter tile.
void AppendGroundLine(std::vector<mu::Vertex3D>& lines, float x0, float y0, float x1, float y1, float lift,
                      std::uint32_t color);
} // namespace Render::Terrain::BrushOutline

#endif // _EDITOR
