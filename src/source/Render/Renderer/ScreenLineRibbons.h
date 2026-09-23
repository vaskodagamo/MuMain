#pragma once

#include "Render/Renderer/MuRenderer.h" // mu::Vertex3D

#include <span>
#include <vector>

#include <glm/glm.hpp>

// Lines of a constant width on screen. The renderer only draws triangles, so each
// segment becomes a quad (a "ribbon") whose sides are moved apart parallel to the
// screen, by half the width in pixels at each end's depth: the ribbon faces the
// camera from any angle (also straight down) and keeps its width in pixels near
// and far. The quads come out in view space, so they are drawn with the projection
// matrix alone.
namespace Render::Lines
{
// Segments are cut this far in front of the camera: the 3D vertex shader drops
// everything with clip-space w below 1 (basic_textured.vert.hlsl), and a ribbon
// needs both ends in front of the camera to know which way the line runs on screen.
constexpr float NEAR_CUT_DISTANCE = 1.0f;

// The camera the lines are drawn with. `projection` is a perspective or
// orthographic matrix as glm (and gluPerspective) build them; the viewport size is
// in the pixels the width is given in.
struct ScreenLineView
{
    glm::mat4 modelView{1.0f};
    glm::mat4 projection{1.0f};
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
};

// Appends one quad (four view-space vertices in perimeter order, the colours of
// the segment's ends) for each segment of `segments` (pairs of vertices, an odd
// last vertex is ignored), `widthPixels` wide on screen. A segment wholly behind
// the camera, or pointing straight at it, adds nothing.
void AppendRibbons(std::span<const mu::Vertex3D> segments, const ScreenLineView& view, float widthPixels,
                   std::vector<mu::Vertex3D>& quads);
} // namespace Render::Lines
