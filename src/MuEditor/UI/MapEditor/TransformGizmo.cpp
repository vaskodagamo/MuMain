#include "stdafx.h"

#ifdef _EDITOR

#include "TransformGizmo.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

using namespace Editor::Gizmo;
namespace Transform = Editor::Editing::Transform;

namespace
{
constexpr float HANDLE_PIXELS = 110.0f; // axis length on screen
constexpr float HIT_PIXELS = 9.0f;      // how near the cursor must come to a handle
constexpr float PLANE_NEAR = 0.3f;      // the ground-plane square, in parts of the axis length
constexpr float PLANE_FAR = 0.55f;
constexpr float RING_SCALE = 0.9f; // ring radius, in parts of the axis length
constexpr int RING_SEGMENTS = 64;
constexpr float CENTER_HALF_PIXELS = 8.0f;     // the scale handle's centre square
constexpr float SCALE_BOX_HALF_PIXELS = 5.0f;  // the boxes at the scale axes' ends
constexpr float MIN_SCALE_DRAG_PIXELS = 12.0f; // a scale drag measures from at least this far out
constexpr float MIN_RING_VECTOR = 1e-3f;       // world units: the cursor sits on the pivot
constexpr float LINE_THICKNESS = 3.0f;
constexpr float RING_THICKNESS = 2.5f;
constexpr float ARROW_LENGTH = 14.0f;
constexpr float ARROW_HALF_WIDTH = 6.0f;
constexpr float PIVOT_RADIUS = 4.0f;
constexpr float LABEL_OFFSET = 18.0f;
constexpr float LABEL_PADDING = 4.0f;
constexpr float TWO_PI = 6.28318530718f;
constexpr int AXIS_COUNT = 3;

const ImU32 AXIS_COLORS[AXIS_COUNT] = {IM_COL32(235, 70, 70, 255), IM_COL32(90, 215, 80, 255),
                                       IM_COL32(80, 130, 255, 255)};
const ImU32 HOT_COLOR = IM_COL32(255, 230, 60, 255);
const ImU32 PLANE_FILL_COLOR = IM_COL32(255, 230, 60, 70);
const ImU32 PLANE_HOT_FILL_COLOR = IM_COL32(255, 230, 60, 150);
const ImU32 CENTER_COLOR = IM_COL32(230, 230, 230, 255);
const ImU32 PIVOT_COLOR = IM_COL32(255, 255, 255, 230);
const ImU32 LABEL_BACKGROUND = IM_COL32(0, 0, 0, 180);
const ImU32 LABEL_TEXT = IM_COL32(255, 255, 255, 255);

ImVec2 ToImVec(const Vec2& v)
{
    return ImVec2(v.x, v.y);
}

Vec2 FromImVec(const ImVec2& v)
{
    return {v.x, v.y};
}

Vec3 RingPoint(const Vec3& pivot, int axis, float radius, float angle)
{
    const Vec3 u = AxisVector((axis + 1) % AXIS_COUNT);
    const Vec3 v = AxisVector((axis + 2) % AXIS_COUNT);
    return pivot + (u * std::cos(angle) + v * std::sin(angle)) * radius;
}

// The ring around `axis` on screen; false when part of it is behind the camera.
bool ProjectRing(const View& view, const Vec3& pivot, int axis, float radius, Vec2 (&points)[RING_SEGMENTS])
{
    for (int i = 0; i < RING_SEGMENTS; ++i)
    {
        const float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(RING_SEGMENTS);
        if (!WorldToScreen(view, RingPoint(pivot, axis, radius, angle), points[i]))
            return false;
    }
    return true;
}

// The ground-plane square of the Move handles on screen.
bool ProjectPlaneSquare(const View& view, const Vec3& pivot, float length, Vec2 (&corners)[4])
{
    const float nearSide = length * PLANE_NEAR;
    const float farSide = length * PLANE_FAR;
    const Vec3 world[4] = {pivot + Vec3{nearSide, nearSide, 0.0f}, pivot + Vec3{farSide, nearSide, 0.0f},
                           pivot + Vec3{farSide, farSide, 0.0f}, pivot + Vec3{nearSide, farSide, 0.0f}};
    for (int i = 0; i < 4; ++i)
    {
        if (!WorldToScreen(view, world[i], corners[i]))
            return false;
    }
    return true;
}

bool ProjectAxis(const View& view, const Vec3& pivot, float length, int axis, Vec2& start, Vec2& end)
{
    return WorldToScreen(view, pivot, start) && WorldToScreen(view, pivot + AxisVector(axis) * length, end);
}

// Pixels from the cursor to a handle; far away when it is not on screen.
float RingDistance(const View& view, const Vec3& pivot, float radius, int axis, const Vec2& mouse)
{
    Vec2 ring[RING_SEGMENTS];
    if (!ProjectRing(view, pivot, axis, radius, ring))
        return FLT_MAX;
    float distance = FLT_MAX;
    for (int i = 0; i < RING_SEGMENTS; ++i)
        distance = std::min(distance, DistanceToSegment(mouse, ring[i], ring[(i + 1) % RING_SEGMENTS]));
    return distance;
}

float AxisDistance(const View& view, const Vec3& pivot, float length, int axis, const Vec2& mouse)
{
    Vec2 start;
    Vec2 end;
    if (!ProjectAxis(view, pivot, length, axis, start, end))
        return FLT_MAX;
    return DistanceToSegment(mouse, start, end);
}

void DrawArrowHead(ImDrawList* draw, const Vec2& start, const Vec2& end, ImU32 color)
{
    const float length = Distance(start, end);
    if (length <= 0.0f)
        return;
    const Vec2 direction = {(end.x - start.x) / length, (end.y - start.y) / length};
    const Vec2 side = {-direction.y, direction.x};
    const ImVec2 tip(end.x + direction.x * ARROW_LENGTH, end.y + direction.y * ARROW_LENGTH);
    const ImVec2 left(end.x + side.x * ARROW_HALF_WIDTH, end.y + side.y * ARROW_HALF_WIDTH);
    const ImVec2 right(end.x - side.x * ARROW_HALF_WIDTH, end.y - side.y * ARROW_HALF_WIDTH);
    draw->AddTriangleFilled(tip, left, right, color);
}

void DrawSquare(ImDrawList* draw, const Vec2& center, float halfSide, ImU32 color)
{
    draw->AddRectFilled(ImVec2(center.x - halfSide, center.y - halfSide),
                        ImVec2(center.x + halfSide, center.y + halfSide), color);
}
} // namespace

