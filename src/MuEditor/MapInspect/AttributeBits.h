#pragma once

#ifdef _EDITOR

#include <cstdint>

// The TW_* bits of TerrainWall (Core/Globals/_define.h), named for the inspection
// units, which do not include engine headers. Editor::LiveMap checks that they agree.
namespace Editor::MapInspect::Attribute
{
constexpr std::uint16_t SAFEZONE = 0x0001;
constexpr std::uint16_t CHARACTER = 0x0002;
constexpr std::uint16_t NOMOVE = 0x0004;
constexpr std::uint16_t NOGROUND = 0x0008;
constexpr std::uint16_t WATER = 0x0010;
constexpr std::uint16_t ACTION = 0x0020;
constexpr std::uint16_t HEIGHT = 0x0040;
constexpr std::uint16_t CAMERA_UP = 0x0080;
constexpr std::uint16_t NOATTACK = 0x0100;
constexpr std::uint16_t ATT1 = 0x0200;
constexpr std::uint16_t ATT2 = 0x0400;
constexpr std::uint16_t ATT3 = 0x0800;
constexpr std::uint16_t ATT4 = 0x1000;
constexpr std::uint16_t ATT5 = 0x2000;
constexpr std::uint16_t ATT6 = 0x4000;
constexpr std::uint16_t ATT7 = 0x8000;
} // namespace Editor::MapInspect::Attribute

#endif // _EDITOR
