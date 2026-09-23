#include <doctest.h>

#include "Editing/ThumbnailFraming.h"

#include <algorithm>
#include <cmath>

using namespace Editor::Thumbnail;
using Editor::Gizmo::Vec3;

namespace
{
constexpr float PI = 3.14159265f;

struct Picture
{
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    bool allInFront = true;
};

Vec3 Unit(const Vec3& v)
{
    return v * (1.0f / Editor::Gizmo::Length(v));
}

// Where the 8 corners of the box land in the square picture (-1..1 on both axes),
// through the camera as LoadLookAt + gluPerspective set it up.
Picture Project(const Camera& camera, const Vec3& boxMin, const Vec3& boxMax)
{
    const Vec3 forward = Unit(camera.center - camera.eye);
    const Vec3 right = Unit(Editor::Gizmo::Cross(forward, camera.up));
    const Vec3 up = Editor::Gizmo::Cross(right, forward);
    const float tanHalf = std::tan(camera.fovDegrees * 0.5f * PI / 180.0f);
    Picture picture;
    for (int i = 0; i < 8; ++i)
    {
        const Vec3 corner{(i & 1) ? boxMax.x : boxMin.x, (i & 2) ? boxMax.y : boxMin.y,
                          (i & 4) ? boxMax.z : boxMin.z};
        const Vec3 offset = corner - camera.eye;
        const float depth = Editor::Gizmo::Dot(offset, forward);
        picture.allInFront = picture.allInFront && depth > camera.zNear && depth < camera.zFar;
        const float x = Editor::Gizmo::Dot(offset, right) / (depth * tanHalf);
        const float y = Editor::Gizmo::Dot(offset, up) / (depth * tanHalf);
        picture.minX = std::min(picture.minX, x);
        picture.maxX = std::max(picture.maxX, x);
        picture.minY = std::min(picture.minY, y);
        picture.maxY = std::max(picture.maxY, y);
    }
    return picture;
}

// The share of the picture's width or height the box covers, whichever is larger.
float Fill(const Picture& picture)
{
    return std::max(picture.maxX - picture.minX, picture.maxY - picture.minY) / 2.0f;
}

bool Inside(const Picture& picture)
{
    return picture.minX >= -1.0f && picture.maxX <= 1.0f && picture.minY >= -1.0f && picture.maxY <= 1.0f;
}
} // namespace

TEST_CASE("item framing fills the picture with a long spear and with a small ring [editor][thumbnail]")
{
    // Sword20.bmd's bind-pose bounds: thin along X, the blade along -Y.
    const Vec3 swordMin{-5.42f, -193.22f, -34.67f};
    const Vec3 swordMax{5.43f, 45.55f, 34.71f};
    const Vec3 spearMin{-4.0f, -380.0f, -6.0f};
    const Vec3 spearMax{4.0f, 60.0f, 6.0f};
    const Vec3 ringMin{-3.0f, -0.6f, -3.0f};
    const Vec3 ringMax{3.0f, 0.6f, 3.0f};
    const Vec3 helmMin{-18.0f, -20.0f, 150.0f};
    const Vec3 helmMax{18.0f, 16.0f, 190.0f};

    for (const auto& [boxMin, boxMax] : {std::pair{swordMin, swordMax}, std::pair{spearMin, spearMax},
                                         std::pair{ringMin, ringMax}, std::pair{helmMin, helmMax}})
    {
        const Camera camera = FrameBounds(boxMin, boxMax, Framing::Item);
        const Picture picture = Project(camera, boxMin, boxMax);
        CHECK(picture.allInFront);
        CHECK(Inside(picture));
        CHECK(Fill(picture) > 0.75f);
    }
}

TEST_CASE("a long item runs corner to corner, its tip at the top [editor][thumbnail]")
{
    const Vec3 spearMin{-4.0f, -380.0f, -6.0f};
    const Vec3 spearMax{4.0f, 60.0f, 6.0f};
    const Camera camera = FrameBounds(spearMin, spearMax, Framing::Item);
    const Vec3 tip{0.0f, -380.0f, 0.0f};
    const Picture tipPicture = Project(camera, tip, tip);
    CHECK(tipPicture.maxY > 0.5f);                 // top half
    CHECK(std::fabs(tipPicture.maxX) > 0.5f);      // towards a corner, not the middle
}

TEST_CASE("object framing keeps the Map Editor's roomy 3/4 view [editor][thumbnail]")
{
    const Vec3 treeMin{-100.0f, -100.0f, 0.0f};
    const Vec3 treeMax{100.0f, 100.0f, 400.0f};
    const Camera camera = FrameBounds(treeMin, treeMax, Framing::Object);
    CHECK(camera.fovDegrees == doctest::Approx(35.0f));
    CHECK(camera.up.z == doctest::Approx(1.0f));
    const Picture picture = Project(camera, treeMin, treeMax);
    CHECK(Inside(picture));
    CHECK(Fill(picture) < 0.75f);

    // A degenerate box frames a typical object around (0, 0, 80).
    const Camera fallback = FrameBounds(Vec3{}, Vec3{}, Framing::Object);
    CHECK(fallback.center.z == doctest::Approx(80.0f));
}