void CTransformGizmo::SetMode(GizmoMode mode)
{
    if (m_dragging)
        return;
    m_mode = mode;
}

void CTransformGizmo::EndDrag()
{
    m_dragging = false;
    m_dragHandle = HANDLE_NONE;
}

GizmoFrame CTransformGizmo::Update(const View& view, const Vec3& pivot, bool mouseOverWorld)
{
    GizmoFrame frame;
    const Vec2 mouse = FromImVec(ImGui::GetIO().MousePos);
    if (m_dragging)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            UpdateDrag(view, mouse);
            frame.phase = GizmoFrame::Phase::Dragging;
        }
        else
        {
            frame.phase = GizmoFrame::Phase::Ended;
            EndDrag();
        }
        frame.delta = m_delta;
        Draw(view, pivot, m_dragHandle);
        return frame;
    }

    Vec2 pivotScreen;
    const bool visible = WorldToScreen(view, pivot, pivotScreen); // not behind the camera
    m_hot = (visible && mouseOverWorld) ? HandleUnderMouse(view, pivot, mouse) : HANDLE_NONE;
    if (m_hot != HANDLE_NONE && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && BeginDrag(view, pivot, m_hot, mouse))
    {
        frame.phase = GizmoFrame::Phase::Began;
        frame.delta = m_delta;
    }
    else if (m_hot != HANDLE_NONE)
    {
        frame.phase = GizmoFrame::Phase::Hovered;
    }
    Draw(view, pivot, m_hot);
    return frame;
}

