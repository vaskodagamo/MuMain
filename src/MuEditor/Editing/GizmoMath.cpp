#include "GizmoMath.h"

#ifdef _EDITOR

#include <cmath>

// Core/Math/ZzzMathLib.cpp: the engine's Euler angles (degrees) to a rotation, the
// convention every world object's Angle[] follows. Declared here because its header
// needs the client's precompiled header.
extern "C" void AngleMatrix(const float angles[3], float matrix[3][4]);

namespace Editor::Gizmo
{
namespace
{
constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;
constexpr float RADIANS_TO_DEGREES = 180.0f / 3.14159265358979323846f;
// Below this (about 2 degrees), the view runs too nearly along an axis or plane to
// drag on it: the point would jump far for a pixel of mouse movement.
constexpr float PARALLEL_EPSILON = 1e-3f;
// cos(pitch) below this: pitch is +-90 degrees and roll folds into yaw.
constexpr float GIMBAL_EPSILON = 1e-6f;

Vec3 Rotate(const float matrix[3][4], const Vec3& v)
{
    return {matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z,
            matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z,
            matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z};
}

// The inverse (transposed) rotation of `matrix`.
Vec3 RotateBack(const float matrix[3][4], const Vec3& v)
{
    return {matrix[0][0] * v.x + matrix[1][0] * v.y + matrix[2][0] * v.z,
            matrix[0][1] * v.x + matrix[1][1] * v.y + matrix[2][1] * v.z,
            matrix[0][2] * v.x + matrix[1][2] * v.y + matrix[2][2] * v.z};
}

Vec3 ToEye(const View& view, const Vec3& world)
{
    const Vec3 rotated = Rotate(view.matrix, world);
    return {rotated.x + view.matrix[0][3], rotated.y + view.matrix[1][3], rotated.z + view.matrix[2][3]};
}

// The matrix that turns around a world axis (right-hand rule), as AngleMatrix lays it out.
void AxisRotationMatrix(int axis, float degrees, float out[3][4])
{
    const float c = std::cos(degrees * DEGREES_TO_RADIANS);
    const float s = std::sin(degrees * DEGREES_TO_RADIANS);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            out[row][column] = (row == column) ? 1.0f : 0.0f;
    const int a = (axis + 1) % 3; // the two axes the turn mixes, in right-handed order
    const int b = (axis + 2) % 3;
    out[a][a] = c;
    out[a][b] = -s;
    out[b][a] = s;
    out[b][b] = c;
}

// out = a * b for the rotation parts.
void MultiplyRotations(const float a[3][4], const float b[3][4], float out[3][4])
{
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            out[row][column] = a[row][0] * b[0][column] + a[row][1] * b[1][column] + a[row][2] * b[2][column];
        out[row][3] = 0.0f;
    }
}

Vec2 Minus(const Vec2& a, const Vec2& b)
{
    return {a.x - b.x, a.y - b.y};
}

float Cross2(const Vec2& a, const Vec2& b)
{
    return a.x * b.y - a.y * b.x;
}
} // namespace

Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float Length(const Vec3& v)
{
    return std::sqrt(Dot(v, v));
}

