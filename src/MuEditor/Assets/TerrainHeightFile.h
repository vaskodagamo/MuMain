#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The heights of most maps, Data/World{N}/TerrainHeight.OZB: 4 prefix bytes, a BMP's
// 1080-byte header (file and info header and a 256-entry palette), then one byte per
// corner, row y = 0 first, as the client's OpenTerrainHeight reads it. A corner's height
// is its byte times the map's factor (1.5; 3.0 on the login scene). A few maps use a
// 24-bit layout instead (Render::Terrain::Files::DecodeExtendedHeight).
namespace Editor::HeightMap
{
constexpr std::size_t OZB_PREFIX_BYTES = 4;
constexpr std::size_t BMP_HEADER_BYTES = 1080;
constexpr int HEIGHT_MAP_SIZE = 256;
constexpr std::size_t OZB_BYTES =
    OZB_PREFIX_BYTES + BMP_HEADER_BYTES + static_cast<std::size_t>(HEIGHT_MAP_SIZE) * HEIGHT_MAP_SIZE;

// Fills `heights` (256 x 256 floats) from the file as the client loads it. False with
// the reason, and nothing written, when the file is shorter than OZB_BYTES (the loader
// refuses those too).
bool DecodeOzb(const std::vector<std::uint8_t>& file, float factor, float* heights, std::string& error);
} // namespace Editor::HeightMap

#endif // _EDITOR