int CTransformGizmo::HandleUnderMouse(const View& view, const Vec3& pivot, const Vec2& mouse) const
{
    const float length = PixelsToWorld(view, pivot, HANDLE_PIXELS);
    if (m_mode == GizmoMode::Move)
    {
        Vec2 corners[4];
        if (ProjectPlaneSquare(view, pivot, length, corners) && InsideQuad(mouse, corners))
            return HANDLE_PLANE_XY;
    }
    if (m_mode == GizmoMode::Scale)
    {
        Vec2 center;
        if (WorldToScreen(view, pivot, center) && std::fabs(mouse.x - center.x) <= CENTER_HALF_PIXELS &&
            std::fabs(mouse.y - center.y) <= CENTER_HALF_PIXELS)
            return HANDLE_CENTER;
    }

    int best = HANDLE_NONE;
    float bestDistance = HIT_PIXELS;
    for (int axis = 0; axis < AXIS_COUNT; ++axis)
    {
        const float distance = m_mode == GizmoMode::Rotate ? RingDistance(view, pivot, length * RING_SCALE, axis, mouse)
                                                           : AxisDistance(view, pivot, length, axis, mouse);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = axis;
        }
    }
    return best;
}

bool CTransformGizmo::BeginDrag(const View& view, const Vec3& pivot, int handle, const Vec2& mouse)
{
    const Ray ray = ScreenRay(view, mouse);
    m_delta = Transform::Delta{};
    m_dragPivot = pivot;
    WorldToScreen(view, pivot, m_dragPivotScreen);
    switch (m_mode)
    {
    case GizmoMode::Move:
        m_delta.kind = Transform::Kind::Move;
        if (handle == HANDLE_PLANE_XY)
        {
            m_delta.axis = Transform::AXIS_XY;
            if (!IntersectPlane(ray, pivot, AxisVector(AXIS_Z), m_startHit))
                return false;
        }
        else
        {
            m_delta.axis = handle;
            if (!ClosestOnAxis(ray, pivot, AxisVector(handle), m_startAlongAxis))
                return false;
        }
        break;
    case GizmoMode::Rotate:
    {
        m_delta.kind = Transform::Kind::Rotate;
        m_delta.axis = handle;
        Vec3 hit;
        if (!IntersectPlane(ray, pivot, AxisVector(handle), hit))
            return false;
        m_ringVector = hit - pivot;
        break;
    }
    case GizmoMode::Scale:
        m_delta.kind = Transform::Kind::Scale;
        m_startDistance = std::max(Distance(mouse, m_dragPivotScreen), MIN_SCALE_DRAG_PIXELS);
        break;
    }
    m_dragging = true;
    m_dragHandle = handle;
    return true;
}

void CTransformGizmo::UpdateDrag(const View& view, const Vec2& mouse)
{
    const Ray ray = ScreenRay(view, mouse);
    switch (m_delta.kind)
    {
    case Transform::Kind::Move:
        if (m_delta.axis == Transform::AXIS_XY)
        {
            Vec3 hit;
            if (IntersectPlane(ray, m_dragPivot, AxisVector(AXIS_Z), hit))
            {
                m_delta.move[0] = hit.x - m_startHit.x;
                m_delta.move[1] = hit.y - m_startHit.y;
            }
        }
        else
        {
            float along = 0.0f;
            if (ClosestOnAxis(ray, m_dragPivot, AxisVector(m_delta.axis), along))
                m_delta.move[m_delta.axis] = along - m_startAlongAxis;
        }
        break;
    case Transform::Kind::Rotate:
        UpdateRotation(view, mouse);
        break;
    case Transform::Kind::Scale:
        m_delta.factor = Distance(mouse, m_dragPivotScreen) / m_startDistance;
        break;
    }
}

// The turn adds up frame by frame, so a drag once around the ring turns 360 degrees.
void CTransformGizmo::UpdateRotation(const View& view, const Vec2& mouse)
{
    const Vec3 axis = AxisVector(m_delta.axis);
    Vec3 hit;
    if (!IntersectPlane(ScreenRay(view, mouse), m_dragPivot, axis, hit))
        return;
    const Vec3 ringVector = hit - m_dragPivot;
    if (Length(ringVector) < MIN_RING_VECTOR)
        return;
    m_delta.degrees += SignedAngleDegrees(m_ringVector, ringVector, axis);
    m_ringVector = ringVector;
}

