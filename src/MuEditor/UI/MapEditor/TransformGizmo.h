#pragma once

#ifdef _EDITOR

#include "Editing/GizmoMath.h"
#include "Editing/ObjectTransform.h"

enum class GizmoMode
{
    Move,
    Rotate,
    Scale,
};

// What the gizmo did with the mouse this frame.
struct GizmoFrame
{
    enum class Phase
    {
        Idle,     // not under the cursor
        Hovered,  // a handle is under the cursor: a click grabs it
        Began,    // a handle was grabbed; `delta` is still zero
        Dragging, // `delta` is the drag so far
        Ended,    // the button was released; `delta` is the whole drag
    };
    Phase phase = Phase::Idle;
    Editor::Editing::Transform::Delta delta;
};

// The Objects tab's transform gizmo, drawn over the 3D view in ImGui's overlay with
// the camera the world was drawn with: Move has three axis arrows and a ground-plane
// square, Rotate three rings around the world axes, Scale a centre square and three
// axis handles that all scale uniformly. The handles keep their size on screen. The
// gizmo only measures the drag; the caller applies it to the objects.
class CTransformGizmo
{
public:
    GizmoMode Mode() const
    {
        return m_mode;
    }
    void SetMode(GizmoMode mode);

    // Draws the handles around `pivot` and follows a drag of the left mouse button.
    // `mouseOverWorld`: the cursor is on the 3D view, not on an editor window.
    GizmoFrame Update(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec3& pivot, bool mouseOverWorld);
    bool IsDragging() const
    {
        return m_dragging;
    }
    // Forgets a drag without a result (Esc, the tool switched off, the map changed).
    void EndDrag();
    // Writes what a drag does next to the cursor, e.g. "Rotate Z 45.0 deg".
    void DrawDragLabel(const Editor::Editing::Transform::Delta& delta) const;

private:
    enum Handle
    {
        HANDLE_NONE = -1,
        HANDLE_X = Editor::Gizmo::AXIS_X,
        HANDLE_Y = Editor::Gizmo::AXIS_Y,
        HANDLE_Z = Editor::Gizmo::AXIS_Z,
        HANDLE_PLANE_XY,
        HANDLE_CENTER,
    };

    int HandleUnderMouse(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec3& pivot,
                         const Editor::Gizmo::Vec2& mouse) const;
    bool BeginDrag(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec3& pivot, int handle,
                   const Editor::Gizmo::Vec2& mouse);
    void UpdateDrag(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec2& mouse);
    void UpdateRotation(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec2& mouse);
    void Draw(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec3& pivot, int hot) const;
    void DrawAxes(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec3& pivot, float length, int hot) const;
    void DrawRings(const Editor::Gizmo::View& view, const Editor::Gizmo::Vec3& pivot, float length, int hot) const;

    GizmoMode m_mode = GizmoMode::Move;
    int m_hot = HANDLE_NONE;
    bool m_dragging = false;
    int m_dragHandle = HANDLE_NONE;
    Editor::Gizmo::Vec3 m_dragPivot;
    Editor::Gizmo::Vec2 m_dragPivotScreen;
    float m_startAlongAxis = 0.0f;    // Move along an axis: where the drag started on it
    Editor::Gizmo::Vec3 m_startHit;   // Move in the ground plane: where the drag started
    Editor::Gizmo::Vec3 m_ringVector; // Rotate: pivot to the last point on the ring's plane
    float m_startDistance = 1.0f;     // Scale: pixels from the pivot where the drag started
    Editor::Editing::Transform::Delta m_delta;
};

#endif // _EDITOR
