#include "stdafx.h"

#ifdef _EDITOR

#include "EditorCamera.h"

#include "LiveMap.h"
#include "MapInspect/LayerImage.h" // MeasureHeightRange

#include "Camera/CameraConfig.h" // HFovToVFov
#include "Camera/CameraManager.h"
#include "Camera/CameraMode.h"
#include "Camera/CameraState.h" // g_Camera: the view the world was drawn with
#include "Camera/FreeFlyCamera.h"
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

extern unsigned int WindowWidth;
extern unsigned int WindowHeight;

namespace Editor::Camera
{
namespace
{
namespace Inspect = Editor::MapInspect;

// Free-fly pitch 0 looks straight down, -90 level: -40 is 50 degrees down.
constexpr float FOCUS_YAW = -45.0f;
constexpr float FOCUS_PITCH = -40.0f;
constexpr float FOCUS_DISTANCE = 1800.0f;
constexpr float FOCUS_HEIGHT = 150.0f; // look at a point above the base, not into the ground

// A top-down view of an area looks north.
constexpr float NORTH_YAW = 0.0f;
constexpr float HALF = 0.5f;

// A gate area's view (FrameGateArea): from the south, 50 degrees down.
constexpr float GATE_LOOK_YAW = 0.0f;
constexpr float GATE_LOOK_PITCH = 50.0f;
constexpr float GATE_LOOK_DISTANCE = 2500.0f;
// Tiles between the ground points projected along a framed area's edges: often enough
// that a hill on the edge still widens the box.
constexpr int OUTLINE_STEP_TILES = 8;

// The free-fly camera's own view range, as it was before a top-down framing stretched
// it; tile framings go back to it.
const CameraConfig& UsualConfig(const FreeFlyCamera& camera)
{
    static const CameraConfig usual = camera.GetConfig();
    return usual;
}

float ViewAspect()
{
    return WindowHeight > 0 ? static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight) : 1.0f;
}

// `config` drawing out to at least `range`, with its fog kept in the same proportion.
CameraConfig WithViewRange(CameraConfig config, float range)
{
    if (range <= config.farPlane)
        return config;
    const float fogShare = config.fogEnd > 0.0f ? config.fogStart / config.fogEnd : 1.0f;
    config.farPlane = range;
    config.terrainCullRange = std::max(config.terrainCullRange, range);
    config.objectCullRange = std::max(config.objectCullRange, range);
    config.fogEnd = range;
    config.fogStart = range * fogShare;
    return config;
}

void FillPose(const FreeFlyCamera& camera, const vec3_t target, Pose& pose)
{
    vec3_t position;
    float yaw = 0.0f;
    float pitch = 0.0f;
    camera.GetPose(position, yaw, pitch);
    VectorCopy(target, pose.target);
    VectorCopy(position, pose.position);
    pose.yaw = Inspect::NormalizedYaw(yaw);
    pose.pitch = Inspect::PitchBelowHorizon(pitch);
    vec3_t offset;
    VectorSubtract(position, target, offset);
    pose.distance = VectorLength(offset);
    pose.viewRange = camera.GetConfig().farPlane;
    pose.verticalFov = HFovToVFov(camera.GetConfig().hFov, ViewAspect());
}
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

bool FrameTile(int x, int y, float yaw, float pitch, float distance, Pose& pose)
{
    FreeFlyCamera* camera = ActivateFreeFly();
    if (camera == nullptr)
        return false;
    camera->SetConfig(UsualConfig(*camera));
    const float targetX = Inspect::TileCentre(x);
    const float targetY = Inspect::TileCentre(y);
    const vec3_t target = {targetX, targetY, LiveMap::GroundHeight(targetX, targetY)};
    camera->LookAt(target, yaw, Inspect::EnginePitch(pitch), distance);
    FillPose(*camera, target, pose);
    return true;
}

bool FrameGateArea(int x1, int y1, int x2, int y2, Pose& pose)
{
    return FrameTile((x1 + x2) / 2, (y1 + y2) / 2, GATE_LOOK_YAW, GATE_LOOK_PITCH, GATE_LOOK_DISTANCE, pose);
}

bool FrameTopDown(const Inspect::CellRect& area, Pose& pose)
{
    FreeFlyCamera* camera = ActivateFreeFly();
    if (camera == nullptr)
        return false;
    const CameraConfig& usual = UsualConfig(*camera);
    const float aspect = ViewAspect();
    const Inspect::ViewShape view{HFovToVFov(usual.hFov, aspect), aspect};
    // Framed at the highest ground of the area, so every tile fits however it rises.
    const Inspect::HeightRange ground = Inspect::MeasureHeightRange(LiveMap::Terrain(), area);
    const float height = Inspect::TopDownHeight(area, view);
    const float centreX = static_cast<float>(area.minX + area.maxX + 1) * Inspect::TILE_WORLD_SIZE * HALF;
    const float centreY = static_cast<float>(area.minY + area.maxY + 1) * Inspect::TILE_WORLD_SIZE * HALF;
    const vec3_t target = {centreX, centreY, ground.max};
    const vec3_t position = {centreX, centreY, ground.max + height};

    camera->SetConfig(WithViewRange(usual, Inspect::TopDownViewRange(area, height + ground.max - ground.min)));
    camera->SnapToPosition(position, NORTH_YAW, Inspect::EnginePitch(Inspect::STEEPEST_PITCH));
    FillPose(*camera, target, pose);
    return true;
}

Editor::Gizmo::View DrawnView()
{
    Editor::Gizmo::View view;
    std::memcpy(view.matrix, g_Camera.Matrix, sizeof(view.matrix));
    view.perspectiveX = g_Camera.PerspectiveX;
    view.perspectiveY = g_Camera.PerspectiveY;
    view.centerX = static_cast<float>(g_Camera.ScreenCenterX);
    view.centerY = static_cast<float>(g_Camera.ScreenCenterY);
    return view;
}

std::optional<Inspect::ScreenBox> CaptureBoxOf(const Inspect::CellRect& area, int width, int height)
{
    if (WindowWidth == 0 || WindowHeight == 0)
        return std::nullopt;
    // The view works in window points; a capture has the swapchain's pixels.
    const float scaleX = static_cast<float>(width) / static_cast<float>(WindowWidth);
    const float scaleY = static_cast<float>(height) / static_cast<float>(WindowHeight);
    const Editor::Gizmo::View view = DrawnView();

    std::vector<Inspect::ScreenPoint> points;
    for (const Inspect::GroundPoint& point : Inspect::AreaOutline(area, OUTLINE_STEP_TILES))
    {
        const Editor::Gizmo::Vec3 ground = {point.x, point.y, LiveMap::GroundHeight(point.x, point.y)};
        Editor::Gizmo::Vec2 screen;
        if (Editor::Gizmo::WorldToScreen(view, ground, screen))
            points.push_back({screen.x * scaleX, screen.y * scaleY});
    }
    return Inspect::BoundingBox(points, width, height);
}
} // namespace Editor::Camera

#endif // _EDITOR
