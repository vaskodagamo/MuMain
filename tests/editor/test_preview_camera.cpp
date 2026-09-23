#include <doctest.h>

#include "Editing/PreviewCamera.h"
#include "Editing/PreviewSlot.h"

#include <cmath>

using namespace Editor::Preview;
using Editor::Gizmo::Vec3;

namespace
{
constexpr float PI = 3.14159265f;
constexpr float EPSILON = 1e-3f;

float Radians(float degrees)
{
    return degrees * PI / 180.0f;
}

// `point` in eye space through the 3x4 camera rows (x right, y up, -z forward).
Vec3 ToEye(const float rows[3][4], const Vec3& point)
{
    const float in[3] = {point.x, point.y, point.z};
    float out[3];
    for (int row = 0; row < 3; ++row)
        out[row] = rows[row][0] * in[0] + rows[row][1] * in[1] + rows[row][2] * in[2] + rows[row][3];
    return {out[0], out[1], out[2]};
}
} // namespace

TEST_CASE("Preview orbit: drags turn and tilt within limits, the wheel zooms within limits")
{
    const Orbit start{0.0f, 10.0f, 1.0f};

    const Orbit right = Dragged(start, 100.0f, 0.0f);
    CHECK(right.yawDegrees > 180.0f); // turned the negative way, wrapped into [0, 360)
    CHECK(right.yawDegrees < 360.0f);
    CHECK(right.pitchDegrees == doctest::Approx(10.0f));

    const Orbit down = Dragged(start, 0.0f, 10000.0f);
    CHECK(down.pitchDegrees == doctest::Approx(MAX_PITCH_DEGREES));
    const Orbit up = Dragged(start, 0.0f, -10000.0f);
    CHECK(up.pitchDegrees == doctest::Approx(MIN_PITCH_DEGREES));

    CHECK(Zoomed(start, 1.0f).zoom > 1.0f);
    CHECK(Zoomed(start, -1.0f).zoom < 1.0f);
    CHECK(Zoomed(start, 100.0f).zoom == doctest::Approx(MAX_ZOOM));
    CHECK(Zoomed(start, -100.0f).zoom == doctest::Approx(MIN_ZOOM));
}

TEST_CASE("Preview orbit: the automatic turn goes round and wraps")
{
    Orbit orbit;
    orbit.yawDegrees = 350.0f;
    const Orbit turned = Turned(orbit, 1.0f);
    CHECK(turned.yawDegrees >= 0.0f);
    CHECK(turned.yawDegrees < 350.0f); // wrapped past 360
    CHECK(turned.pitchDegrees == orbit.pitchDegrees);
}

TEST_CASE("Preview orbit: Facing is the orbit OrbitView looks from")
{
    const Vec3 direction{-1.0f, 2.0f, 1.0f};
    const Orbit orbit = Facing(direction);
    const View view = OrbitView({10.0f, 20.0f, 30.0f}, 50.0f, orbit, 1.0f);
    const Vec3 fromCenter = view.eye - view.center;
    const float length = Editor::Gizmo::Length(fromCenter);
    const float directionLength = Editor::Gizmo::Length(direction);
    CHECK(fromCenter.x / length == doctest::Approx(direction.x / directionLength).epsilon(EPSILON));
    CHECK(fromCenter.y / length == doctest::Approx(direction.y / directionLength).epsilon(EPSILON));
    CHECK(fromCenter.z / length == doctest::Approx(direction.z / directionLength).epsilon(EPSILON));
}

TEST_CASE("Preview orbit: the subject's sphere stays in the picture from every side")
{
    const Vec3 center{100.0f, -50.0f, 20.0f};
    const float radius = 80.0f;
    for (const float aspect : {0.5f, 1.0f, 2.0f})
    {
        for (float yaw = 0.0f; yaw < 360.0f; yaw += 45.0f)
        {
            for (const float pitch : {-60.0f, 0.0f, 60.0f})
            {
                const View view = OrbitView(center, radius, {yaw, pitch, 1.0f}, aspect);
                float rows[3][4];
                LookAtRows(view, rows);
                const float tanHalf = std::tan(Radians(view.fovDegrees * 0.5f));
                // Six points of the sphere along the world axes.
                const Vec3 offsets[6] = {{radius, 0, 0}, {-radius, 0, 0}, {0, radius, 0},
                                         {0, -radius, 0}, {0, 0, radius}, {0, 0, -radius}};
                for (const Vec3& offset : offsets)
                {
                    const Vec3 eye = ToEye(rows, center + offset);
                    REQUIRE(eye.z < -view.zNear);
                    CHECK(std::fabs(eye.y) / -eye.z <= tanHalf);
                    CHECK(std::fabs(eye.x) / -eye.z <= tanHalf * aspect);
                }
                CHECK(Editor::Gizmo::Length(view.eye - center) + radius < view.zFar);
            }
        }
    }
}

