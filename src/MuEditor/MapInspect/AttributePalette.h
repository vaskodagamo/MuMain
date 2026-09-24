#pragma once

#ifdef _EDITOR

#include "Image.h" // Rgb

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The walkability attribute of a tile (TerrainWall, the TW_* bits of the map's .att)
// as scripted clients see it: flag names and the colours of the exported image.
namespace Editor::MapInspect
{
struct AttributeFlag
{
    std::uint16_t bit;
    std::string_view name;
};

// Every TW_* bit with its protocol name: safezone, character, nomove, noground,
// water, action, height, camera_up, noattack, att1 ... att7.
const std::vector<AttributeFlag>& AttributeFlags();

// The names of the bits set in `value`, lowest bit first.
std::vector<std::string> AttributeFlagNames(std::uint16_t value);

// The bits a map's .att keeps: the low byte, without the "a character stands here"
// bit the game sets while it runs.
std::uint16_t StoredAttribute(std::uint16_t value);

// The colour of a tile in the exported attribute image. The strongest property wins:
// noground black, nomove dark red (dark green inside a safe zone), water blue,
// height magenta, action yellow, camera_up cyan, noattack orange, safezone green,
// walkable light grey.
Rgb AttributeColor(std::uint16_t value);

struct AttributeColorRule
{
    std::string_view flag; // "" for the walkable default
    Rgb color;
    std::string_view meaning;
};

// The rules AttributeColor applies, strongest first, for the export legend.
const std::vector<AttributeColorRule>& AttributeColorRules();
} // namespace Editor::MapInspect

#endif // _EDITOR
