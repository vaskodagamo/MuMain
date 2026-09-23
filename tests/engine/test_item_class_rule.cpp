#include "App/stdafx.h"

#include <doctest.h>

#include "GameLogic/Items/ItemClassRule.h"

#include <array>
#include <cstdint>

using GameLogic::Items::CanClassEquip;

namespace
{
using RequireClass = std::array<std::uint8_t, MAX_CLASS>;

constexpr std::uint8_t BASE = 1;
constexpr std::uint8_t SECOND = 2;
constexpr std::uint8_t THIRD = 3;

// RequireClass in CLASS_TYPE order: DW, DK, Elf, MG, DL, SUM, RF.
constexpr RequireClass NOT_FOR_RF = {1, 1, 1, 1, 1, 1, 0};
constexpr RequireClass KNIGHT_BLADE = {0, 2, 0, 0, 0, 0, 0};   // 0-20: second-class Dark Knights
constexpr RequireClass DW_AND_DK_ONLY = {1, 1, 0, 0, 0, 0, 0}; // no MG entry
constexpr RequireClass DW_AND_DK_THIRD = {3, 3, 0, 0, 0, 0, 0};
constexpr RequireClass DK_ONLY = {0, 1, 0, 0, 0, 0, 0};
constexpr RequireClass NOBODY = {0, 0, 0, 0, 0, 0, 0};
} // namespace

TEST_CASE("an item is usable from the stage its class entry names")
{
    CHECK(CanClassEquip(NOT_FOR_RF, CLASS_KNIGHT, BASE));
    CHECK(CanClassEquip(NOT_FOR_RF, CLASS_ELF, THIRD));
    CHECK_FALSE(CanClassEquip(NOT_FOR_RF, CLASS_RAGEFIGHTER, THIRD));

    CHECK_FALSE(CanClassEquip(KNIGHT_BLADE, CLASS_KNIGHT, BASE));
    CHECK(CanClassEquip(KNIGHT_BLADE, CLASS_KNIGHT, SECOND));
    CHECK(CanClassEquip(KNIGHT_BLADE, CLASS_KNIGHT, THIRD));
    CHECK_FALSE(CanClassEquip(KNIGHT_BLADE, CLASS_WIZARD, THIRD));
}

TEST_CASE("nobody may use an item whose class entries are all zero")
{
    for (int baseClass = CLASS_WIZARD; baseClass <= CLASS_RAGEFIGHTER; ++baseClass)
        CHECK_FALSE(CanClassEquip(NOBODY, static_cast<CLASS_TYPE>(baseClass), THIRD));
}

TEST_CASE("a Magic Gladiator may use what the Dark Wizard and Dark Knight both may")
{
    CHECK(CanClassEquip(DW_AND_DK_ONLY, CLASS_DARK, BASE));
    // The shared rule ignores the stage: the MG entry (0) is what the stage is checked against.
    CHECK(CanClassEquip(DW_AND_DK_THIRD, CLASS_DARK, BASE));
    // Only the Dark Knight: not shared.
    CHECK_FALSE(CanClassEquip(DK_ONLY, CLASS_DARK, THIRD));
    // Only for MG: the other classes get nothing from it.
    CHECK_FALSE(CanClassEquip(DW_AND_DK_ONLY, CLASS_ELF, THIRD));
    CHECK_FALSE(CanClassEquip(DW_AND_DK_ONLY, CLASS_DARK_LORD, THIRD));
}

TEST_CASE("a class outside the base classes may use nothing")
{
    CHECK_FALSE(CanClassEquip(NOT_FOR_RF, CLASS_SOULMASTER, THIRD));
    CHECK_FALSE(CanClassEquip(NOT_FOR_RF, CLASS_UNDEFINED, THIRD));
}
