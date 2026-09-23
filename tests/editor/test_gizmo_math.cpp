#include <doctest.h>

#include "Editing/GizmoMath.h"
#include "Editing/ObjectTransform.h"

#include <cmath>
#include <vector>

// Core/Math/ZzzMathLib.cpp: the engine's Euler angles to rotation, which every
// world object's Angle[] goes through when it is drawn.
extern "C" void AngleMatrix(const float angles[3], float matrix[3][4]);

using namespace Editor::Gizmo;
namespace Transform = Editor::Editing::Transform;
using Editor::Editing::ObjectState;

namespace
{
constexpr float MATRIX_TOLERANCE = 1e-4f;
constexpr float WORLD_TOLERANCE = 1e-2f;
constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;

// The rotation around a world axis written out by hand (right-hand rule).
void HandAxisRotation(int axis, float degrees, float out[3][3])
{
    const float c = std::cos(degrees * DEGREES_TO_RADIANS);
    const float s = std::sin(degrees * DEGREES_TO_RADIANS);
    const float x[3][3] = {{1, 0, 0}, {0, c, -s}, {0, s, c}};
    const float y[3][3] = {{c, 0, s}, {0, 1, 0}, {-s, 0, c}};
    const float z[3][3] = {{c, -s, 0}, {s, c, 0}, {0, 0, 1}};
    const float (*chosen)[3] = axis == AXIS_X ? x : (axis == AXIS_Y ? y : z);
    for (int r = 0; r < 3; ++r)
        for (int k = 0; k < 3; ++k)
            out[r][k] = chosen[r][k];
}

// AngleMatrix(after) must equal turn(axis, degrees) * AngleMatrix(before).
void CheckTurnMatchesEngine(const float before[3], int axis, float degrees)
{
    float after[3];
    RotateAngles(before, axis, degrees, after);

    float original[3][4];
    AngleMatrix(before, original);
    float turn[3][3];
    HandAxisRotation(axis, degrees, turn);
    float engine[3][4];
    AngleMatrix(after, engine);
    for (int r = 0; r < 3; ++r)
    {
        for (int k = 0; k < 3; ++k)
        {
            const float expected =
                turn[r][0] * original[0][k] + turn[r][1] * original[1][k] + turn[r][2] * original[2][k];
            CAPTURE(r);
            CAPTURE(k);
            CHECK(std::fabs(engine[r][k] - expected) < MATRIX_TOLERANCE);
        }
    }
}

// A camera looking down at 45 degrees at a point of a map, about 1000 units away,
// set up the way CameraState keeps it: eye = R * world + t.
View MakeView()
{
    View view;
    const float angles[3] = {-45.0f, 0.0f, -45.0f};
    float rotation[3][4];
    AngleMatrix(angles, rotation);
    // Use the transposed engine rotation as a world-to-eye rotation.
    for (int r = 0; r < 3; ++r)
        for (int k = 0; k < 3; ++k)
            view.matrix[r][k] = rotation[k][r];
    const Vec3 camera = {13200.0f, 12150.0f, 887.0f};
    for (int r = 0; r < 3; ++r)
        view.matrix[r][3] =
            -(view.matrix[r][0] * camera.x + view.matrix[r][1] * camera.y + view.matrix[r][2] * camera.z);
    view.perspectiveX = 0.0012f;
    view.perspectiveY = 0.0012f;
    view.centerX = 960.0f;
    view.centerY = 540.0f;
    return view;
}

ObjectState At(float x, float y, float z, float yaw = 0.0f)
{
    ObjectState state;
    state.type = 1;
    state.position[0] = x;
    state.position[1] = y;
    state.position[2] = z;
    state.angle[2] = yaw;
    return state;
}
} // namespace

TEST_CASE("Turning an object around a world axis gives the Angle[] AngleMatrix agrees with [editor][gizmo]")
{
    const float samples[][3] = {{0.0f, 0.0f, 0.0f},     {0.0f, 0.0f, 90.0f},     {0.0f, 0.0f, 225.0f},
                                {10.0f, -20.0f, 30.0f}, {-75.0f, 40.0f, 170.0f}, {0.0f, 89.0f, 12.0f}};
    const float turns[] = {45.0f, -30.0f, 90.0f, 180.0f, 7.5f};
    for (const auto& sample : samples)
    {
        for (int axis = AXIS_X; axis <= AXIS_Z; ++axis)
        {
            for (float degrees : turns)
            {
                CAPTURE(axis);
                CAPTURE(degrees);
                CheckTurnMatchesEngine(sample, axis, degrees);
            }
        }
    }
}

