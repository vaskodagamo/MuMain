#include "stdafx.h"

#include <doctest.h>

#include "Editing/PreviewOutfit.h"

#include <set>

using namespace Editor::Preview;

namespace
{
using RequireClass = std::array<std::uint8_t, MAX_CLASS>;

constexpr int TYPE(int group, int index)
{
    return group * MAX_ITEM_INDEX + index;
}

// Every armour group has index 1; index 2 lacks a helm (like a Magic Gladiator set).
bool Exists(int type)
{
    static const std::set<int> known = {
        TYPE(ITEM_GROUP_HELM, 1),   TYPE(ITEM_GROUP_ARMOR, 1), TYPE(ITEM_GROUP_PANTS, 1),
        TYPE(ITEM_GROUP_GLOVES, 1), TYPE(ITEM_GROUP_BOOTS, 1), TYPE(ITEM_GROUP_ARMOR, 2),
        TYPE(ITEM_GROUP_PANTS, 2),  TYPE(ITEM_GROUP_GLOVES, 2), TYPE(ITEM_GROUP_BOOTS, 2),
    };
    return known.contains(type);
}
} // namespace

TEST_CASE("Preview outfit: weapons go in the hand the game puts them in")
{
    const int sword = TYPE(ITEM_GROUP_SWORD, 19);
    const Outfit right = DressFor(sword, EQUIPMENT_WEAPON_RIGHT, BowKind::None, Exists);
    CHECK(right.wearing == Wearing::RightHand);
    CHECK(right.rightHand == sword);
    CHECK(right.leftHand == NO_ITEM);

    const int shield = TYPE(ITEM_GROUP_SHIELD, 3);
    const Outfit left = DressFor(shield, EQUIPMENT_WEAPON_LEFT, BowKind::None, Exists);
    CHECK(left.wearing == Wearing::LeftHand);
    CHECK(left.leftHand == shield);
    CHECK(left.rightHand == NO_ITEM);

    // A shield the table puts in slot 0 still goes on the left arm.
    CHECK(DressFor(shield, EQUIPMENT_WEAPON_RIGHT, BowKind::None, Exists).leftHand == shield);
}

TEST_CASE("Preview outfit: bows and crossbows come with their ammunition")
{
    const int bow = TYPE(ITEM_GROUP_BOW, 5);
    const Outfit withBow = DressFor(bow, EQUIPMENT_WEAPON_LEFT, BowKind::Bow, Exists);
    CHECK(withBow.wearing == Wearing::Bow);
    CHECK(withBow.leftHand == bow);
    CHECK(withBow.rightHand == ITEM_ARROWS);

    const int crossbow = TYPE(ITEM_GROUP_BOW, 9);
    const Outfit withCrossbow = DressFor(crossbow, EQUIPMENT_WEAPON_RIGHT, BowKind::Crossbow, Exists);
    CHECK(withCrossbow.wearing == Wearing::Crossbow);
    CHECK(withCrossbow.rightHand == crossbow);
    CHECK(withCrossbow.leftHand == ITEM_BOLT);

    const Outfit arrows = DressFor(ITEM_ARROWS, EQUIPMENT_WEAPON_RIGHT, BowKind::None, Exists);
    CHECK(arrows.wearing == Wearing::Ammunition);
    CHECK(arrows.rightHand == ITEM_ARROWS);
    const Outfit bolts = DressFor(ITEM_BOLT, EQUIPMENT_WEAPON_LEFT, BowKind::None, Exists);
    CHECK(bolts.leftHand == ITEM_BOLT);
}

