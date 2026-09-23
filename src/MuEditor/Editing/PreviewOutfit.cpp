#include "stdafx.h"

#include "PreviewOutfit.h"

#ifdef _EDITOR

#include "Assets/ItemBrowse.h" // ClassStages

#include "GameLogic/Items/ItemClassRule.h"

#include <algorithm>

namespace Editor::Preview
{
namespace
{
constexpr int FIRST_STAGE = 1;
constexpr int THIRD_STAGE = 3;
constexpr int DEFAULT_BASE_CLASS = CLASS_KNIGHT;
constexpr int NO_ARMOUR_PART = -1;

constexpr int ARMOUR_GROUPS[ARMOUR_PART_COUNT] = {ITEM_GROUP_HELM, ITEM_GROUP_ARMOR, ITEM_GROUP_PANTS,
                                                  ITEM_GROUP_GLOVES, ITEM_GROUP_BOOTS};

// CLASS_TYPE per base class and stage 1..3; a class without a second stage keeps its
// first there.
constexpr CLASS_TYPE CLASSES_BY_STAGE[MAX_CLASS][THIRD_STAGE] = {
    {CLASS_WIZARD, CLASS_SOULMASTER, CLASS_GRANDMASTER},
    {CLASS_KNIGHT, CLASS_BLADEKNIGHT, CLASS_BLADEMASTER},
    {CLASS_ELF, CLASS_MUSEELF, CLASS_HIGHELF},
    {CLASS_DARK, CLASS_DARK, CLASS_DUELMASTER},
    {CLASS_DARK_LORD, CLASS_DARK_LORD, CLASS_LORDEMPEROR},
    {CLASS_SUMMONER, CLASS_BLOODYSUMMONER, CLASS_DIMENSIONMASTER},
    {CLASS_RAGEFIGHTER, CLASS_RAGEFIGHTER, CLASS_TEMPLENIGHT},
};

int GroupOf(int itemType)
{
    return itemType / MAX_ITEM_INDEX;
}

int IndexOf(int itemType)
{
    return itemType % MAX_ITEM_INDEX;
}

int ArmourPartOf(int group)
{
    for (int part = 0; part < ARMOUR_PART_COUNT; ++part)
    {
        if (ARMOUR_GROUPS[part] == group)
            return part;
    }
    return NO_ARMOUR_PART;
}

Outfit DressArmour(int itemType, const std::function<bool(int)>& exists)
{
    Outfit outfit;
    outfit.wearing = Wearing::Armour;
    const int index = IndexOf(itemType);
    for (int part = 0; part < ARMOUR_PART_COUNT; ++part)
    {
        const int partType = ARMOUR_GROUPS[part] * MAX_ITEM_INDEX + index;
        if (partType == itemType || exists(partType))
            outfit.armour[part] = partType;
    }
    return outfit;
}

Outfit DressWeapon(int itemType, int itemSlot, BowKind bowKind)
{
    Outfit outfit;
    if (itemType == ITEM_ARROWS || itemType == ITEM_BOLT)
    {
        // Ammunition sits in the slot opposite its bow and is drawn as a quiver.
        outfit.wearing = Wearing::Ammunition;
        (itemType == ITEM_ARROWS ? outfit.rightHand : outfit.leftHand) = itemType;
        return outfit;
    }
    if (bowKind == BowKind::Bow)
    {
        outfit.wearing = Wearing::Bow;
        outfit.leftHand = itemType;
        outfit.rightHand = ITEM_ARROWS;
        return outfit;
    }
    if (bowKind == BowKind::Crossbow)
    {
        outfit.wearing = Wearing::Crossbow;
        outfit.rightHand = itemType;
        outfit.leftHand = ITEM_BOLT;
        return outfit;
    }
    const bool leftHand = itemSlot == EQUIPMENT_WEAPON_LEFT || GroupOf(itemType) == ITEM_GROUP_SHIELD;
    outfit.wearing = leftHand ? Wearing::LeftHand : Wearing::RightHand;
    (leftHand ? outfit.leftHand : outfit.rightHand) = itemType;
    return outfit;
}
} // namespace

Outfit DressFor(int itemType, int itemSlot, BowKind bowKind, const std::function<bool(int)>& exists)
{
    if (ArmourPartOf(GroupOf(itemType)) != NO_ARMOUR_PART && itemSlot >= EQUIPMENT_HELM && itemSlot <= EQUIPMENT_BOOTS)
        return DressArmour(itemType, exists);

    if (itemSlot == EQUIPMENT_WEAPON_RIGHT || itemSlot == EQUIPMENT_WEAPON_LEFT)
        return DressWeapon(itemType, itemSlot, bowKind);

    Outfit outfit;
    if (itemSlot == EQUIPMENT_WING)
    {
        outfit.wearing = Wearing::Wings;
        outfit.wings = itemType;
    }
    else if (itemSlot >= EQUIPMENT_HELPER && itemSlot < MAX_EQUIPMENT)
    {
        outfit.wearing = Wearing::NotShown;
    }
    return outfit;
}

const char* WearingLabel(Wearing wearing)
{
    switch (wearing)
    {
    case Wearing::RightHand:
        return "in the right hand";
    case Wearing::LeftHand:
        return "in the left hand";
    case Wearing::Bow:
        return "bow in the left hand, arrows as a quiver";
    case Wearing::Crossbow:
        return "crossbow in the right hand, bolts as a quiver";
    case Wearing::Ammunition:
        return "as a quiver on the back";
    case Wearing::Wings:
        return "on the back";
    case Wearing::Armour:
        return "with the rest of its set";
    case Wearing::NotShown:
        return "worn, but not drawn on the body (pet, ring or pendant)";
    case Wearing::NotWearable:
        return "cannot be worn";
    }
    return "";
}

CLASS_TYPE ClassAtStage(int baseClass, int stage)
{
    if (baseClass < 0 || baseClass >= MAX_CLASS)
        baseClass = DEFAULT_BASE_CLASS;
    const int column = std::clamp(stage, FIRST_STAGE, THIRD_STAGE) - FIRST_STAGE;
    return CLASSES_BY_STAGE[baseClass][column];
}

ClassChoice PreviewClass(std::span<const std::uint8_t, MAX_CLASS> requireClass, int filterClass, int filterStage)
{
    if (filterClass >= 0 && filterClass < MAX_CLASS)
        return {filterClass, filterStage};

    for (int baseClass = 0; baseClass < MAX_CLASS; ++baseClass)
    {
        for (const std::uint8_t stage : Items::ClassStages(baseClass))
        {
            if (GameLogic::Items::CanClassEquip(requireClass, static_cast<CLASS_TYPE>(baseClass), stage))
                return {baseClass, stage};
        }
    }
    return {DEFAULT_BASE_CLASS, FIRST_STAGE};
}
} // namespace Editor::Preview

#endif // _EDITOR
