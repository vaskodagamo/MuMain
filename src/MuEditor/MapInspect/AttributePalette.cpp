#include "AttributePalette.h"

#ifdef _EDITOR

#include "AttributeBits.h"

namespace Editor::MapInspect
{
namespace
{
constexpr std::uint16_t STORED_BITS = 0x00FF;

constexpr Rgb NOGROUND_COLOR = {0, 0, 0};
constexpr Rgb NOMOVE_COLOR = {150, 40, 40};
constexpr Rgb SAFE_NOMOVE_COLOR = {35, 105, 45};
constexpr Rgb WATER_COLOR = {40, 90, 220};
constexpr Rgb HEIGHT_COLOR = {200, 80, 200};
constexpr Rgb ACTION_COLOR = {230, 200, 50};
constexpr Rgb CAMERA_UP_COLOR = {80, 200, 220};
constexpr Rgb NOATTACK_COLOR = {230, 140, 40};
constexpr Rgb SAFEZONE_COLOR = {90, 210, 90};
constexpr Rgb WALKABLE_COLOR = {170, 170, 170};

bool Has(std::uint16_t value, std::uint16_t bit)
{
    return (value & bit) != 0;
}
} // namespace

const std::vector<AttributeFlag>& AttributeFlags()
{
    static const std::vector<AttributeFlag> flags = {
        {Attribute::SAFEZONE, "safezone"}, {Attribute::CHARACTER, "character"}, {Attribute::NOMOVE, "nomove"},
        {Attribute::NOGROUND, "noground"}, {Attribute::WATER, "water"},         {Attribute::ACTION, "action"},
        {Attribute::HEIGHT, "height"},     {Attribute::CAMERA_UP, "camera_up"}, {Attribute::NOATTACK, "noattack"},
        {Attribute::ATT1, "att1"},         {Attribute::ATT2, "att2"},           {Attribute::ATT3, "att3"},
        {Attribute::ATT4, "att4"},         {Attribute::ATT5, "att5"},           {Attribute::ATT6, "att6"},
        {Attribute::ATT7, "att7"},
    };
    return flags;
}

std::vector<std::string> AttributeFlagNames(std::uint16_t value)
{
    std::vector<std::string> names;
    for (const AttributeFlag& flag : AttributeFlags())
    {
        if (Has(value, flag.bit))
            names.emplace_back(flag.name);
    }
    return names;
}

std::uint16_t StoredAttribute(std::uint16_t value)
{
    return static_cast<std::uint16_t>(value & STORED_BITS & ~Attribute::CHARACTER);
}

Rgb AttributeColor(std::uint16_t value)
{
    if (Has(value, Attribute::NOGROUND))
        return NOGROUND_COLOR;
    if (Has(value, Attribute::NOMOVE))
        return Has(value, Attribute::SAFEZONE) ? SAFE_NOMOVE_COLOR : NOMOVE_COLOR;
    if (Has(value, Attribute::WATER))
        return WATER_COLOR;
    if (Has(value, Attribute::HEIGHT))
        return HEIGHT_COLOR;
    if (Has(value, Attribute::ACTION))
        return ACTION_COLOR;
    if (Has(value, Attribute::CAMERA_UP))
        return CAMERA_UP_COLOR;
    if (Has(value, Attribute::NOATTACK))
        return NOATTACK_COLOR;
    if (Has(value, Attribute::SAFEZONE))
        return SAFEZONE_COLOR;
    return WALKABLE_COLOR;
}

const std::vector<AttributeColorRule>& AttributeColorRules()
{
    static const std::vector<AttributeColorRule> rules = {
        {"noground", NOGROUND_COLOR, "no ground: nothing is drawn, nobody walks"},
        {"nomove+safezone", SAFE_NOMOVE_COLOR, "blocked tile inside a safe zone (town walls, houses)"},
        {"nomove", NOMOVE_COLOR, "blocked tile"},
        {"water", WATER_COLOR, "water"},
        {"height", HEIGHT_COLOR, "ground raised to the special height"},
        {"action", ACTION_COLOR, "action tile"},
        {"camera_up", CAMERA_UP_COLOR, "the camera rises here"},
        {"noattack", NOATTACK_COLOR, "no attacks here"},
        {"safezone", SAFEZONE_COLOR, "walkable safe zone"},
        {"", WALKABLE_COLOR, "walkable"},
    };
    return rules;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
