#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// The byte layouts of a map's terrain files, checked before anything is copied out of
// them: a file cut short (or edited by hand) is refused with a reason instead of being
// read past its end. Plain code, no engine state.
namespace Render::Terrain::Files
{
// Cells of every terrain layer (256 x 256; ZzzLodTerrain checks TERRAIN_SIZE agrees).
constexpr std::size_t CELLS = 256 * 256;

// EncTerrain{N}.map after MapFileDecrypt: version, map number, layer 1's texture slots,
// layer 2's texture slots, then layer 2's opacity (0..255), one byte per cell each.
constexpr std::size_t MAPPING_HEADER_BYTES = 2;
constexpr std::size_t MAPPING_BYTES = MAPPING_HEADER_BYTES + 3 * CELLS;

struct MappingLayers
{
    std::uint8_t* layer1 = nullptr;
    std::uint8_t* layer2 = nullptr;
    float* alpha = nullptr; // 0..1
};

// Fills `layers` from a decrypted mapping file and returns the map number from its
// header. -1, with the reason in `error` and nothing written, when it is shorter than
// MAPPING_BYTES.
int DecodeMapping(const std::uint8_t* data, std::size_t size, const MappingLayers& layers, std::string& error);

// TerrainHeight.OZB of the maps with 24-bit heights (OpenTerrainHeightNew): a 4-byte
// prefix, a 14-byte BMP file header and a 40-byte info header, then three bytes per
// cell, the height's low, middle and high byte.
constexpr std::size_t EXTENDED_HEIGHT_PREFIX_BYTES = 4;
constexpr std::size_t BMP_FILE_HEADER_BYTES = 14;
constexpr std::size_t BMP_INFO_HEADER_BYTES = 40;
constexpr std::size_t EXTENDED_HEIGHT_BYTES_PER_CELL = 3;
constexpr std::size_t EXTENDED_HEIGHT_BYTES = EXTENDED_HEIGHT_PREFIX_BYTES + BMP_FILE_HEADER_BYTES +
                                              BMP_INFO_HEADER_BYTES + EXTENDED_HEIGHT_BYTES_PER_CELL * CELLS;

// Fills `heights` (CELLS floats) with each cell's height plus `baseHeight`. False, with
// the reason in `error` and nothing written, when the file is shorter than
// EXTENDED_HEIGHT_BYTES.
bool DecodeExtendedHeight(const std::uint8_t* data, std::size_t size, float baseHeight, float* heights,
                          std::string& error);
} // namespace Render::Terrain::Files
