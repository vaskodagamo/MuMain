#include "TerrainFiles.h"

#include <algorithm>

namespace Render::Terrain::Files
{
namespace
{
constexpr std::size_t MAP_NUMBER_OFFSET = 1;
constexpr float ALPHA_MAX = 255.0f;
constexpr int BITS_PER_BYTE = 8;

std::string TooShort(const char* what, std::size_t size, std::size_t needed)
{
    return std::string(what) + " is " + std::to_string(size) + " bytes, needs " + std::to_string(needed);
}
} // namespace

int DecodeMapping(const std::uint8_t* data, std::size_t size, const MappingLayers& layers, std::string& error)
{
    if (data == nullptr || size < MAPPING_BYTES)
    {
        error = TooShort("the terrain mapping", data == nullptr ? 0 : size, MAPPING_BYTES);
        return -1;
    }

    const std::uint8_t* layer1 = data + MAPPING_HEADER_BYTES;
    const std::uint8_t* layer2 = layer1 + CELLS;
    const std::uint8_t* alpha = layer2 + CELLS;
    std::copy_n(layer1, CELLS, layers.layer1);
    std::copy_n(layer2, CELLS, layers.layer2);
    for (std::size_t cell = 0; cell < CELLS; ++cell)
        layers.alpha[cell] = static_cast<float>(alpha[cell]) / ALPHA_MAX;
    return data[MAP_NUMBER_OFFSET];
}

bool DecodeExtendedHeight(const std::uint8_t* data, std::size_t size, float baseHeight, float* heights,
                          std::string& error)
{
    if (data == nullptr || size < EXTENDED_HEIGHT_BYTES)
    {
        error = TooShort("the height map", data == nullptr ? 0 : size, EXTENDED_HEIGHT_BYTES);
        return false;
    }

    const std::uint8_t* cells = data + EXTENDED_HEIGHT_PREFIX_BYTES + BMP_FILE_HEADER_BYTES + BMP_INFO_HEADER_BYTES;
    for (std::size_t cell = 0; cell < CELLS; ++cell)
    {
        const std::uint8_t* bytes = cells + cell * EXTENDED_HEIGHT_BYTES_PER_CELL;
        // Stored blue, green, red: the red byte is the height's lowest.
        const std::uint32_t height = static_cast<std::uint32_t>(bytes[2]) |
                                     (static_cast<std::uint32_t>(bytes[1]) << BITS_PER_BYTE) |
                                     (static_cast<std::uint32_t>(bytes[0]) << (2 * BITS_PER_BYTE));
        heights[cell] = static_cast<float>(height) + baseHeight;
    }
    return true;
}
} // namespace Render::Terrain::Files
