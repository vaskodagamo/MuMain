#include "ThumbnailFraming.h"

#ifdef _EDITOR

#include <algorithm>
#include <array>
#include <cmath>

namespace Editor::Thumbnail
{
namespace
{
using Gizmo::Vec3;

constexpr float PI = 3.14159265f;
constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
constexpr float MIN_LENGTH = 1e-6f;

// A box this small or this large is not a real model; frame a typical object instead.
constexpr float MIN_RADIUS = 1.0f;
constexpr float MAX_RADIUS = 100000.0f;
constexpr Vec3 FALLBACK_CENTER{0.0f, 0.0f, 80.0f};
constexpr float FALLBACK_RADIUS = 160.0f;
const Vec3 Z_UP{0.0f, 0.0f, 1.0f};

// Object framing (the Map Editor's palette, unchanged since it was written).
constexpr float OBJECT_FOV_DEGREES = 35.0f;
constexpr float OBJECT_ROOM = 1.8f; // distance factor: room around the model
constexpr Vec3 OBJECT_VIEW{1.0f, -1.0f, 0.8f};
constexpr float OBJECT_NEAR_MIN = 2.0f;
constexpr float OBJECT_NEAR_FACTOR = 0.05f;
constexpr float OBJECT_FAR_RADII = 8.0f;
constexpr float OBJECT_FAR_EXTRA = 4000.0f;

// Item framing.
constexpr float ITEM_FOV_DEGREES = 30.0f;
constexpr float ITEM_MARGIN = 1.08f;     // 8% room around the tightest fit
constexpr float FLAT_RATIO = 0.6f;       // thinnest side under 60% of the middle one: look at the broad side
constexpr float ELONGATED_RATIO = 1.8f;  // longest side over 1.8x the middle one: lay it corner to corner
constexpr float BROAD_SIDE_TILT = 0.35f; // turn a little off the broad side so the model reads as 3D
constexpr Vec3 ITEM_THREE_QUARTER_VIEW{0.55f, -1.0f, 0.45f};
constexpr float UP_PARALLEL_LIMIT = 0.9f; // |cos| above this: Z cannot be "up" for the view
constexpr float ITEM_NEAR_MIN = 0.1f;
constexpr float ITEM_NEAR_FACTOR = 0.5f;
constexpr float ITEM_FAR_FACTOR = 2.0f;
// The side of each axis the broad-side view looks from: +X, -Y (the front of a
// character model), +Z.
constexpr float VIEW_SIDE[3] = {1.0f, -1.0f, 1.0f};

constexpr int CORNER_COUNT = 8;

Vec3 Normalized(const Vec3& v)
{
    const float length = Gizmo::Length(v);
    return length > MIN_LENGTH ? v * (1.0f / length) : v;
}

float Component(const Vec3& v, int axis)
{
    return axis == Gizmo::AXIS_X ? v.x : (axis == Gizmo::AXIS_Y ? v.y : v.z);
}

struct Box
{
    Vec3 center;
    Vec3 min;
    Vec3 max;
    float radius = 0.0f; // half the diagonal
};

Box MakeBox(const Vec3& boundsMin, const Vec3& boundsMax)
{
    Box box{(boundsMin + boundsMax) * 0.5f, boundsMin, boundsMax, 0.5f * Gizmo::Length(boundsMax - boundsMin)};
    if (box.radius > MIN_RADIUS && box.radius <= MAX_RADIUS)
        return box;
    const Vec3 half{FALLBACK_RADIUS, FALLBACK_RADIUS, FALLBACK_RADIUS};
    const float cubeHalfDiagonalToSide = 1.0f / std::sqrt(3.0f);
    const Vec3 side = half * cubeHalfDiagonalToSide;
    return {FALLBACK_CENTER, FALLBACK_CENTER - side, FALLBACK_CENTER + side, FALLBACK_RADIUS};
}

std::array<Vec3, CORNER_COUNT> Corners(const Box& box)
{
    std::array<Vec3, CORNER_COUNT> corners;
    for (int i = 0; i < CORNER_COUNT; ++i)
    {
        corners[i] = {(i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y,
                      (i & 4) ? box.max.z : box.min.z};
    }
    return corners;
}

Camera FrameObject(const Box& box)
{
    Camera camera;
    const float distance = box.radius / std::tan(OBJECT_FOV_DEGREES * 0.5f * DEGREES_TO_RADIANS) * OBJECT_ROOM;
    camera.center = box.center;
    camera.eye = box.center + Normalized(OBJECT_VIEW) * distance;
    camera.up = Z_UP;
    camera.fovDegrees = OBJECT_FOV_DEGREES;
    camera.zNear = std::fmax(OBJECT_NEAR_MIN, distance * OBJECT_NEAR_FACTOR);
    camera.zFar = distance + box.radius * OBJECT_FAR_RADII + OBJECT_FAR_EXTRA;
    return camera;
}

// The box's axes from its thinnest side to its longest.
std::array<int, 3> AxesByExtent(const Vec3& extent)
{
    std::array<int, 3> axes = {Gizmo::AXIS_X, Gizmo::AXIS_Y, Gizmo::AXIS_Z};
    std::sort(axes.begin(), axes.end(), [&](int a, int b) { return Component(extent, a) < Component(extent, b); });
    return axes;
}

bool IsElongated(const Vec3& extent, const std::array<int, 3>& axes)
{
    return Component(extent, axes[2]) > ELONGATED_RATIO * Component(extent, axes[1]);
}

// From the model towards the camera: at a long or flat item's broad side
// (square to its length), else a 3/4 view.
Vec3 ItemViewDirection(const Vec3& extent, const std::array<int, 3>& axes)
{
    const int thin = axes[0];
    const int middle = axes[1];
    const bool flat = Component(extent, thin) < FLAT_RATIO * Component(extent, middle);
    if (!flat && !IsElongated(extent, axes))
        return Normalized(ITEM_THREE_QUARTER_VIEW);
    const Vec3 broadSide = Gizmo::AxisVector(thin) * VIEW_SIDE[thin];
    return Normalized(broadSide + Gizmo::AxisVector(middle) * BROAD_SIDE_TILT);
}

// A long item runs corner to corner, its far end (a blade's tip: the side
// farther from the model origin, where the grip is) at the top.
Vec3 DiagonalUp(const Box& box, int longAxis, const Vec3& view)
{
    const float towardMax = std::fabs(Component(box.max, longAxis));
    const float towardMin = std::fabs(Component(box.min, longAxis));
    const Vec3 tip = Gizmo::AxisVector(longAxis) * (towardMax >= towardMin ? 1.0f : -1.0f);
    const Vec3 across = Normalized(tip - view * Gizmo::Dot(tip, view));
    const Vec3 side = Gizmo::Cross(view, across);
    return Normalized(across + side);
}

Vec3 ItemUp(const Box& box, const Vec3& extent, const std::array<int, 3>& axes, const Vec3& view)
{
    const int longAxis = axes[2];
    if (IsElongated(extent, axes))
        return DiagonalUp(box, longAxis, view);
    if (std::fabs(Gizmo::Dot(view, Z_UP)) > UP_PARALLEL_LIMIT)
        return Gizmo::AxisVector(longAxis);
    return Z_UP;
}

// The nearest distance from the centre along `view` at which every corner of the
// box is inside the square picture: a corner at lateral offset r and depth z
// (towards the camera) needs distance >= r / tan(fov / 2) + z.
float FitDistance(const Box& box, const Vec3& view, const Vec3& up, float fovDegrees, float& nearestDepth,
                  float& farthestDepth)
{
    const Vec3 forward = view * -1.0f;
    const Vec3 right = Normalized(Gizmo::Cross(forward, up));
    const Vec3 screenUp = Gizmo::Cross(right, forward);
    const float tanHalf = std::tan(fovDegrees * 0.5f * DEGREES_TO_RADIANS);
    float distance = 0.0f;
    nearestDepth = -box.radius;
    farthestDepth = box.radius;
    for (const Vec3& corner : Corners(box))
    {
        const Vec3 offset = corner - box.center;
        const float lateral = std::fmax(std::fabs(Gizmo::Dot(offset, right)), std::fabs(Gizmo::Dot(offset, screenUp)));
        const float depth = Gizmo::Dot(offset, view);
        distance = std::fmax(distance, lateral / tanHalf + depth);
        nearestDepth = std::fmax(nearestDepth, depth);
        farthestDepth = std::fmin(farthestDepth, depth);
    }
    return distance * ITEM_MARGIN;
}

Camera FrameItem(const Box& box)
{
    const Vec3 extent = box.max - box.min;
    const std::array<int, 3> axes = AxesByExtent(extent);
    const Vec3 view = ItemViewDirection(extent, axes);

    Camera camera;
    camera.center = box.center;
    camera.up = ItemUp(box, extent, axes, view);
    camera.fovDegrees = ITEM_FOV_DEGREES;
    float nearestDepth = 0.0f;
    float farthestDepth = 0.0f;
    const float distance = FitDistance(box, view, camera.up, camera.fovDegrees, nearestDepth, farthestDepth);
    camera.eye = box.center + view * distance;
    camera.zNear = std::fmax(ITEM_NEAR_MIN, (distance - nearestDepth) * ITEM_NEAR_FACTOR);
    camera.zFar = (distance - farthestDepth) * ITEM_FAR_FACTOR;
    return camera;
}
} // namespace

Camera FrameBounds(const Vec3& boundsMin, const Vec3& boundsMax, Framing framing)
{
    const Box box = MakeBox(boundsMin, boundsMax);
    return framing == Framing::Item ? FrameItem(box) : FrameObject(box);
}
} // namespace Editor::Thumbnail

#endif // _EDITOR
