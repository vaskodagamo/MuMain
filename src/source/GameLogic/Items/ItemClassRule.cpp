#include "stdafx.h"

#include "GameLogic/Items/ItemClassRule.h"

namespace GameLogic::Items
{
namespace
{
// A Magic Gladiator may use what the Dark Wizard and the Dark Knight may both use.
bool MagicGladiatorSharesItem(std::span<const std::uint8_t, MAX_CLASS> requireClass, CLASS_TYPE baseClass)
{
    return baseClass == CLASS_DARK && requireClass[CLASS_WIZARD] != 0 && requireClass[CLASS_KNIGHT] != 0;
}
} // namespace

bool CanClassEquip(std::span<const std::uint8_t, MAX_CLASS> requireClass, CLASS_TYPE baseClass,
                   std::uint8_t classStage)
{
    if (baseClass >= MAX_CLASS)
        return false;

    const std::uint8_t requiredStage = requireClass[baseClass];
    const bool classAllowed = requiredStage != 0 || MagicGladiatorSharesItem(requireClass, baseClass);
    if (!classAllowed)
        return false;

    return requiredStage <= classStage;
}
} // namespace GameLogic::Items
