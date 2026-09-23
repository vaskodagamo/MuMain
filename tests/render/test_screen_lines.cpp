#include <doctest.h>

#include "Render/Renderer/LineTopology.h"
#include "Render/Renderer/ScreenLineRibbons.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

using Render::Lines::AppendRibbons;
using Render::Lines::ScreenLineView;
using Render::Topology::ForEachLineSegment;
using Render::Topology::LineMode;

namespace
{
constexpr float VIEWPORT_WIDTH = 1280.0f;
constexpr float VIEWPORT_HEIGHT = 720.0f;
constexpr float FIELD_OF_VIEW_DEGREES = 60.0f;
constexpr float NEAR_PLANE = 10.0f;
constexpr float FAR_PLANE = 50000.0f;
constexpr float PIXEL_TOLERANCE = 0.01f;
constexpr std::uint32_t COLOR_A = 0xFF0000FFu;
constexpr std::uint32_t COLOR_B = 0xFF00FF00u;

std::vector<std::pair<std::size_t, std::size_t>> Segments(LineMode mode, std::size_t count)
{
    std::vector<std::pair<std::size_t, std::size_t>> segments;
    ForEachLineSegment(mode, count, [&segments](std::size_t a, std::size_t b) { segments.emplace_back(a, b); });
    return segments;
}

ScreenLineView ViewFrom(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up)
{
    ScreenLineView view;
    view.modelView = glm::lookAt(eye, target, up);
    view.projection =
        glm::perspective(glm::radians(FIELD_OF_VIEW_DEGREES), VIEWPORT_WIDTH / VIEWPORT_HEIGHT, NEAR_PLANE, FAR_PLANE);
    view.viewportWidth = VIEWPORT_WIDTH;
    view.viewportHeight = VIEWPORT_HEIGHT;
    return view;
}

// The MU client's usual view: 45 degrees down onto the ground from 1500 units away.
ScreenLineView GameView()
{
    return ViewFrom({0.0f, -1060.0f, 1060.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
}

mu::Vertex3D At(float x, float y, float z, std::uint32_t color)
{
    return {x, y, z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, color};
}

// A view-space corner of a ribbon in window pixels.
glm::vec2 ToPixels(const ScreenLineView& view, const mu::Vertex3D& corner)
{
    const glm::vec4 clip = view.projection * glm::vec4(corner.x, corner.y, corner.z, 1.0f);
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return {(ndc.x + 1.0f) * VIEWPORT_WIDTH * 0.5f, (ndc.y + 1.0f) * VIEWPORT_HEIGHT * 0.5f};
}

// Width on screen at the ribbon's first end (corners 0 and 3) and at its second (1 and 2).
std::pair<float, float> EndWidths(const ScreenLineView& view, const std::vector<mu::Vertex3D>& quad)
{
    return {glm::length(ToPixels(view, quad[3]) - ToPixels(view, quad[0])),
            glm::length(ToPixels(view, quad[2]) - ToPixels(view, quad[1]))};
}

std::vector<mu::Vertex3D> Ribbons(const ScreenLineView& view, std::vector<mu::Vertex3D> segments, float width)
{
    std::vector<mu::Vertex3D> quads;
    AppendRibbons(segments, view, width, quads);
    return quads;
}
} // namespace

TEST_CASE("GL line modes break into the segments OpenGL draws [render][lines]")
{
    using Pairs = std::vector<std::pair<std::size_t, std::size_t>>;
    CHECK(Segments(LineMode::Lines, 5) == Pairs{{0, 1}, {2, 3}});
    CHECK(Segments(LineMode::LineStrip, 4) == Pairs{{0, 1}, {1, 2}, {2, 3}});
    CHECK(Segments(LineMode::LineLoop, 4) == Pairs{{0, 1}, {1, 2}, {2, 3}, {3, 0}});
    CHECK(Segments(LineMode::LineLoop, 2) == Pairs{{0, 1}});
    CHECK(Segments(LineMode::LineStrip, 1).empty());
    CHECK(Segments(LineMode::LineLoop, 0).empty());
}

TEST_CASE("A ribbon is the given number of pixels wide near and far [render][lines]")
{
    const ScreenLineView view = GameView();
    constexpr float WIDTH = 2.0f;
    // A ground line running away from the camera: its far end is much further away.
    const std::vector<mu::Vertex3D> quads =
        Ribbons(view, {At(-300.0f, -600.0f, 0.0f, COLOR_A), At(200.0f, 2500.0f, 0.0f, COLOR_B)}, WIDTH);
    REQUIRE(quads.size() == 4);
    const auto [nearWidth, farWidth] = EndWidths(view, quads);
    CHECK(nearWidth == doctest::Approx(WIDTH).epsilon(PIXEL_TOLERANCE));
    CHECK(farWidth == doctest::Approx(WIDTH).epsilon(PIXEL_TOLERANCE));
    CHECK(quads[0].color == COLOR_A);
    CHECK(quads[3].color == COLOR_A);
    CHECK(quads[1].color == COLOR_B);
    CHECK(quads[2].color == COLOR_B);
}

TEST_CASE("A ground line seen from straight above is still a visible ribbon [render][lines]")
{
    // The old RenderLines widened a level line along the world's Z axis: edge-on from above.
    const ScreenLineView view = ViewFrom({0.0f, 0.0f, 3000.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    constexpr float WIDTH = 3.0f;
    const std::vector<mu::Vertex3D> quads =
        Ribbons(view, {At(-500.0f, 100.0f, 0.0f, COLOR_A), At(500.0f, 100.0f, 0.0f, COLOR_A)}, WIDTH);
    REQUIRE(quads.size() == 4);
    const auto [firstWidth, secondWidth] = EndWidths(view, quads);
    CHECK(firstWidth == doctest::Approx(WIDTH).epsilon(PIXEL_TOLERANCE));
    CHECK(secondWidth == doctest::Approx(WIDTH).epsilon(PIXEL_TOLERANCE));
}

TEST_CASE("Segments are cut in front of the camera and dropped behind it [render][lines]")
{
    const ScreenLineView view = GameView();
    // From in front of the camera to behind it (the camera looks towards +Y).
    const std::vector<mu::Vertex3D> crossing =
        Ribbons(view, {At(0.0f, 0.0f, 0.0f, COLOR_A), At(0.0f, -3000.0f, 1200.0f, COLOR_B)}, 2.0f);
    REQUIRE(crossing.size() == 4);
    for (const mu::Vertex3D& corner : crossing)
        CHECK(-corner.z >= Render::Lines::NEAR_CUT_DISTANCE - 1.0e-3f);

    const std::vector<mu::Vertex3D> behind =
        Ribbons(view, {At(0.0f, -3000.0f, 1000.0f, COLOR_A), At(100.0f, -3200.0f, 1100.0f, COLOR_B)}, 2.0f);
    CHECK(behind.empty());
}

TEST_CASE("A segment pointing at the camera, or a zero width, draws nothing [render][lines]")
{
    const ScreenLineView view = GameView();
    const glm::vec3 eye(0.0f, -1060.0f, 1060.0f);
    const glm::vec3 far = eye + (glm::vec3(0.0f) - eye) * 2.0f;
    CHECK(Ribbons(view, {At(0.0f, 0.0f, 0.0f, COLOR_A), At(far.x, far.y, far.z, COLOR_A)}, 2.0f).empty());
    CHECK(Ribbons(view, {At(0.0f, 0.0f, 0.0f, COLOR_A), At(100.0f, 0.0f, 0.0f, COLOR_A)}, 0.0f).empty());

    ScreenLineView noViewport = view;
    noViewport.viewportHeight = 0.0f;
    CHECK(Ribbons(noViewport, {At(0.0f, 0.0f, 0.0f, COLOR_A), At(100.0f, 0.0f, 0.0f, COLOR_A)}, 2.0f).empty());
}
