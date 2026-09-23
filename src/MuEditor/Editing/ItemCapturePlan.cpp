#include "ItemCapturePlan.h"

#ifdef _EDITOR

#include <cmath>
#include <cstdio>

namespace Editor::Preview
{
namespace
{
constexpr float TURNTABLE_PITCH_DEGREES = 10.0f;
constexpr float THREE_QUARTER_PITCH_DEGREES = 20.0f;
// A little from the character's right, so a weapon in the right hand and a shield
// on the left arm are both in view.
constexpr float EQUIPPED_YAW_DEGREES = 300.0f;
constexpr float EQUIPPED_PITCH_DEGREES = 10.0f;
constexpr float FULL_TURN_DEGREES = 360.0f;
constexpr int GLOW_LEVELS[] = {9, 13};
constexpr std::size_t FILE_NAME_CHARS = 96;

// Degrees around the item from its front, 0..360.
float AngleFromFront(float yawDegrees)
{
    const float angle = std::fmod(yawDegrees - FRONT_YAW_DEGREES, FULL_TURN_DEGREES);
    return angle < 0.0f ? angle + FULL_TURN_DEGREES : angle;
}

CaptureShot TurntableShot(const char* slug, float yaw, float pitch)
{
    CaptureShot shot;
    shot.slug = slug;
    shot.requestView = "turntable";
    shot.yawDegrees = yaw;
    shot.pitchDegrees = pitch;
    shot.angle = AngleFromFront(yaw);
    return shot;
}

CaptureShot GlowShot(int level, bool excellent)
{
    CaptureShot shot;
    shot.slug = "glow-" + std::to_string(level) + (excellent ? "-exc" : "");
    shot.requestView = "glow";
    shot.yawDegrees = THREE_QUARTER_YAW_DEGREES;
    shot.pitchDegrees = THREE_QUARTER_PITCH_DEGREES;
    shot.level = level;
    shot.excellent = excellent;
    return shot;
}
} // namespace

std::vector<CaptureShot> PlanItemCaptures(bool canBeExcellent)
{
    std::vector<CaptureShot> shots = {
        TurntableShot("front", FRONT_YAW_DEGREES, TURNTABLE_PITCH_DEGREES),
        TurntableShot("side", SIDE_YAW_DEGREES, TURNTABLE_PITCH_DEGREES),
        TurntableShot("back", BACK_YAW_DEGREES, TURNTABLE_PITCH_DEGREES),
        TurntableShot("three-quarter", THREE_QUARTER_YAW_DEGREES, THREE_QUARTER_PITCH_DEGREES),
    };

    CaptureShot inventory;
    inventory.slug = "inventory";
    inventory.requestView = "inventory";
    inventory.view = CaptureView::Inventory;
    shots.push_back(inventory);

    CaptureShot equipped;
    equipped.slug = "equipped";
    equipped.requestView = "equipped-front";
    equipped.view = CaptureView::Equipped;
    equipped.yawDegrees = EQUIPPED_YAW_DEGREES;
    equipped.pitchDegrees = EQUIPPED_PITCH_DEGREES;
    equipped.onlyWhenWorn = true;
    shots.push_back(equipped);

    if (canBeExcellent)
        shots.push_back(GlowShot(0, true));
    for (const int level : GLOW_LEVELS)
        shots.push_back(GlowShot(level, canBeExcellent));
    return shots;
}

std::string CaptureFileName(int number, const std::string& slug)
{
    char prefix[FILE_NAME_CHARS];
    std::snprintf(prefix, sizeof(prefix), "%02d-", number);
    return prefix + slug + ".jpg";
}
} // namespace Editor::Preview

#endif // _EDITOR
