#pragma once

#ifdef _EDITOR

#include "Editing/GizmoMath.h"        // Editor::Gizmo::View
#include "MapInspect/CameraFraming.h" // CellRect, ScreenBox

#include <optional>

class FreeFlyCamera;
class OBJECT;

// The editor's use of the game cameras.
namespace Editor::Camera
{
// Switches to the free-fly camera, which then also culls the world with its own
// frustum (see CameraManager::SetFreeFlyCullsWorld). Starts the cameras first when
// no scene frame has run yet. Returns nullptr when the switch failed.
FreeFlyCamera* ActivateFreeFly();

// Points the free-fly camera at `object` from 50 degrees above, on the start view's
// heading, far enough back that a Lorencia tree fits with its neighbours (farther for
// objects placed larger). Returns false when the camera could not switch.
bool FocusOn(const OBJECT* object);

// Where the free-fly camera stands after a framing, in the terms of
// MapInspect/CameraFraming.h (yaw: compass heading, pitch: degrees below the horizon).
struct Pose
{
    float target[3] = {};   // the point it looks at
    float position[3] = {}; // world units
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 0.0f;
    float viewRange = 0.0f;   // how far it draws
    float verticalFov = 0.0f; // degrees
};

// Looks at the ground in the middle of tile (x, y) from `distance` away, with the
// free-fly camera's usual view range. False when the camera could not switch.
bool FrameTile(int x, int y, float yaw, float pitch, float distance, Pose& pose);

// The view of a gate area the Gates tab's "Look at it" and the control socket's gate-show
// share: its middle tile seen from the south, 50 degrees down, close enough to read the
// area's tiles. The corners may come in any order. False when the camera could not switch.
bool FrameGateArea(int x1, int y1, int x2, int y2, Pose& pose);

// Looks straight down (north up) on the middle of `area` from high enough to show all
// of it, drawing as far as that needs. False when the camera could not switch.
bool FrameTopDown(const Editor::MapInspect::CellRect& area, Pose& pose);

// The camera the world was drawn with in the last frame (window points).
Editor::Gizmo::View DrawnView();

// The part of a `width` x `height` capture of the last drawn frame that the ground of
// `area` covers; empty when none of it was in view.
std::optional<Editor::MapInspect::ScreenBox> CaptureBoxOf(const Editor::MapInspect::CellRect& area, int width,
                                                          int height);
} // namespace Editor::Camera

#endif // _EDITOR
