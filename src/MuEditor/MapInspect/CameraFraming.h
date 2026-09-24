#pragma once

#ifdef _EDITOR

#include "TileArea.h"

#include <optional>
#include <vector>

// Where the editor's free-fly camera has to be to show a tile or a rectangle of the
// map, in the terms scripted clients use (map-camera):
//  - yaw: the compass heading of the view in degrees, 0 = looking towards +y (north),
//    90 = towards +x (east);
//  - pitch: degrees below the horizon, 0 = level, 90 = straight down.
// The free-fly camera itself counts its pitch from straight down (0) to level (-90).
namespace Editor::MapInspect
{
// The start view of an offline map (Editor::OfflineWorld).
constexpr float START_YAW = -45.0f;
constexpr float START_PITCH = 45.0f;
constexpr float START_DISTANCE = 6000.0f;

// The free-fly camera cannot look exactly straight down (its up vector would be
// undefined); its steepest view is this.
constexpr float STEEPEST_PITCH = 89.7f;
constexpr float SHALLOWEST_PITCH = 5.0f;
constexpr float CLOSEST_DISTANCE = 100.0f;
constexpr float FARTHEST_DISTANCE = 60000.0f;

// The share of a framed rectangle's size left free around it.
constexpr float FRAME_MARGIN = 1.05f;

// The camera's vertical field of view and the view's width / height.
struct ViewShape
{
    float verticalFovDegrees = 0.0f;
    float aspect = 1.0f;
};

float EnginePitch(float pitchBelowHorizon);
float PitchBelowHorizon(float enginePitch);
// `yaw` in [0, 360).
float NormalizedYaw(float yaw);

// The distance along the view at which a camera looking `pitchBelowHorizon` degrees
// down is `height` above its target.
float DistanceForHeight(float height, float pitchBelowHorizon);

// How far above the ground a camera looking straight down must be to see every tile
// of `area` with FRAME_MARGIN around it.
float TopDownHeight(const CellRect& area, const ViewShape& view);

// The view range (far plane) that keeps every corner of `area` inside the view of a
// camera `height` above its centre, with FRAME_MARGIN to spare.
float TopDownViewRange(const CellRect& area, float height);

// A point on the map's ground plane, world units.
struct GroundPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

// Points along the outer edge of `area` (its corners included), one every
// `stepTiles` tiles, for projecting the area onto the screen.
std::vector<GroundPoint> AreaOutline(const CellRect& area, int stepTiles);

struct ScreenPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct ScreenBox
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// The smallest box of whole pixels around `points`, clipped to a `width` x `height`
// view. Empty when there are no points or none of the box lies inside the view.
std::optional<ScreenBox> BoundingBox(const std::vector<ScreenPoint>& points, int width, int height);
} // namespace Editor::MapInspect

#endif // _EDITOR
