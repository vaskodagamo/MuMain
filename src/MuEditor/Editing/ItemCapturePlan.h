#pragma once

#ifdef _EDITOR

#include <optional>
#include <string>
#include <vector>

// The pictures the Item Editor takes of an item for an item request (Ask Codex):
// the turntable from the front, side, back and three-quarter, the inventory slot,
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

// Camera sides around the item (yaw around Z; a character faces -Y).
constexpr float FRONT_YAW_DEGREES = 270.0f;
constexpr float SIDE_YAW_DEGREES = 0.0f;
constexpr float BACK_YAW_DEGREES = 90.0f;
constexpr float THREE_QUARTER_YAW_DEGREES = 315.0f;

struct CaptureShot
{
    std::string slug;        // names the file: "front" -> 01-front.jpg
    std::string requestView; // evidence.captures[].view: turntable, inventory, equipped-front or glow
    CaptureView view = CaptureView::Turntable;
    float yawDegrees = FRONT_YAW_DEGREES; // unused by the inventory view (the game's own camera)
    float pitchDegrees = 0.0f;
    std::optional<float> angle; // turntable shots: degrees around the item from its front
    int level = 0;
    bool excellent = false;
    bool onlyWhenWorn = false; // dropped when the item is not drawn on the character
};

std::vector<CaptureShot> PlanItemCaptures(bool canBeExcellent);

// "01-front.jpg": the shot's place among the kept shots (from 1) and its slug.
std::string CaptureFileName(int number, const std::string& slug);
} // namespace Editor::Preview

#endif // _EDITOR
