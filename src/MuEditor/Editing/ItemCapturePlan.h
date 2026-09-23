#pragma once

#ifdef _EDITOR

#include <optional>
#include <string>
#include <vector>

// The pictures the Item Editor takes of an item for an item request (Ask Codex):
// the turntable from the front (the broad face), side, back and three-quarter, the inventory slot,
// the item worn by a character, and the +level glow (+0 / +9 / +13 excellent when
// the item can be excellent, else +9 / +13). Pure data; UI/ItemEditor/ItemCaptureRun
// drives the preview through them.
namespace Editor::Preview
{
// The preview view a shot uses.
enum class CaptureView
{
    Turntable,
    Inventory,
    Equipped,
};

// Camera yaws around Z (0 looks from +X, 90 from +Y; a character faces -Y).
// "Front" is the item's broad face: weapons, bows, staffs and shields are
// modelled flat across the X axis, so they show it from +X; armour, wings and
// everything else face -Y like the character that wears them.
constexpr float WEAPON_FACE_YAW_DEGREES = 0.0f;
constexpr float BODY_FACE_YAW_DEGREES = 270.0f;
constexpr int LAST_WEAPON_GROUP = 6; // swords .. shields
// Offsets from the front, in degrees around the item.
constexpr float SIDE_OFFSET_DEGREES = 90.0f;
constexpr float BACK_OFFSET_DEGREES = 180.0f;
constexpr float THREE_QUARTER_OFFSET_DEGREES = 45.0f;

// The yaw that looks at the broad face of an item of `itemGroup` (0..15).
float FaceYawDegrees(int itemGroup);

struct CaptureShot
{
    std::string slug;        // names the file: "front" -> 01-front.jpg
    std::string requestView; // evidence.captures[].view: turntable, inventory, equipped-front or glow
    CaptureView view = CaptureView::Turntable;
    float yawDegrees = BODY_FACE_YAW_DEGREES; // unused by the inventory view (the game's own camera)
    float pitchDegrees = 0.0f;
    std::optional<float> angle; // turntable shots: degrees around the item from its front
    int level = 0;
    bool excellent = false;
    bool onlyWhenWorn = false; // dropped when the item is not drawn on the character
};

// The shots for an item that can (not) be excellent, its front at `faceYawDegrees`.
std::vector<CaptureShot> PlanItemCaptures(bool canBeExcellent, float faceYawDegrees);

// "01-front.jpg": the shot's place among the kept shots (from 1) and its slug.
std::string CaptureFileName(int number, const std::string& slug);
} // namespace Editor::Preview

#endif // _EDITOR