void CTransformGizmo::Draw(const View& view, const Vec3& pivot, int hot) const
{
    Vec2 center;
    if (!WorldToScreen(view, pivot, center))
        return; // the selection is behind the camera
    const float length = PixelsToWorld(view, pivot, HANDLE_PIXELS);
    if (m_mode == GizmoMode::Rotate)
        DrawRings(view, pivot, length, hot);
    else
        DrawAxes(view, pivot, length, hot);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(ToImVec(center), PIVOT_RADIUS, PIVOT_COLOR);
}

void CTransformGizmo::DrawAxes(const View& view, const Vec3& pivot, float length, int hot) const
{
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (m_mode == GizmoMode::Move)
    {
        Vec2 corners[4];
        if (ProjectPlaneSquare(view, pivot, length, corners))
        {
            const ImVec2 points[4] = {ToImVec(corners[0]), ToImVec(corners[1]), ToImVec(corners[2]),
                                      ToImVec(corners[3])};
            const bool planeHot = hot == HANDLE_PLANE_XY;
            draw->AddConvexPolyFilled(points, 4, planeHot ? PLANE_HOT_FILL_COLOR : PLANE_FILL_COLOR);
            draw->AddPolyline(points, 4, HOT_COLOR, ImDrawFlags_Closed, planeHot ? LINE_THICKNESS : 1.0f);
        }
    }
    for (int axis = 0; axis < AXIS_COUNT; ++axis)
    {
        Vec2 start;
        Vec2 end;
        if (!ProjectAxis(view, pivot, length, axis, start, end))
            continue;
        // Every Scale handle scales uniformly, so they all light up together.
        const bool axisHot = hot == axis || (m_mode == GizmoMode::Scale && hot != HANDLE_NONE);
        const ImU32 color = axisHot ? HOT_COLOR : AXIS_COLORS[axis];
        draw->AddLine(ToImVec(start), ToImVec(end), color, LINE_THICKNESS);
        if (m_mode == GizmoMode::Move)
            DrawArrowHead(draw, start, end, color);
        else
            DrawSquare(draw, end, SCALE_BOX_HALF_PIXELS, color);
    }
    Vec2 center;
    if (m_mode == GizmoMode::Scale && WorldToScreen(view, pivot, center))
        DrawSquare(draw, center, CENTER_HALF_PIXELS, hot == HANDLE_NONE ? CENTER_COLOR : HOT_COLOR);
}

void CTransformGizmo::DrawRings(const View& view, const Vec3& pivot, float length, int hot) const
{
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (int axis = 0; axis < AXIS_COUNT; ++axis)
    {
        Vec2 ring[RING_SEGMENTS];
        if (!ProjectRing(view, pivot, axis, length * RING_SCALE, ring))
            continue;
        ImVec2 points[RING_SEGMENTS];
        for (int i = 0; i < RING_SEGMENTS; ++i)
            points[i] = ToImVec(ring[i]);
        const bool ringHot = hot == axis;
        draw->AddPolyline(points, RING_SEGMENTS, ringHot ? HOT_COLOR : AXIS_COLORS[axis], ImDrawFlags_Closed,
                          ringHot ? LINE_THICKNESS : RING_THICKNESS);
    }
}

void CTransformGizmo::DrawDragLabel(const Transform::Delta& delta) const
{
    const std::string text = Transform::Describe(delta);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 position(mouse.x + LABEL_OFFSET, mouse.y + LABEL_OFFSET);
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddRectFilled(ImVec2(position.x - LABEL_PADDING, position.y - LABEL_PADDING),
                        ImVec2(position.x + size.x + LABEL_PADDING, position.y + size.y + LABEL_PADDING),
                        LABEL_BACKGROUND);
    draw->AddText(position, LABEL_TEXT, text.c_str());
}

#endif // _EDITOR
