#pragma once

#ifdef _EDITOR

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
} // namespace Editor::Camera

#endif // _EDITOR
