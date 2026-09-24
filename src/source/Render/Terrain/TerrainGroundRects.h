#pragma once

#ifdef _EDITOR

#include <cstdint>
#include <vector>

// Rectangles of tiles marked on the ground, such as the Map Editor's gate areas: a
// translucent fill over each tile and an outline, following the terrain, drawn after the
// attribute overlay in the terrain's normal pass.
namespace Render::Terrain::GroundRects
{
struct Rect
{
    int x1 = 0; // tiles, both corners included
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    std::uint32_t fill = 0;    // packed ABGR
    std::uint32_t outline = 0; // packed ABGR
};

// Draws `rects` from the next frame on, until Hide. The Map Editor sets them each frame
// its Gates tab is shown.
void Show(const std::vector<Rect>& rects);
void Hide();

// Draws the rectangles shown now, if any (RenderTerrain).
void Render();
} // namespace Render::Terrain::GroundRects

#endif // _EDITOR