float Distance(const Vec2& a, const Vec2& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

Vec3 AxisVector(int axis)
{
    return {axis == AXIS_X ? 1.0f : 0.0f, axis == AXIS_Y ? 1.0f : 0.0f, axis == AXIS_Z ? 1.0f : 0.0f};
}

bool WorldToScreen(const View& view, const Vec3& world, Vec2& screen)
{
    const Vec3 eye = ToEye(view, world);
    if (eye.z >= 0.0f)
        return false; // the camera looks down -z
    screen.x = -(eye.x / view.perspectiveX / eye.z) + view.centerX;
    screen.y = (eye.y / view.perspectiveY / eye.z) + view.centerY;
    return true;
}

Ray ScreenRay(const View& view, const Vec2& screen)
{
    const Vec3 eyeDirection = {(screen.x - view.centerX) * view.perspectiveX,
                               -(screen.y - view.centerY) * view.perspectiveY, -1.0f};
    const Vec3 translation = {-view.matrix[0][3], -view.matrix[1][3], -view.matrix[2][3]};
    Ray ray;
    ray.origin = RotateBack(view.matrix, translation);
    const Vec3 direction = RotateBack(view.matrix, eyeDirection);
    ray.direction = direction * (1.0f / Length(direction));
    return ray;
}

float PixelsToWorld(const View& view, const Vec3& at, float pixels)
{
    const float depth = -ToEye(view, at).z;
    return pixels * view.perspectiveX * depth;
}

bool ClosestOnAxis(const Ray& ray, const Vec3& origin, const Vec3& axis, float& t)
{
    const Vec3 offset = origin - ray.origin;
    const float along = Dot(axis, ray.direction);
    const float denominator = 1.0f - along * along; // both unit length
    if (denominator < PARALLEL_EPSILON)
        return false;
    t = (along * Dot(ray.direction, offset) - Dot(axis, offset)) / denominator;
    return true;
}

bool IntersectPlane(const Ray& ray, const Vec3& point, const Vec3& normal, Vec3& hit)
{
    const float facing = Dot(ray.direction, normal);
    if (std::fabs(facing) < PARALLEL_EPSILON)
        return false;
    const float distance = Dot(point - ray.origin, normal) / facing;
    if (distance < 0.0f)
        return false;
    hit = ray.origin + ray.direction * distance;
    return true;
}

float SignedAngleDegrees(const Vec3& from, const Vec3& to, const Vec3& axis)
{
    return std::atan2(Dot(axis, Cross(from, to)), Dot(from, to)) * RADIANS_TO_DEGREES;
}

float DistanceToSegment(const Vec2& point, const Vec2& a, const Vec2& b)
{
    const Vec2 ab = Minus(b, a);
    const float lengthSquared = ab.x * ab.x + ab.y * ab.y;
    if (lengthSquared <= 0.0f)
        return Distance(point, a);
    const Vec2 ap = Minus(point, a);
    float t = (ap.x * ab.x + ap.y * ab.y) / lengthSquared;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return Distance(point, {a.x + ab.x * t, a.y + ab.y * t});
}

bool InsideQuad(const Vec2& point, const Vec2 corners[4])
{
    bool anyNegative = false;
    bool anyPositive = false;
    for (int i = 0; i < 4; ++i)
    {
        const float side = Cross2(Minus(corners[(i + 1) % 4], corners[i]), Minus(point, corners[i]));
        anyNegative = anyNegative || side < 0.0f;
        anyPositive = anyPositive || side > 0.0f;
    }
    return !(anyNegative && anyPositive);
}

Vec3 RotateAroundAxis(const Vec3& v, int axis, float degrees)
{
    float matrix[3][4];
    AxisRotationMatrix(axis, degrees, matrix);
    return Rotate(matrix, v);
}

void AnglesFromMatrix(const float matrix[3][4], float out[3])
{
    const float sinPitch = -matrix[2][0];
    const float clamped = sinPitch > 1.0f ? 1.0f : (sinPitch < -1.0f ? -1.0f : sinPitch);
    const float pitch = std::asin(clamped);
    const float cosPitch = std::cos(pitch);
    float roll = 0.0f;
    float yaw = 0.0f;
    if (cosPitch > GIMBAL_EPSILON)
    {
        roll = std::atan2(matrix[2][1], matrix[2][2]);
        yaw = std::atan2(matrix[1][0], matrix[0][0]);
    }
    else
    {
        yaw = std::atan2(-matrix[0][1], matrix[1][1]); // roll folds into yaw; keep roll 0
    }
    out[0] = roll * RADIANS_TO_DEGREES;
    out[1] = pitch * RADIANS_TO_DEGREES;
    out[2] = yaw * RADIANS_TO_DEGREES;
}

void RotateAngles(const float angles[3], int axis, float degrees, float out[3])
{
    if (axis == AXIS_Z)
    {
        // Rz(d) * Rz(yaw) * Ry * Rx = Rz(yaw + d) * Ry * Rx: the exact values stay.
        out[0] = angles[0];
        out[1] = angles[1];
        out[2] = angles[2] + degrees;
        return;
    }
    float current[3][4];
    AngleMatrix(angles, current);
    float turn[3][4];
    AxisRotationMatrix(axis, degrees, turn);
    float turned[3][4];
    MultiplyRotations(turn, current, turned);
    AnglesFromMatrix(turned, out);
}

float SnapTo(float value, float step)
{
    if (step <= 0.0f)
        return value;
    return std::round(value / step) * step;
}
} // namespace Editor::Gizmo

#endif // _EDITOR
