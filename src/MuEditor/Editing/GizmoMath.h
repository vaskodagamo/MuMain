#pragma once

#ifdef _EDITOR

namespace Editor::Gizmo
{
struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& v, float s);
float Dot(const Vec3& a, const Vec3& b);
Vec3 Cross(const Vec3& a, const Vec3& b);
float Length(const Vec3& v);
float Distance(const Vec2& a, const Vec2& b);

// The world axes a handle moves or turns along.
constexpr int AXIS_X = 0;
constexpr int AXIS_Y = 1;
constexpr int AXIS_Z = 2;
Vec3 AxisVector(int axis);

// The camera the world was drawn with, as the engine keeps it (CameraState): the 3x4
// world-to-eye matrix (rotation and translation), the perspective factors and the
// screen centre. Screen coordinates are the game window's points, which ImGui uses too.
struct View
{
    float matrix[3][4] = {};
    float perspectiveX = 1.0f;
    float perspectiveY = 1.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
};

struct Ray
{
    Vec3 origin;
    Vec3 direction; // unit length
};

// Where `world` appears on screen, as CameraProjection::WorldToScreen computes it but
// without rounding. False for a point behind the camera.
bool WorldToScreen(const View& view, const Vec3& world, Vec2& screen);
// The ray from the camera through a screen point (CameraProjection::ScreenToWorldRay).
Ray ScreenRay(const View& view, const Vec2& screen);
// The world length that spans `pixels` on screen at the depth of `at`, so a handle
// keeps its size on screen however far away the object is.
float PixelsToWorld(const View& view, const Vec3& at, float pixels);

// The parameter t of the point on the line origin + t * axis (unit axis) closest to
// the ray. False when the ray runs along the line.
bool ClosestOnAxis(const Ray& ray, const Vec3& origin, const Vec3& axis, float& t);
// Where the ray meets the plane through `point` with `normal`. False when it runs
// along the plane or the plane is behind the camera.
bool IntersectPlane(const Ray& ray, const Vec3& point, const Vec3& normal, Vec3& hit);
// The angle in degrees that turns `from` onto `to` around `axis`, positive
// counter-clockwise when looking down the axis (right-hand rule), in (-180, 180].
float SignedAngleDegrees(const Vec3& from, const Vec3& to, const Vec3& axis);

float DistanceToSegment(const Vec2& point, const Vec2& a, const Vec2& b);
// For a convex quad whose corners go round in either direction.
bool InsideQuad(const Vec2& point, const Vec2 corners[4]);

// `v` turned around a world axis by `degrees` (right-hand rule).
Vec3 RotateAroundAxis(const Vec3& v, int axis, float degrees);
// A world object's angles (degrees, the engine's Angle[]: AngleMatrix builds
// Rz(angle[2]) * Ry(angle[1]) * Rx(angle[0])) after the object turned `degrees`
// around a world axis. A turn around Z only adds to angle[2]; X and Y go through
// the matrix.
void RotateAngles(const float angles[3], int axis, float degrees, float out[3]);
// The angles AngleMatrix turns into the rotation part of `matrix`.
void AnglesFromMatrix(const float matrix[3][4], float out[3]);

// `value` rounded to the nearest multiple of `step`.
float SnapTo(float value, float step);
} // namespace Editor::Gizmo

#endif // _EDITOR
