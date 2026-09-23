#include "stdafx.h"

#ifdef _EDITOR

#include "EditorCamera.h"

#include "Camera/CameraManager.h"
#include "Camera/CameraMode.h"
#include "Camera/FreeFlyCamera.h"
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT

#include <algorithm>

namespace Editor::Camera
{
namespace
{
// Free-fly pitch 0 looks straight down, -90 level: -40 is 50 degrees down.
constexpr float FOCUS_YAW = -45.0f;
constexpr float FOCUS_PITCH = -40.0f;
constexpr float FOCUS_DISTANCE = 1800.0f;
constexpr float FOCUS_HEIGHT = 150.0f; // look at a point above the base, not into the ground
} // namespace

FreeFlyCamera* ActivateFreeFly()
{
    CameraManager& cameras = CameraManager::Instance();
    if (cameras.GetActiveCamera() == nullptr)
        cameras.Initialize(); // cameras start lazily with the first scene frame

    cameras.SetCameraMode(CameraMode::FreeFly);
    if (cameras.GetCurrentMode() != CameraMode::FreeFly)
        return nullptr;
    // Set here too, so the very next frame is culled by FreeFly and not by the
    // game camera, which may look somewhere else entirely.
    cameras.SetFreeFlyCullsWorld(true);
    return static_cast<FreeFlyCamera*>(cameras.GetActiveCamera());
}

bool FocusOn(const OBJECT* object)
{
    FreeFlyCamera* camera = ActivateFreeFly();
    if (camera == nullptr)
        return false;
    const vec3_t target = {object->Position[0], object->Position[1], object->Position[2] + FOCUS_HEIGHT};
    camera->LookAt(target, FOCUS_YAW, FOCUS_PITCH, FOCUS_DISTANCE * std::max(1.0f, object->Scale));
    return true;
}
} // namespace Editor::Camera

#endif // _EDITOR
