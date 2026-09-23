#include "Render/Renderer/ScreenLineRibbons.h"

#include <cmath>
#include <cstdint>

namespace Render::Lines
{
namespace
{
// Shorter than this on screen, a segment points (almost) straight at the camera and
// has no side to widen it to.
constexpr float MIN_SCREEN_LENGTH_PIXELS = 1.0e-3f;
constexpr float HALF = 0.5f;

struct ViewPoint
{
    glm::vec3 position;
    std::uint32_t color;
};

ViewPoint ToView(const glm::mat4& modelView, const mu::Vertex3D& vertex)
{
    const glm::vec4 view = modelView * glm::vec4(vertex.x, vertex.y, vertex.z, 1.0f);
    return {glm::vec3(view), vertex.color};
}

// OpenGL view space looks down -Z.
float DepthOf(const ViewPoint& point)
{
    return -point.position.z;
}

// Moves the end of the segment that lies closer than NEAR_CUT_DISTANCE onto that
// distance. False when the whole segment is closer.
bool CutAtNear(ViewPoint& a, ViewPoint& b)
{
    const float depthA = DepthOf(a);
    const float depthB = DepthOf(b);
    if (depthA < NEAR_CUT_DISTANCE && depthB < NEAR_CUT_DISTANCE)
    {
        return false;
    }
    if (depthA >= NEAR_CUT_DISTANCE && depthB >= NEAR_CUT_DISTANCE)
    {
        return true;
    }

    const float t = (NEAR_CUT_DISTANCE - depthA) / (depthB - depthA);
    const glm::vec3 cut = a.position + (b.position - a.position) * t;
    if (depthA < NEAR_CUT_DISTANCE)
    {
        a.position = cut;
    }
    else
    {
        b.position = cut;
    }
    return true;
}

glm::vec4 ToClip(const ScreenLineView& view, const ViewPoint& point)
{
    return view.projection * glm::vec4(point.position, 1.0f);
}

// The segment's direction on screen, in pixels.
glm::vec2 ScreenDirection(const ScreenLineView& view, const glm::vec4& clipA, const glm::vec4& clipB)
{
    const glm::vec2 ndcA = glm::vec2(clipA) / clipA.w;
    const glm::vec2 ndcB = glm::vec2(clipB) / clipB.w;
    const glm::vec2 pixelsPerNdc(view.viewportWidth * HALF, view.viewportHeight * HALF);
    return (ndcB - ndcA) * pixelsPerNdc;
}

// The view-space offset that moves a point with clip-space w by `sideNdc` on screen.
// A standard projection maps a view-space x/y step to clip x/y through its diagonal
// alone and leaves w unchanged, so the step is the screen offset scaled back by w.
glm::vec3 SideOffset(const ScreenLineView& view, const glm::vec2& sideNdc, float clipW)
{
    return {sideNdc.x * clipW / view.projection[0][0], sideNdc.y * clipW / view.projection[1][1], 0.0f};
}

mu::Vertex3D ToVertex(const glm::vec3& position, std::uint32_t color)
{
    return {position.x, position.y, position.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, color};
}

void AppendRibbon(ViewPoint a, ViewPoint b, const ScreenLineView& view, float widthPixels,
                  std::vector<mu::Vertex3D>& quads)
{
    if (!CutAtNear(a, b))
    {
        return;
    }

    const glm::vec4 clipA = ToClip(view, a);
    const glm::vec4 clipB = ToClip(view, b);
    const glm::vec2 direction = ScreenDirection(view, clipA, clipB);
    const float length = glm::length(direction);
    if (length < MIN_SCREEN_LENGTH_PIXELS)
    {
        return;
    }

    const glm::vec2 sidePixels = glm::vec2(-direction.y, direction.x) * (widthPixels * HALF / length);
    const glm::vec2 sideNdc = sidePixels / glm::vec2(view.viewportWidth * HALF, view.viewportHeight * HALF);
    const glm::vec3 offsetA = SideOffset(view, sideNdc, clipA.w);
    const glm::vec3 offsetB = SideOffset(view, sideNdc, clipB.w);

    quads.push_back(ToVertex(a.position - offsetA, a.color));
    quads.push_back(ToVertex(b.position - offsetB, b.color));
    quads.push_back(ToVertex(b.position + offsetB, b.color));
    quads.push_back(ToVertex(a.position + offsetA, a.color));
}
} // namespace

void AppendRibbons(std::span<const mu::Vertex3D> segments, const ScreenLineView& view, float widthPixels,
                   std::vector<mu::Vertex3D>& quads)
{
    const bool projects = view.projection[0][0] != 0.0f && view.projection[1][1] != 0.0f;
    if (widthPixels <= 0.0f || view.viewportWidth <= 0.0f || view.viewportHeight <= 0.0f || !projects)
    {
        return;
    }

    for (std::size_t i = 0; i + 1 < segments.size(); i += 2)
    {
        AppendRibbon(ToView(view.modelView, segments[i]), ToView(view.modelView, segments[i + 1]), view, widthPixels,
                     quads);
    }
}
} // namespace Render::Lines
