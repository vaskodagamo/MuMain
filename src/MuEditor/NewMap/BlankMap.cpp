#include "BlankMap.h"

#ifdef _EDITOR

#include "Assets/TerrainHeightFile.h"
#include "Assets/TerrainLightFile.h"
#include "Engine/Object/WorldObjectFile.h"
#include "Render/Terrain/TerrainFiles.h"

#include <algorithm>

namespace Editor::NewMap
{
namespace
{
namespace TerrainFiles = Render::Terrain::Files;

constexpr std::uint8_t FILE_VERSION = 0; // every shipped EncTerrain file has version 0
constexpr std::uint8_t NO_OVERLAY_TILE = 255;
constexpr std::uint8_t NO_OPACITY = 0;
constexpr std::uint8_t ATTRIBUTE_EXTENT = 255;

// The BMP inside TerrainHeight.OZB: its bit count at offset 28, its pixels after a 1078-byte
// header. The client reads the corners from 1080 on (Editor::HeightMap), so every byte
// from 1078 to the end is a height.
constexpr std::size_t BMP_BIT_COUNT_OFFSET = 28;
constexpr std::uint8_t HEIGHT_BITS_PER_PIXEL = 8;
constexpr std::size_t BMP_PIXEL_OFFSET = 1078;

Bytes Header(int folder)
{
    return {FILE_VERSION, static_cast<std::uint8_t>(folder)};
}
} // namespace

Bytes BlankMappingPlain(int folder, std::uint8_t tileSlot)
{
    Bytes plain = Header(folder);
    plain.reserve(TerrainFiles::MAPPING_BYTES);
    plain.insert(plain.end(), TerrainFiles::CELLS, tileSlot);
    plain.insert(plain.end(), TerrainFiles::CELLS, NO_OVERLAY_TILE);
    plain.insert(plain.end(), TerrainFiles::CELLS, NO_OPACITY);
    return plain;
}

Bytes BlankAttributePlain(int folder, std::uint8_t attribute)
{
    Bytes plain = Header(folder);
    plain.push_back(ATTRIBUTE_EXTENT);
    plain.push_back(ATTRIBUTE_EXTENT);
    plain.insert(plain.end(), TerrainFiles::CELLS, attribute);
    return plain;
}

Bytes EmptyObjectsPlain(int folder)
{
    Engine::Object::WorldObjectFile::Contents contents;
    contents.version = FILE_VERSION;
    contents.mapNumber = static_cast<std::uint8_t>(folder);
    return Engine::Object::WorldObjectFile::Encode(contents);
}

bool IsEightBitHeightFile(const Bytes& file)
{
    const std::size_t bitCountAt = Editor::HeightMap::OZB_PREFIX_BYTES + BMP_BIT_COUNT_OFFSET;
    return file.size() == Editor::HeightMap::OZB_BYTES && file[bitCountAt] == HEIGHT_BITS_PER_PIXEL;
}

bool FlatHeightFile(const Bytes& templateFile, std::uint8_t heightByte, Bytes& out, std::string& error)
{
    if (!IsEightBitHeightFile(templateFile))
    {
        error = "the template height file is not an 8-bit TerrainHeight.OZB of " +
                std::to_string(Editor::HeightMap::OZB_BYTES) + " bytes";
        return false;
    }
    out = templateFile;
    std::fill(out.begin() + static_cast<std::ptrdiff_t>(Editor::HeightMap::OZB_PREFIX_BYTES + BMP_PIXEL_OFFSET),
              out.end(), heightByte);
    return true;
}

Bytes FlatLightFile(const std::array<float, 3>& rgb)
{
    constexpr int SIZE = Editor::LightMap::LIGHT_MAP_SIZE;
    std::vector<float> light(static_cast<std::size_t>(SIZE) * SIZE * rgb.size());
    for (std::size_t i = 0; i < light.size(); ++i)
        light[i] = rgb[i % rgb.size()];
    return Editor::LightMap::EncodeOzj(light.data(), SIZE);
}
} // namespace Editor::NewMap

#endif // _EDITOR