TEST_CASE("A turn around Z keeps the object's tilt and only adds to its yaw [editor][gizmo]")
{
    const float before[3] = {12.0f, -3.0f, 90.0f};
    float after[3];
    RotateAngles(before, AXIS_Z, 45.0f, after);
    CHECK(after[0] == 12.0f);
    CHECK(after[1] == -3.0f);
    CHECK(after[2] == 135.0f);
}

TEST_CASE("A turn that tips an object to 90 degrees pitch still matches the engine [editor][gizmo]")
{
    const float upright[3] = {0.0f, 0.0f, 30.0f};
    CheckTurnMatchesEngine(upright, AXIS_Y, 90.0f);
    const float tipped[3] = {0.0f, 90.0f, 0.0f};
    CheckTurnMatchesEngine(tipped, AXIS_X, 45.0f);
}

TEST_CASE("A screen point's ray passes through the world point it shows [editor][gizmo]")
{
    const View view = MakeView();
    const Vec3 world = {12650.0f, 11700.0f, 200.0f};
    Vec2 screen;
    REQUIRE(WorldToScreen(view, world, screen));
    const Ray ray = ScreenRay(view, screen);
    const Vec3 toPoint = world - ray.origin;
    const float along = Dot(toPoint, ray.direction);
    const Vec3 closest = ray.origin + ray.direction * along;
    CHECK(Length(world - closest) < WORLD_TOLERANCE);
    CHECK(along > 0.0f);

    const Vec3 behind = ray.origin - ray.direction * 100.0f;
    Vec2 unused;
    CHECK_FALSE(WorldToScreen(view, behind, unused));
}

TEST_CASE("A handle keeps its size on screen [editor][gizmo]")
{
    const View view = MakeView();
    const Vec3 at = {12700.0f, 11650.0f, 180.0f};
    constexpr float pixels = 90.0f;
    const float length = PixelsToWorld(view, at, pixels);
    Vec2 a;
    Vec2 b;
    REQUIRE(WorldToScreen(view, at, a));
    // Along the camera's own right direction the projected length is exact.
    const Vec3 right = {view.matrix[0][0], view.matrix[0][1], view.matrix[0][2]};
    REQUIRE(WorldToScreen(view, at + right * length, b));
    CHECK(std::fabs(Distance(a, b) - pixels) < 1.0f);
}

TEST_CASE("Axis drags follow the point on the axis closest to the mouse ray [editor][gizmo]")
{
    Ray ray;
    ray.origin = {5.0f, -10.0f, 3.0f};
    ray.direction = {0.0f, 1.0f, 0.0f};
    float t = 0.0f;
    REQUIRE(ClosestOnAxis(ray, {0.0f, 0.0f, 0.0f}, AxisVector(AXIS_X), t));
    CHECK(std::fabs(t - 5.0f) < 1e-4f);
    CHECK_FALSE(ClosestOnAxis(ray, {0.0f, 0.0f, 0.0f}, AxisVector(AXIS_Y), t));

    Vec3 hit;
    ray.direction = {0.0f, 0.6f, -0.8f};
    REQUIRE(IntersectPlane(ray, {0.0f, 0.0f, 0.0f}, AxisVector(AXIS_Z), hit));
    CHECK(std::fabs(hit.z) < 1e-4f);
    CHECK(std::fabs(hit.y - (-10.0f + 0.6f * 3.75f)) < 1e-4f);
    ray.direction = {0.0f, 0.6f, 0.8f}; // away from the plane
    CHECK_FALSE(IntersectPlane(ray, {0.0f, 0.0f, 0.0f}, AxisVector(AXIS_Z), hit));
}

TEST_CASE("Ring angles are positive counter-clockwise around the axis [editor][gizmo]")
{
    CHECK(std::fabs(SignedAngleDegrees(AxisVector(AXIS_X), AxisVector(AXIS_Y), AxisVector(AXIS_Z)) - 90.0f) < 1e-4f);
    CHECK(std::fabs(SignedAngleDegrees(AxisVector(AXIS_Y), AxisVector(AXIS_X), AxisVector(AXIS_Z)) + 90.0f) < 1e-4f);
    const Vec3 turned = RotateAroundAxis(AxisVector(AXIS_X), AXIS_Z, 90.0f);
    CHECK(std::fabs(turned.y - 1.0f) < 1e-5f);
}

