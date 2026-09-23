#include "ObjectTransform.h"

#ifdef _EDITOR

#include "GizmoMath.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace Editor::Editing::Transform
{
namespace
{
constexpr int AXIS_COUNT = 3;
constexpr const char* AXIS_NAMES = "XYZ";
// Room for the longest label, "Move -25600, -25600, -25600" and the like.
constexpr std::size_t DESCRIPTION_CAPACITY = 64;

char AxisName(int axis)
{
    return axis >= 0 && axis < AXIS_COUNT ? AXIS_NAMES[axis] : '?';
}

bool MovesAlong(const Delta& delta, int component)
{
    return delta.axis == component || (delta.axis == AXIS_XY && component != Gizmo::AXIS_Z);
}

Gizmo::Vec3 ToVec3(const float v[3])
{
    return {v[0], v[1], v[2]};
}

void MoveAround(const float pivot[3], float position[3], int axis, float degrees)
{
    const Gizmo::Vec3 center = ToVec3(pivot);
    const Gizmo::Vec3 turned = center + Gizmo::RotateAroundAxis(ToVec3(position) - center, axis, degrees);
    position[0] = turned.x;
    position[1] = turned.y;
    position[2] = turned.z;
}
} // namespace

void Pivot(const std::vector<ObjectState>& objects, float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    if (objects.empty())
        return;
    for (const ObjectState& object : objects)
        for (int k = 0; k < AXIS_COUNT; ++k)
            out[k] += object.position[k];
    for (int k = 0; k < AXIS_COUNT; ++k)
        out[k] /= static_cast<float>(objects.size());
}

std::vector<ObjectState> Apply(const std::vector<ObjectState>& start, const float pivot[3], const Delta& delta)
{
    std::vector<ObjectState> result = start;
    for (ObjectState& object : result)
    {
        switch (delta.kind)
        {
        case Kind::Move:
            for (int k = 0; k < AXIS_COUNT; ++k)
                object.position[k] += delta.move[k];
            break;
        case Kind::Rotate:
        {
            MoveAround(pivot, object.position, delta.axis, delta.degrees);
            const float angles[3] = {object.angle[0], object.angle[1], object.angle[2]};
            Gizmo::RotateAngles(angles, delta.axis, delta.degrees, object.angle);
            break;
        }
        case Kind::Scale:
            for (int k = 0; k < AXIS_COUNT; ++k)
                object.position[k] = pivot[k] + (object.position[k] - pivot[k]) * delta.factor;
            object.scale = std::max(MIN_OBJECT_SCALE, object.scale * delta.factor);
            break;
        }
    }
    return result;
}

Delta Snapped(const Delta& delta, const ObjectState& reference)
{
    Delta snapped = delta;
    switch (delta.kind)
    {
    case Kind::Move:
        for (int k = 0; k < AXIS_COUNT; ++k)
        {
            if (MovesAlong(delta, k))
                snapped.move[k] =
                    Gizmo::SnapTo(reference.position[k] + delta.move[k], MOVE_SNAP) - reference.position[k];
        }
        break;
    case Kind::Rotate:
        snapped.degrees = Gizmo::SnapTo(delta.degrees, ROTATE_SNAP_DEGREES);
        break;
    case Kind::Scale:
    {
        const float scale = std::max(SCALE_SNAP, Gizmo::SnapTo(reference.scale * delta.factor, SCALE_SNAP));
        snapped.factor = reference.scale > 0.0f ? scale / reference.scale : delta.factor;
        break;
    }
    }
    return snapped;
}

std::string Describe(const Delta& delta)
{
    char text[DESCRIPTION_CAPACITY] = {};
    switch (delta.kind)
    {
    case Kind::Move:
        std::snprintf(text, sizeof(text), "Move %.0f, %.0f, %.0f", delta.move[0], delta.move[1], delta.move[2]);
        break;
    case Kind::Rotate:
        std::snprintf(text, sizeof(text), "Rotate %c %.1f deg", AxisName(delta.axis), delta.degrees);
        break;
    case Kind::Scale:
        std::snprintf(text, sizeof(text), "Scale x%.2f", delta.factor);
        break;
    }
    return text;
}
} // namespace Editor::Editing::Transform

#endif // _EDITOR
