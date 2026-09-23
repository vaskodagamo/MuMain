#include "TilePalette.h"

#ifdef _EDITOR

#include <array>
#include <cmath>
#include <cstdio>

namespace Editor::MapInspect
{
namespace
{
struct NamedSlot
{
    const char* name;
    Rgb color;
};

// Slots 0..13 in the order MapManager's terrain load fills them, coloured after
// their material so an exported texture layer reads like a map.
constexpr std::array<NamedSlot, 14> NAMED_SLOTS = {{
    {"TileGrass01", {60, 150, 60}},
    {"TileGrass02", {120, 190, 70}},
    {"TileGround01", {150, 110, 70}},
    {"TileGround02", {195, 155, 105}},
    {"TileGround03", {110, 80, 50}},
    {"TileWater01", {40, 90, 200}},
    {"TileWood01", {170, 90, 30}},
    {"TileRock01", {130, 130, 130}},
    {"TileRock02", {175, 175, 175}},
    {"TileRock03", {90, 90, 105}},
    {"TileRock04", {205, 200, 180}},
    {"TileRock05", {125, 100, 145}},
    {"TileRock06", {155, 120, 165}},
    {"TileRock07", {80, 70, 60}},
}};

constexpr int FIRST_EXT_SLOT = static_cast<int>(NAMED_SLOTS.size());
constexpr const char* EXT_TILE_PREFIX = "ExtTile";
constexpr const char* NO_OVERLAY_NAME = "none";
constexpr const char* UNKNOWN_SLOT_PREFIX = "slot";
constexpr Rgb NO_OVERLAY_COLOR = {0, 0, 0};

// The ExtTile slots and anything past them get hues a golden angle apart, so
// neighbouring slot numbers never look alike.
constexpr float GOLDEN_ANGLE_DEGREES = 137.508f;
constexpr float FULL_TURN_DEGREES = 360.0f;
constexpr float HUE_SECTOR_DEGREES = 60.0f;
constexpr float EXT_SATURATION = 0.8f;
constexpr float EXT_VALUE = 0.95f;
constexpr float CHANNEL_MAX = 255.0f;
constexpr float ROUND_HALF = 0.5f;

std::uint8_t ToChannel(float value)
{
    return static_cast<std::uint8_t>(value * CHANNEL_MAX + ROUND_HALF);
}

struct Channels
{
    float r;
    float g;
    float b;
};

// The strongest and second channel of a hue in one of the six 60-degree sectors.
Channels SectorChannels(int sector, float chroma, float second)
{
    switch (sector)
    {
    case 0:
        return {chroma, second, 0.0f};
    case 1:
        return {second, chroma, 0.0f};
    case 2:
        return {0.0f, chroma, second};
    case 3:
        return {0.0f, second, chroma};
    case 4:
        return {second, 0.0f, chroma};
    default:
        return {chroma, 0.0f, second};
    }
}

Rgb FromHsv(float hueDegrees, float saturation, float value)
{
    const float chroma = value * saturation;
    const float sector = hueDegrees / HUE_SECTOR_DEGREES;
    const float second = chroma * (1.0f - std::fabs(std::fmod(sector, 2.0f) - 1.0f));
    const float base = value - chroma;
    const Channels channels = SectorChannels(static_cast<int>(sector), chroma, second);
    return Rgb{ToChannel(channels.r + base), ToChannel(channels.g + base), ToChannel(channels.b + base)};
}
} // namespace

std::string TileSlotName(int slot)
{
    if (slot == NO_OVERLAY_TILE)
        return NO_OVERLAY_NAME;
    if (slot >= 0 && slot < FIRST_EXT_SLOT)
        return NAMED_SLOTS[static_cast<std::size_t>(slot)].name;
    char name[32];
    if (slot >= FIRST_EXT_SLOT && slot < TILE_SLOT_COUNT)
        std::snprintf(name, sizeof(name), "%s%02d", EXT_TILE_PREFIX, slot - FIRST_EXT_SLOT + 1);
    else
        std::snprintf(name, sizeof(name), "%s%d", UNKNOWN_SLOT_PREFIX, slot);
    return name;
}

Rgb TileSlotColor(int slot)
{
    if (slot == NO_OVERLAY_TILE)
        return NO_OVERLAY_COLOR;
    if (slot >= 0 && slot < FIRST_EXT_SLOT)
        return NAMED_SLOTS[static_cast<std::size_t>(slot)].color;
    const float hue = std::fmod(static_cast<float>(slot - FIRST_EXT_SLOT) * GOLDEN_ANGLE_DEGREES, FULL_TURN_DEGREES);
    return FromHsv(hue < 0.0f ? hue + FULL_TURN_DEGREES : hue, EXT_SATURATION, EXT_VALUE);
}
} // namespace Editor::MapInspect

#endif // _EDITOR