TEST_CASE("Preview orbit: the column-major matrix is the 3x4 camera matrix")
{
    const View view = OrbitView({0.0f, 0.0f, 0.0f}, 10.0f, {30.0f, 20.0f, 1.0f}, 1.0f);
    float rows[3][4];
    float columns[16];
    LookAtRows(view, rows);
    LookAtColumnMajor(view, columns);
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
            CHECK(columns[column * 4 + row] == doctest::Approx(rows[row][column]));
    }
    CHECK(columns[3] == 0.0f);
    CHECK(columns[15] == 1.0f);
    // The centre lies straight ahead of the eye.
    const Vec3 center = ToEye(rows, view.center);
    CHECK(center.x == doctest::Approx(0.0f).epsilon(EPSILON));
    CHECK(center.y == doctest::Approx(0.0f).epsilon(EPSILON));
    CHECK(center.z < 0.0f);
}

TEST_CASE("Preview target size: steps, never below or above the limits")
{
    CHECK(TargetSize(0.0f) == MIN_TARGET);
    CHECK(TargetSize(129.0f) == 192);
    CHECK(TargetSize(192.0f) == 192);
    CHECK(TargetSize(350.0f) == 384);
    CHECK(TargetSize(100000.0f) == MAX_TARGET);
}

TEST_CASE("Inventory slot view: the slot sits centred and the item keeps the game's size against it")
{
    const InventoryMetrics metrics{20.0f, 480.0f, 1.0f};
    const SlotProjection slot = SlotInTarget(400, 400, 2, 4, metrics);

    CHECK(slot.slotWidth == doctest::Approx(40.0f));
    CHECK(slot.slotHeight == doctest::Approx(80.0f));
    // The taller side fills SLOT_FILL of the target, centred.
    CHECK(slot.slotHeight * slot.pixelsPerUnit == doctest::Approx(SLOT_FILL * 400.0f));
    CHECK(slot.offsetY == doctest::Approx(0.5f * (400.0f - SLOT_FILL * 400.0f)));
    CHECK(slot.offsetX + 0.5f * slot.slotWidth * slot.pixelsPerUnit == doctest::Approx(200.0f));
    CHECK(slot.centerX == 200);
    CHECK(slot.centerY == 200);

    // A world length at a given depth covers the same number of logical units as in
    // the game, where the 480-unit screen spans the 1-degree camera.
    const float depth = 200.0f;
    const float gameUnitsPerLogical = 2.0f * depth * std::tan(Radians(0.5f)) / 480.0f;
    const float targetUnitsPerPixel = depth * slot.perspective;
    CHECK(targetUnitsPerPixel * slot.pixelsPerUnit == doctest::Approx(gameUnitsPerLogical).epsilon(EPSILON));
    // perspective matches the field of view over half the target's height.
    CHECK(slot.perspective == doctest::Approx(std::tan(Radians(slot.fovDegrees * 0.5f)) / 200.0f));
}

TEST_CASE("Inventory slot view: a wide slot fills the width")
{
    const InventoryMetrics metrics{20.0f, 480.0f, 1.0f};
    const SlotProjection slot = SlotInTarget(300, 600, 4, 2, metrics);
    CHECK(slot.slotWidth * slot.pixelsPerUnit == doctest::Approx(SLOT_FILL * 300.0f));
    CHECK(slot.slotHeight * slot.pixelsPerUnit < SLOT_FILL * 600.0f);
}

extern "C" void AngleMatrix(const float angles[3], float matrix[3][4]);

namespace
{
// Where AngleMatrix(angles) turns `v`.
Vec3 Turn(const Vec3& angles, const Vec3& v)
{
    const float in[3] = {angles.x, angles.y, angles.z};
    float matrix[3][4];
    AngleMatrix(in, matrix);
    return {matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z,
            matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z,
            matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z};
}
} // namespace

TEST_CASE("Preview upright: a long item stands on its grip, its tip up")
{
    struct Case
    {
        Vec3 min;
        Vec3 max;
        Vec3 tip;
    };
    const Case cases[] = {
        {{-5, -10, -8}, {120, 10, 8}, {1, 0, 0}},   // blade along +X
        {{-120, -10, -8}, {5, 10, 8}, {-1, 0, 0}},  // along -X
        {{-10, -4, -8}, {10, 150, 8}, {0, 1, 0}},   // along +Y
        {{-10, -150, -8}, {10, 4, 8}, {0, -1, 0}},  // along -Y
        {{-10, -8, -3}, {10, 8, 90}, {0, 0, 1}},    // already upright
        {{-10, -8, -90}, {10, 8, 3}, {0, 0, -1}},   // upside down
    };
    for (const Case& item : cases)
    {
        const Vec3 up = Turn(UprightAngles(item.min, item.max), item.tip);
        CHECK(up.x == doctest::Approx(0.0f).epsilon(EPSILON));
        CHECK(up.y == doctest::Approx(0.0f).epsilon(EPSILON));
        CHECK(up.z == doctest::Approx(1.0f).epsilon(EPSILON));
    }
}

TEST_CASE("Preview upright: broad or round items keep their pose")
{
    const Vec3 shield = UprightAngles({-40, -5, -50}, {40, 5, 50});
    CHECK(shield.x == 0.0f);
    CHECK(shield.y == 0.0f);
    const Vec3 ring = UprightAngles({-5, -5, -5}, {5, 5, 5});
    CHECK(ring.x == 0.0f);
    CHECK(ring.y == 0.0f);
}
