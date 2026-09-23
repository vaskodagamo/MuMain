#include "CameraFraming.h"

#ifdef _EDITOR

#include <algorithm>
#include <cmath>

namespace Editor::MapInspect
{
namespace
{
constexpr float LEVEL_PITCH_OFFSET = 90.0f; // engine pitch -90 is level
constexpr float FULL_TURN = 360.0f;
constexpr float PI = 3.14159265358979323846f;
constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
constexpr float HALF = 0.5f;

float HalfExtent(int tiles)
{
    return static_cast<float>(tiles) * TILE_WORLD_SIZE * HALF;
}
} // namespace

float EnginePitch(float pitchBelowHorizon)
{
    return pitchBelowHorizon - LEVEL_PITCH_OFFSET;
}

float PitchBelowHorizon(float enginePitch)
{
    return enginePitch + LEVEL_PITCH_OFFSET;
}

float NormalizedYaw(float yaw)
{
    const float wrapped = std::fmod(yaw, FULL_TURN);
    return wrapped < 0.0f ? wrapped + FULL_TURN : wrapped;
}

float DistanceForHeight(float height, float pitchBelowHorizon)
{
    const float pitch = std::clamp(pitchBelowHorizon, SHALLOWEST_PITCH, LEVEL_PITCH_OFFSET);
    return height / std::sin(pitch * DEGREES_TO_RADIANS);
}

float TopDownHeight(const CellRect& area, const ViewShape& view)
{
    const float tanVertical = std::tan(view.verticalFovDegrees * HALF * DEGREES_TO_RADIANS);
    const float tanHorizontal = tanVertical * view.aspect;
    if (tanVertical <= 0.0f || tanHorizontal <= 0.0f)
        return 0.0f;
    const float forWidth = HalfExtent(area.Width()) / tanHorizontal;
    const float forHeight = HalfExtent(area.Height()) / tanVertical;
    return std::max(forWidth, forHeight) * FRAME_MARGIN;
}

float TopDownViewRange(const CellRect& area, float height)
{
    const float halfWidth = HalfExtent(area.Width());
    const float halfHeight = HalfExtent(area.Height());
    return std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight + height * height) * FRAME_MARGIN;
}

std::vector<GroundPoint> AreaOutline(const CellRect& area, int stepTiles)
{
    std::vector<GroundPoint> outline;
    if (area.IsEmpty() || stepTiles <= 0)
        return outline;

    const float left = static_cast<float>(area.minX) * TILE_WORLD_SIZE;
    const float right = static_cast<float>(area.maxX + 1) * TILE_WORLD_SIZE;
    const float bottom = static_cast<float>(area.minY) * TILE_WORLD_SIZE;
    const float top = static_cast<float>(area.maxY + 1) * TILE_WORLD_SIZE;
    const float step = static_cast<float>(stepTiles) * TILE_WORLD_SIZE;
    for (float x = left; x < right; x += step)
    {
        outline.push_back({x, bottom});
        outline.push_back({x, top});
    }
    for (float y = bottom; y < top; y += step)
    {
        outline.push_back({left, y});
        outline.push_back({right, y});
    }
    outline.push_back({right, top});
    return outline;
}

std::optional<ScreenBox> BoundingBox(const std::vector<ScreenPoint>& points, int width, int height)
{
    if (points.empty())
        return std::nullopt;

    float minX = points.front().x;
    float maxX = minX;
    float minY = points.front().y;
    float maxY = minY;
    for (const ScreenPoint& point : points)
    {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }

    const int left = std::max(static_cast<int>(std::floor(minX)), 0);
    const int top = std::max(static_cast<int>(std::floor(minY)), 0);
    const int right = std::min(static_cast<int>(std::ceil(maxX)), width);
    const int bottom = std::min(static_cast<int>(std::ceil(maxY)), height);
    if (right <= left || bottom <= top)
        return std::nullopt;
    return ScreenBox{left, top, right - left, bottom - top};
}
} // namespace Editor::MapInspect

#endif // _EDITOR
