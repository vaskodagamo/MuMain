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

float Wrapped(float degrees)
{
    const float angle = std::fmod(degrees, FULL_TURN_DEGREES);
    return angle < 0.0f ? angle + FULL_TURN_DEGREES : angle;
}

// `offset` degrees around the item from its front, which is at `faceYaw`.
CaptureShot TurntableShot(const char* slug, float faceYaw, float offset, float pitch)
{
    CaptureShot shot;
    shot.slug = slug;
    shot.requestView = "turntable";
    shot.yawDegrees = Wrapped(faceYaw + offset);
    shot.pitchDegrees = pitch;
    shot.angle = Wrapped(offset);
    return shot;
}

CaptureShot GlowShot(int level, bool excellent, float faceYaw)
{
    CaptureShot shot;
    shot.slug = "glow-" + std::to_string(level) + (excellent ? "-exc" : "");
    shot.requestView = "glow";
    shot.yawDegrees = Wrapped(faceYaw + THREE_QUARTER_OFFSET_DEGREES);
    shot.pitchDegrees = THREE_QUARTER_PITCH_DEGREES;
    shot.level = level;
    shot.excellent = excellent;
    return shot;
}
} // namespace

float FaceYawDegrees(int itemGroup)
{
    return itemGroup >= 0 && itemGroup <= LAST_WEAPON_GROUP ? WEAPON_FACE_YAW_DEGREES : BODY_FACE_YAW_DEGREES;
}

std::vector<CaptureShot> PlanItemCaptures(bool canBeExcellent, float faceYawDegrees)
{
    std::vector<CaptureShot> shots = {
        TurntableShot("front", faceYawDegrees, 0.0f, TURNTABLE_PITCH_DEGREES),
        TurntableShot("side", faceYawDegrees, SIDE_OFFSET_DEGREES, TURNTABLE_PITCH_DEGREES),
        TurntableShot("back", faceYawDegrees, BACK_OFFSET_DEGREES, TURNTABLE_PITCH_DEGREES),
        TurntableShot("three-quarter", faceYawDegrees, THREE_QUARTER_OFFSET_DEGREES, THREE_QUARTER_PITCH_DEGREES),
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
        shots.push_back(GlowShot(0, true, faceYawDegrees));
    for (const int level : GLOW_LEVELS)
        shots.push_back(GlowShot(level, canBeExcellent, faceYawDegrees));
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
