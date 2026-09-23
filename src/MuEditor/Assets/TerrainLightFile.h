#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The painted light map of a map, Data/World{N}/TerrainLight.OZJ: 24 prefix bytes
// (the JPEG's own first 24 bytes, as tools/mu_texture.py wraps textures; the loader
// skips them) and a JPEG of 256 x 256 RGB pixels. The client's loader
// (OpenJpegBuffer) decodes it bottom row first, so the JPEG's bottom row is the map's
// row y = 0, and turns each byte b into the light value b / 255.
namespace Editor::LightMap
{
constexpr std::size_t OZJ_PREFIX_BYTES = 24;
constexpr int LIGHT_MAP_SIZE = 256;
// Highest quality, no chroma subsampling: the least loss a JPEG allows.
constexpr int LIGHT_MAP_JPEG_QUALITY = 100;

// The .OZJ file for `rgb`: size x size corners, three floats (0..1) each, row by row
// from y = 0 as TerrainLight holds them. Each value is rounded to the nearest byte.
// Empty when the encoder fails.
std::vector<std::uint8_t> EncodeOzj(const float* rgb, int size);

// Reads an .OZJ into size x size x 3 floats exactly as the client's loader does
// (b / 255, clamped to 0..1). False with `error` when the file is not a JPEG of
// size x size pixels (the loader would write past the light map).
bool DecodeOzj(const std::vector<std::uint8_t>& file, int size, float* rgb, std::string& error);
} // namespace Editor::LightMap

#endif // _EDITOR
