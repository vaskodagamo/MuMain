#pragma once

#ifdef _EDITOR

#include "Core/Globals/_enum.h" // CLASS_TYPE, MAX_CLASS

#include <array>
#include <cstdint>
#include <functional>
#include <span>

// What the character in the Item Editor preview's "Equipped" view wears for the
// selected item, by the game's own equipment rules: a weapon in the right hand, a
// shield in the left, a bow in the left hand with arrows (drawn as a quiver on the
// back), a crossbow in the right hand with bolts, wings or a cape on the back, and
// an armour part together with the rest of its set (the parts of the other armour
// groups with the same index). Item types are group * 512 + index. Pure rules, no
// engine state: the caller says which item slot the table gives the item, whether
// the engine counts it as a bow or crossbow and which items exist.
namespace Editor::Preview
{
constexpr int NO_ITEM = -1;

// The armour parts in the order of their item groups (helm, armour, pants,
// gloves, boots).
constexpr int ARMOUR_PART_COUNT = 5;

// The engine's bow kinds (CCharacterManager::GetEquipedBowType).
enum class BowKind
{
    None,
    Bow,
    Crossbow,
};

// How the selected item is worn.
enum class Wearing
{
    RightHand,
    LeftHand, // a shield, or a weapon the table puts in the left hand
    Bow,      // left hand, arrows in the right slot
    Crossbow, // right hand, bolts in the left slot
    Ammunition,
    Wings,    // wings and capes
    Armour,   // with the rest of its set
    NotShown, // pets, rings, pendants: worn, but not drawn on the body here
    NotWearable,
};

struct Outfit
{
    Wearing wearing = Wearing::NotWearable;
    int rightHand = NO_ITEM;
    int leftHand = NO_ITEM;
    int wings = NO_ITEM;
    // Helm .. boots; NO_ITEM leaves the class's bare part.
    std::array<int, ARMOUR_PART_COUNT> armour{NO_ITEM, NO_ITEM, NO_ITEM, NO_ITEM, NO_ITEM};
};

// `itemSlot` is the item table's equipment slot (ITEM_ATTRIBUTE::m_byItemSlot;
// EQUIPMENT_* numbers, anything else = not wearable). `exists(type)` says whether
// an item has a table entry and a model (used for the other set parts).
Outfit DressFor(int itemType, int itemSlot, BowKind bowKind, const std::function<bool(int itemType)>& exists);

// A short phrase for the preview: "in the right hand", "on the back", ...
const char* WearingLabel(Wearing wearing);

// The class to dress: the class filter's class and stage when one is chosen, else
// the first class (in DW, DK, Elf, MG, DL, SUM, RF order) that may equip the item,
// at the lowest stage that may; a Dark Knight when none may.
struct ClassChoice
{
    int baseClass = 0; // CLASS_TYPE of the first stage
    int stage = 1;     // 1..3
};
constexpr int NO_CLASS_FILTER = -1;
ClassChoice PreviewClass(std::span<const std::uint8_t, MAX_CLASS> requireClass, int filterClass, int filterStage);

// The CLASS_TYPE of `baseClass` at `stage` (Magic Gladiator, Dark Lord and Rage
// Fighter have no second class: stage 2 is their first).
CLASS_TYPE ClassAtStage(int baseClass, int stage);
} // namespace Editor::Preview

#endif // _EDITOR