TEST_CASE("Preview outfit: an armour part brings the rest of its set, missing parts stay bare")
{
    const Outfit full = DressFor(TYPE(ITEM_GROUP_GLOVES, 1), EQUIPMENT_GLOVES, BowKind::None, Exists);
    CHECK(full.wearing == Wearing::Armour);
    CHECK(full.armour[0] == TYPE(ITEM_GROUP_HELM, 1));
    CHECK(full.armour[1] == TYPE(ITEM_GROUP_ARMOR, 1));
    CHECK(full.armour[2] == TYPE(ITEM_GROUP_PANTS, 1));
    CHECK(full.armour[3] == TYPE(ITEM_GROUP_GLOVES, 1));
    CHECK(full.armour[4] == TYPE(ITEM_GROUP_BOOTS, 1));
    CHECK(full.rightHand == NO_ITEM);

    const Outfit noHelm = DressFor(TYPE(ITEM_GROUP_ARMOR, 2), EQUIPMENT_ARMOR, BowKind::None, Exists);
    CHECK(noHelm.armour[0] == NO_ITEM);
    CHECK(noHelm.armour[1] == TYPE(ITEM_GROUP_ARMOR, 2));

    // The selected part is worn even if `exists` does not know it.
    const Outfit alone = DressFor(TYPE(ITEM_GROUP_BOOTS, 40), EQUIPMENT_BOOTS, BowKind::None, Exists);
    CHECK(alone.armour[4] == TYPE(ITEM_GROUP_BOOTS, 40));
    CHECK(alone.armour[1] == NO_ITEM);
}

TEST_CASE("Preview outfit: wings on the back, jewellery not drawn, the rest not wearable")
{
    const int wings = TYPE(ITEM_GROUP_WING, 1);
    const Outfit back = DressFor(wings, EQUIPMENT_WING, BowKind::None, Exists);
    CHECK(back.wearing == Wearing::Wings);
    CHECK(back.wings == wings);

    CHECK(DressFor(TYPE(ITEM_GROUP_HELPER, 8), EQUIPMENT_RING_LEFT, BowKind::None, Exists).wearing ==
          Wearing::NotShown);
    CHECK(DressFor(TYPE(ITEM_GROUP_HELPER, 0), EQUIPMENT_HELPER, BowKind::None, Exists).wearing ==
          Wearing::NotShown);
    constexpr int NOT_EQUIPPABLE = 255;
    const Outfit jewel = DressFor(TYPE(ITEM_GROUP_WING, 15), NOT_EQUIPPABLE, BowKind::None, Exists);
    CHECK(jewel.wearing == Wearing::NotWearable);
    CHECK(jewel.wings == NO_ITEM);
    CHECK(WearingLabel(Wearing::NotWearable)[0] != '\0');
}

TEST_CASE("Preview class: the filter's class wins, else the first class that may equip the item")
{
    constexpr RequireClass ELF_ONLY = {0, 0, 2, 0, 0, 0, 0};
    constexpr RequireClass DW_AND_DK = {1, 1, 0, 0, 0, 0, 0};
    constexpr RequireClass NOBODY = {0, 0, 0, 0, 0, 0, 0};

    const ClassChoice filtered = PreviewClass(ELF_ONLY, CLASS_KNIGHT, 3);
    CHECK(filtered.baseClass == CLASS_KNIGHT);
    CHECK(filtered.stage == 3);

    const ClassChoice elf = PreviewClass(ELF_ONLY, NO_CLASS_FILTER, 1);
    CHECK(elf.baseClass == CLASS_ELF);
    CHECK(elf.stage == 2);

    CHECK(PreviewClass(DW_AND_DK, NO_CLASS_FILTER, 1).baseClass == CLASS_WIZARD);
    CHECK(PreviewClass(NOBODY, NO_CLASS_FILTER, 1).baseClass == CLASS_KNIGHT);
}

TEST_CASE("Preview class: stages map to the game's classes")
{
    CHECK(ClassAtStage(CLASS_KNIGHT, 1) == CLASS_KNIGHT);
    CHECK(ClassAtStage(CLASS_KNIGHT, 2) == CLASS_BLADEKNIGHT);
    CHECK(ClassAtStage(CLASS_KNIGHT, 3) == CLASS_BLADEMASTER);
    CHECK(ClassAtStage(CLASS_DARK, 2) == CLASS_DARK); // no second class
    CHECK(ClassAtStage(CLASS_DARK, 3) == CLASS_DUELMASTER);
    CHECK(ClassAtStage(CLASS_RAGEFIGHTER, 3) == CLASS_TEMPLENIGHT);
    CHECK(ClassAtStage(CLASS_SUMMONER, 9) == CLASS_DIMENSIONMASTER); // clamped
}
