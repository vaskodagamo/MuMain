#pragma once

#include "Core/Globals/_enum.h" // CLASS_TYPE, MAX_CLASS

#include <cstdint>
#include <span>

// Which classes may equip an item, by its ITEM_ATTRIBUTE::RequireClass: the
// class part of IsRequireEquipItem (ZzzInfomation.cpp), which then also checks
// the hero's stats and level. Kept free of the hero so the editor's item browser
// filters by exactly the rule the game applies.
//
// RequireClass holds one entry per base class in CLASS_TYPE order (DW, DK, Elf,
// MG, DL, SUM, RF): the lowest class stage that may use the item, 0 = never.
// Stages count from 1 (base class), 2 (second class, e.g. Blade Knight),
// 3 (third class, e.g. Blade Master); Magic Gladiator, Dark Lord and Rage Fighter
// have no second class, so they are at stage 1 or 3.
//
// The one exception, as in the original client: a Magic Gladiator may also use
// an item that both the Dark Wizard and the Dark Knight may use, even when the
// item's MG entry is 0.
namespace GameLogic::Items
{
bool CanClassEquip(std::span<const std::uint8_t, MAX_CLASS> requireClass, CLASS_TYPE baseClass,
                   std::uint8_t classStage);
} // namespace GameLogic::Items