TEST_CASE("Handles are hit near their segments and inside their squares [editor][gizmo]")
{
    CHECK(std::fabs(DistanceToSegment({5.0f, 3.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}) - 3.0f) < 1e-5f);
    CHECK(std::fabs(DistanceToSegment({-4.0f, 3.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}) - 5.0f) < 1e-5f);
    const Vec2 square[4] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f}};
    CHECK(InsideQuad({5.0f, 5.0f}, square));
    CHECK_FALSE(InsideQuad({15.0f, 5.0f}, square));
    const Vec2 clockwise[4] = {square[3], square[2], square[1], square[0]};
    CHECK(InsideQuad({1.0f, 9.0f}, clockwise));
}

TEST_CASE("A group turns around its centre and every object turns with it [editor][gizmo]")
{
    const std::vector<ObjectState> start = {At(1100.0f, 1000.0f, 50.0f), At(900.0f, 1000.0f, 50.0f, 90.0f)};
    float pivot[3];
    Transform::Pivot(start, pivot);
    CHECK(pivot[0] == 1000.0f);
    CHECK(pivot[1] == 1000.0f);

    Transform::Delta turn;
    turn.kind = Transform::Kind::Rotate;
    turn.axis = AXIS_Z;
    turn.degrees = 90.0f;
    const std::vector<ObjectState> turned = Transform::Apply(start, pivot, turn);
    CHECK(std::fabs(turned[0].position[0] - 1000.0f) < WORLD_TOLERANCE);
    CHECK(std::fabs(turned[0].position[1] - 1100.0f) < WORLD_TOLERANCE);
    CHECK(std::fabs(turned[1].position[1] - 900.0f) < WORLD_TOLERANCE);
    CHECK(turned[0].angle[2] == 90.0f);
    CHECK(turned[1].angle[2] == 180.0f);
    CHECK(turned[0].position[2] == 50.0f);
}

TEST_CASE("A group scales around its centre, never below the smallest scale [editor][gizmo]")
{
    std::vector<ObjectState> start = {At(1100.0f, 1000.0f, 0.0f), At(900.0f, 1000.0f, 0.0f)};
    start[1].scale = 0.06f;
    float pivot[3];
    Transform::Pivot(start, pivot);
    Transform::Delta scale;
    scale.kind = Transform::Kind::Scale;
    scale.factor = 0.5f;
    const std::vector<ObjectState> scaled = Transform::Apply(start, pivot, scale);
    CHECK(scaled[0].position[0] == 1050.0f);
    CHECK(scaled[0].scale == 0.5f);
    CHECK(scaled[1].scale == Transform::MIN_OBJECT_SCALE);
}

TEST_CASE("Snapping puts the primary on the quarter-tile grid, 15 degrees and 0.1 scale [editor][gizmo]")
{
    const ObjectState primary = At(1010.0f, 2037.0f, 170.0f);
    Transform::Delta move;
    move.kind = Transform::Kind::Move;
    move.axis = Transform::AXIS_XY;
    move.move[0] = 30.0f;
    move.move[1] = -10.0f;
    move.move[2] = 3.0f; // a ground-plane move does not snap height
    const Transform::Delta snappedMove = Transform::Snapped(move, primary);
    CHECK(primary.position[0] + snappedMove.move[0] == 1050.0f);
    CHECK(primary.position[1] + snappedMove.move[1] == 2025.0f);
    CHECK(snappedMove.move[2] == 3.0f);

    Transform::Delta turn;
    turn.kind = Transform::Kind::Rotate;
    turn.degrees = 52.0f;
    CHECK(Transform::Snapped(turn, primary).degrees == 45.0f);

    ObjectState scaled = primary;
    scaled.scale = 1.3f;
    Transform::Delta scale;
    scale.kind = Transform::Kind::Scale;
    scale.factor = 1.26f; // 1.638
    const float result = scaled.scale * Transform::Snapped(scale, scaled).factor;
    CHECK(std::fabs(result - 1.6f) < 1e-5f);
}

TEST_CASE("The drag label names the kind, the axis and the amount [editor][gizmo]")
{
    Transform::Delta move;
    move.kind = Transform::Kind::Move;
    move.move[0] = 125.0f;
    CHECK(Transform::Describe(move) == "Move 125, 0, 0");

    Transform::Delta rotate;
    rotate.kind = Transform::Kind::Rotate;
    rotate.axis = Editor::Gizmo::AXIS_Z;
    rotate.degrees = 45.0f;
    CHECK(Transform::Describe(rotate) == "Rotate Z 45.0 deg");
    rotate.axis = Transform::AXIS_XY; // not a turn axis
    CHECK(Transform::Describe(rotate) == "Rotate ? 45.0 deg");

    Transform::Delta scale;
    scale.kind = Transform::Kind::Scale;
    scale.factor = 1.2f;
    CHECK(Transform::Describe(scale) == "Scale x1.20");
}
