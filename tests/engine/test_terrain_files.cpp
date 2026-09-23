// doctest unit tests for the terrain loaders' bounds: a mapping or height file cut
// short, a light map of the wrong size and the tiles of the map's last row must be
// refused or kept inside the terrain arrays, while complete files load exactly as
// before.
//
// Run: ctest --test-dir <build directory> --build-config Release -R "\[terrain-files\]"

#include <doctest.h>

#include "Render/Terrain/TerrainFiles.h"
#include "Render/Terrain/TerrainTileIndex.h"
#include "Render/Textures/JpegFloatDecode.h"

#include "turbojpeg.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Files = Render::Terrain::Files;
namespace TileIndex = Render::Terrain::TileIndex;

namespace
{
constexpr float UNTOUCHED = -7.0f;

// A decrypted mapping file: layer 1 counts up, layer 2 counts down, the opacity is
// the cell's low byte.
std::vector<std::uint8_t> MappingFile(std::uint8_t mapNumber)
{
    std::vector<std::uint8_t> data(Files::MAPPING_BYTES);
    data[1] = mapNumber;
    for (std::size_t cell = 0; cell < Files::CELLS; ++cell)
    {
        data[Files::MAPPING_HEADER_BYTES + cell] = static_cast<std::uint8_t>(cell % 30);
        data[Files::MAPPING_HEADER_BYTES + Files::CELLS + cell] = static_cast<std::uint8_t>(255 - cell % 30);
        data[Files::MAPPING_HEADER_BYTES + 2 * Files::CELLS + cell] = static_cast<std::uint8_t>(cell & 0xFF);
    }
    return data;
}

struct Layers
{
    Layers() : layer1(Files::CELLS, 0), layer2(Files::CELLS, 0), alpha(Files::CELLS, UNTOUCHED) {}
    Files::MappingLayers View()
    {
        return Files::MappingLayers{layer1.data(), layer2.data(), alpha.data()};
    }
    std::vector<std::uint8_t> layer1;
    std::vector<std::uint8_t> layer2;
    std::vector<float> alpha;
};

// A grey JPEG of the given size, as tjCompress2 writes it.
std::vector<unsigned char> GreyJpeg(int width, int height)
{
    const std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3, 128);
    tjhandle compressor = tjInitCompress();
    REQUIRE(compressor != nullptr);
    unsigned char* jpeg = nullptr;
    unsigned long size = 0;
    const int result =
        tjCompress2(compressor, pixels.data(), width, 0, height, TJPF_RGB, &jpeg, &size, TJSAMP_444, 100, 0);
    tjDestroy(compressor);
    REQUIRE(result == 0);
    std::vector<unsigned char> bytes(jpeg, jpeg + size);
    tjFree(jpeg);
    return bytes;
}
} // namespace

TEST_CASE("A complete mapping file fills both tile layers and the opacity [engine][terrain-files]")
{
    const std::vector<std::uint8_t> data = MappingFile(3);
    Layers layers;
    std::string error;
    CHECK(Files::DecodeMapping(data.data(), data.size(), layers.View(), error) == 3);
    CHECK(layers.layer1[31] == 1);
    CHECK(layers.layer2[31] == 254);
    CHECK(layers.alpha[255] == doctest::Approx(1.0f));
    CHECK(layers.alpha[256] == 0.0f);
    CHECK(layers.alpha[Files::CELLS - 1] == doctest::Approx(1.0f));
}

TEST_CASE("A mapping file cut short is refused without writing anything [engine][terrain-files]")
{
    std::vector<std::uint8_t> data = MappingFile(3);
    data.resize(Files::MAPPING_BYTES - 1);
    Layers layers;
    std::string error;
    CHECK(Files::DecodeMapping(data.data(), data.size(), layers.View(), error) == -1);
    CHECK(error.find("needs " + std::to_string(Files::MAPPING_BYTES)) != std::string::npos);
    CHECK(layers.alpha[0] == UNTOUCHED);
    CHECK(layers.layer1[31] == 0);

    CHECK(Files::DecodeMapping(nullptr, 0, layers.View(), error) == -1);
}

TEST_CASE("Extended heights are read as 24-bit numbers, and a short file is refused [engine][terrain-files]")
{
    std::vector<std::uint8_t> data(Files::EXTENDED_HEIGHT_BYTES, 0);
    const std::size_t first =
        Files::EXTENDED_HEIGHT_PREFIX_BYTES + Files::BMP_FILE_HEADER_BYTES + Files::BMP_INFO_HEADER_BYTES;
    // Cell 0 stores blue, green, red = 0x01, 0x02, 0x03: the height is 0x010203.
    data[first] = 0x01;
    data[first + 1] = 0x02;
    data[first + 2] = 0x03;
    std::vector<float> heights(Files::CELLS, UNTOUCHED);
    std::string error;
    REQUIRE(Files::DecodeExtendedHeight(data.data(), data.size(), -500.0f, heights.data(), error));
    CHECK(heights[0] == static_cast<float>(0x010203) - 500.0f);
    CHECK(heights[1] == -500.0f);

    std::vector<float> untouched(Files::CELLS, UNTOUCHED);
    CHECK_FALSE(Files::DecodeExtendedHeight(data.data(), data.size() - 1, -500.0f, untouched.data(), error));
    CHECK(untouched[0] == UNTOUCHED);
    CHECK(error.find("height map") != std::string::npos);
}

TEST_CASE("Tiles read their corners inside the terrain arrays [engine][terrain-files]")
{
    constexpr int LAST = TileIndex::SIDE - 1;
    // Inside the map, and on column 255 below the last row, the index is unchanged.
    CHECK(TileIndex::Corner(10, 20) == 20 * TileIndex::SIDE + 10);
    CHECK(TileIndex::Corner(LAST + 1, 7) == 8 * TileIndex::SIDE);
    // The last row's top corners repeat the last row instead of reading past it.
    CHECK(TileIndex::Corner(4, LAST + 1) == LAST * TileIndex::SIDE + 4);
    CHECK(TileIndex::Corner(LAST + 1, LAST) == TileIndex::CELLS - 1);
    CHECK(TileIndex::Corner(LAST + 1, LAST + 1) == TileIndex::CELLS - 1);
    for (int x = 0; x <= LAST + 1; ++x)
    {
        CHECK(TileIndex::Corner(x, LAST + 1) < TileIndex::CELLS);
        CHECK(TileIndex::Corner(x, LAST) < TileIndex::CELLS);
    }
}

TEST_CASE("A light map decodes only at the size its buffer holds [engine][terrain-files]")
{
    const std::vector<unsigned char> small = GreyJpeg(8, 8);
    std::vector<float> light(8 * 8 * 3, UNTOUCHED);
    std::string error;
    REQUIRE(Render::Textures::DecodeJpegToFloats(small.data(), small.size(), 8, 8, light.data(), error));
    CHECK(light[0] == doctest::Approx(128.0f / 255.0f).epsilon(0.02));

    // A larger image would be written past the buffer: refused, nothing written.
    const std::vector<unsigned char> large = GreyJpeg(16, 8);
    std::vector<float> untouched(8 * 8 * 3, UNTOUCHED);
    CHECK_FALSE(Render::Textures::DecodeJpegToFloats(large.data(), large.size(), 8, 8, untouched.data(), error));
    CHECK(error.find("16 x 8") != std::string::npos);
    CHECK(untouched[0] == UNTOUCHED);

    const std::vector<unsigned char> garbage(64, 0x42);
    CHECK_FALSE(Render::Textures::DecodeJpegToFloats(garbage.data(), garbage.size(), 8, 8, untouched.data(), error));
}
