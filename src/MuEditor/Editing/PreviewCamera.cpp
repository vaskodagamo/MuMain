#include "PreviewCamera.h"

#ifdef _EDITOR

#include <algorithm>
#include <cmath>

namespace Editor::Preview
{
namespace
{
using Gizmo::Vec3;

constexpr float PI = 3.14159265f;
constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
constexpr float FULL_TURN_DEGREES = 360.0f;
constexpr float MIN_LENGTH = 1e-6f;

constexpr float DEGREES_PER_PIXEL = 0.4f;   // a drag across a 450-pixel picture turns half way round
constexpr float ZOOM_PER_STEP = 1.15f;      // one wheel notch
constexpr float TURN_DEGREES_PER_SECOND = 30.0f; // one full turn in 12 seconds

constexpr float FOV_DEGREES = 30.0f;
constexpr float MARGIN = 1.05f;             // 5% room around the subject's sphere
constexpr float NEAR_FACTOR = 0.05f;        // of the distance, so the near plane never cuts the subject
constexpr float MIN_NEAR = 1.0f;
constexpr float FAR_RADII = 12.0f;          // room for the ground behind the subject
const Vec3 Z_UP{0.0f, 0.0f, 1.0f};

// Longest side over this times the middle one: a long item to stand upright (the
// thumbnails' threshold for laying an item corner to corner).
constexpr float ELONGATED_RATIO = 1.8f;
constexpr float QUARTER_TURN = 90.0f;
constexpr float HALF_TURN = 180.0f;

Vec3 Normalized(const Vec3& v)
{
    const float length = Gizmo::Length(v);
    return length > MIN_LENGTH ? v * (1.0f / length) : v;
}

float WrappedYaw(float degrees)
{
    const float wrapped = std::fmod(degrees, FULL_TURN_DEGREES);
    return wrapped < 0.0f ? wrapped + FULL_TURN_DEGREES : wrapped;
}

struct Axes
{
    Vec3 side;     // the camera's right
    Vec3 up;       // the camera's up
    Vec3 forward;  // from the eye to the centre
};

Axes CameraAxes(const View& view)
{
    const Vec3 forward = Normalized(view.center - view.eye);
    const Vec3 side = Normalized(Gizmo::Cross(forward, view.up));
    return {side, Gizmo::Cross(side, forward), forward};
}
} // namespace

Orbit Dragged(const Orbit& orbit, float dxPixels, float dyPixels)
{
    Orbit result = orbit;
    result.yawDegrees = WrappedYaw(orbit.yawDegrees - dxPixels * DEGREES_PER_PIXEL);
    result.pitchDegrees =
        std::clamp(orbit.pitchDegrees + dyPixels * DEGREES_PER_PIXEL, MIN_PITCH_DEGREES, MAX_PITCH_DEGREES);
    return result;
}

Orbit Zoomed(const Orbit& orbit, float steps)
{
    Orbit result = orbit;
    result.zoom = std::clamp(orbit.zoom * std::pow(ZOOM_PER_STEP, steps), MIN_ZOOM, MAX_ZOOM);
    return result;
}

Orbit Turned(const Orbit& orbit, float seconds)
{
    Orbit result = orbit;
    result.yawDegrees = WrappedYaw(orbit.yawDegrees + seconds * TURN_DEGREES_PER_SECOND);
    return result;
}

Orbit Facing(const Vec3& towardCamera)
{
    const Vec3 direction = Normalized(towardCamera);
    Orbit orbit;
    orbit.yawDegrees = WrappedYaw(std::atan2(direction.y, direction.x) / DEGREES_TO_RADIANS);
    const float pitch = std::asin(std::clamp(direction.z, -1.0f, 1.0f)) / DEGREES_TO_RADIANS;
    orbit.pitchDegrees = std::clamp(pitch, MIN_PITCH_DEGREES, MAX_PITCH_DEGREES);
    return orbit;
}

Vec3 UprightAngles(const Vec3& boundsMin, const Vec3& boundsMax)
{
    const float extent[3] = {boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z};
    int longest = Gizmo::AXIS_X;
    for (int axis = Gizmo::AXIS_Y; axis <= Gizmo::AXIS_Z; ++axis)
    {
        if (extent[axis] > extent[longest])
            longest = axis;
    }
    const float middle = std::max(std::min(extent[0], extent[1]),
                                  std::min(std::max(extent[0], extent[1]), extent[2]));
    if (extent[longest] <= ELONGATED_RATIO * middle)
        return {};

    const float towardMax[3] = {std::fabs(boundsMax.x), std::fabs(boundsMax.y), std::fabs(boundsMax.z)};
    const float towardMin[3] = {std::fabs(boundsMin.x), std::fabs(boundsMin.y), std::fabs(boundsMin.z)};
    const bool tipAtMax = towardMax[longest] >= towardMin[longest];
    // AngleMatrix: angle[1] = -90 turns +X onto +Z, angle[0] = 90 turns +Y onto +Z.
    switch (longest)
    {
    case Gizmo::AXIS_X:
        return {0.0f, tipAtMax ? -QUARTER_TURN : QUARTER_TURN, 0.0f};
    case Gizmo::AXIS_Y:
        return {tipAtMax ? QUARTER_TURN : -QUARTER_TURN, 0.0f, 0.0f};
    default:
        return {tipAtMax ? 0.0f : HALF_TURN, 0.0f, 0.0f};
    }
}

View OrbitView(const Vec3& center, float radius, const Orbit& orbit, float aspect)
{
    const float yaw = orbit.yawDegrees * DEGREES_TO_RADIANS;
    const float pitch = orbit.pitchDegrees * DEGREES_TO_RADIANS;
    const Vec3 towardCamera{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};

    // A sphere fits when its radius is within the half angle of the narrower side.
    const float tanHalfVertical = std::tan(FOV_DEGREES * 0.5f * DEGREES_TO_RADIANS);
    const float tanHalfNarrow = tanHalfVertical * std::min(1.0f, aspect);
    const float sinHalfNarrow = tanHalfNarrow / std::sqrt(1.0f + tanHalfNarrow * tanHalfNarrow);
    const float distance = radius * MARGIN / sinHalfNarrow / std::max(orbit.zoom, MIN_ZOOM);

    View view;
    view.center = center;
    view.eye = center + towardCamera * distance;
    view.up = Z_UP;
    view.fovDegrees = FOV_DEGREES;
    view.zNear = std::max(MIN_NEAR, distance * NEAR_FACTOR);
    view.zFar = distance + radius * FAR_RADII;
    return view;
}

void LookAtRows(const View& view, float out[3][4])
{
    const Axes axes = CameraAxes(view);
    const Vec3 backward = axes.forward * -1.0f;
    const Vec3 rows[3] = {axes.side, axes.up, backward};
    for (int row = 0; row < 3; ++row)
    {
        out[row][0] = rows[row].x;
        out[row][1] = rows[row].y;
        out[row][2] = rows[row].z;
        out[row][3] = -Gizmo::Dot(rows[row], view.eye);
    }
}

void LookAtColumnMajor(const View& view, float out[16])
{
    float rows[3][4];
    LookAtRows(view, rows);
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 3; ++row)
            out[column * 4 + row] = rows[row][column];
        out[column * 4 + 3] = column == 3 ? 1.0f : 0.0f;
    }
}

int TargetSize(float pixels)
{
    const int steps = static_cast<int>(std::ceil(std::max(pixels, 0.0f) / TARGET_STEP));
    return std::clamp(steps * TARGET_STEP, MIN_TARGET, MAX_TARGET);
}
} // namespace Editor::Preview

#endif // _EDITOR
