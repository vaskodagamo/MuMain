#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>

// Read-only map inspection for scripted clients (the control socket's map-* commands):
// what the loaded map looks like as numbers and images. No ImGui, no engine state;
// Editor::LiveMap hands these units the live arrays.
namespace Editor::MapInspect
{
// Every map is this many tiles along each side.
constexpr int MAP_TILES = 256;
constexpr int MAP_CELLS = MAP_TILES * MAP_TILES;
// World units per tile (the engine's TERRAIN_SCALE).
constexpr float TILE_WORLD_SIZE = 100.0f;

// The terrain arrays of one map, 256 x 256 cells each, stored row by row
// (index y * 256 + x). Views, not copies: the arrays belong to the caller.
struct TerrainView
{
    const std::uint8_t* baseTiles = nullptr;    // texture slot of layer 1 per cell
    const std::uint8_t* overlayTiles = nullptr; // texture slot of layer 2, 255 = none
    const float* overlayAlpha = nullptr;        // layer 2's opacity, 0..1
    const float* height = nullptr;              // world units
    const std::uint16_t* attribute = nullptr;   // TW_* walkability bits
    const float* light = nullptr;               // painted light map, three floats (r, g, b) per cell, 0..1
};

// The index of cell (x, y) in every array of a TerrainView.
constexpr std::size_t CellIndex(int x, int y)
{
    return static_cast<std::size_t>(y) * MAP_TILES + static_cast<std::size_t>(x);
}
} // namespace Editor::MapInspect

#endif // _EDITOR
