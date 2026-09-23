#include <doctest.h>

#include "Assets/TerrainLightFile.h"

#include "turbojpeg.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using Editor::LightMap::DecodeOzj;
using Editor::LightMap::EncodeOzj;
using Editor::LightMap::LIGHT_MAP_SIZE;
using Editor::LightMap::OZJ_PREFIX_BYTES;

namespace
{
using Bytes = std::vector<std::uint8_t>;
constexpr int CHANNELS = 3;
constexpr std::size_t VALUES = static_cast<std::size_t>(LIGHT_MAP_SIZE) * LIGHT_MAP_SIZE * CHANNELS;
// A JPEG at quality 100 without chroma subsampling moves a value by a few steps at most
// (up to 4.5 of 255 on the gradient below, from the conversion to and from YCbCr).
constexpr float MAX_JPEG_ERROR = 6.0f / 255.0f;

std::vector<float> Gradient()
{
    std::vector<float> rgb(VALUES);
    for (int y = 0; y < LIGHT_MAP_SIZE; ++y)
    {
        for (int x = 0; x < LIGHT_MAP_SIZE; ++x)
        {
            float* pixel = &rgb[(static_cast<std::size_t>(y) * LIGHT_MAP_SIZE + x) * CHANNELS];
            pixel[0] = static_cast<float>(x) / (LIGHT_MAP_SIZE - 1);
            pixel[1] = static_cast<float>(y) / (LIGHT_MAP_SIZE - 1);
            pixel[2] = 0.5f;
        }
    }
    return rgb;
}

float MaxDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    float largest = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i)
        largest = std::max(largest, std::fabs(a[i] - b[i]));
    return largest;
}

Bytes ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The JPEG's rows top row first, as an image viewer shows them.
std::vector<unsigned char> TopDownPixels(const Bytes& file)
{
    std::vector<unsigned char> pixels(VALUES);
    tjhandle handle = tjInitDecompress();
    unsigned char* jpeg = const_cast<unsigned char*>(file.data()) + OZJ_PREFIX_BYTES;
    tjDecompress2(handle, jpeg, static_cast<unsigned long>(file.size() - OZJ_PREFIX_BYTES), pixels.data(),
                  LIGHT_MAP_SIZE, 0, LIGHT_MAP_SIZE, TJPF_RGB, 0);
    tjDestroy(handle);
    return pixels;
}
} // namespace

TEST_CASE("A saved light map is a 256 x 256 JPEG behind its own first 24 bytes [editor][light]")
{
    const std::vector<float> light = Gradient();
    const Bytes file = EncodeOzj(light.data(), LIGHT_MAP_SIZE);
    REQUIRE(file.size() > 2 * OZJ_PREFIX_BYTES);
    CHECK(std::equal(file.begin(), file.begin() + OZJ_PREFIX_BYTES, file.begin() + OZJ_PREFIX_BYTES));
    CHECK(file[OZJ_PREFIX_BYTES] == 0xFF); // JPEG start of image
    CHECK(file[OZJ_PREFIX_BYTES + 1] == 0xD8);

    std::vector<float> loaded(VALUES, -1.0f);
    std::string error;
    REQUIRE(DecodeOzj(file, LIGHT_MAP_SIZE, loaded.data(), error));
    CHECK(MaxDifference(light, loaded) <= MAX_JPEG_ERROR);
}

TEST_CASE("The light map's row y = 0 is the JPEG's bottom row, as the loader reads it [editor][light]")
{
    std::vector<float> light(VALUES, 0.0f);
    for (int x = 0; x < LIGHT_MAP_SIZE; ++x)
        light[static_cast<std::size_t>(x) * CHANNELS] = 1.0f; // row y = 0 red
    const Bytes file = EncodeOzj(light.data(), LIGHT_MAP_SIZE);
    const std::vector<unsigned char> image = TopDownPixels(file);
    constexpr int BRIGHT = 200;
    constexpr int DARK = 50;
    const std::size_t bottomRow = static_cast<std::size_t>(LIGHT_MAP_SIZE - 1) * LIGHT_MAP_SIZE * CHANNELS;
    CHECK(image[bottomRow + LIGHT_MAP_SIZE / 2 * CHANNELS] > BRIGHT);
    CHECK(image[LIGHT_MAP_SIZE / 2 * CHANNELS] < DARK);
}

TEST_CASE("Flat light survives a save exactly [editor][light]")
{
    constexpr float LEVEL = 100.0f / 255.0f;
    std::vector<float> light(VALUES, LEVEL);
    std::vector<float> loaded(VALUES);
    std::string error;
    REQUIRE(DecodeOzj(EncodeOzj(light.data(), LIGHT_MAP_SIZE), LIGHT_MAP_SIZE, loaded.data(), error));
    CHECK(MaxDifference(light, loaded) == 0.0f);
}

TEST_CASE("Lorencia's shipped light map reads and saves back within JPEG precision [editor][light]")
{
    const Bytes shipped = ReadFile(std::filesystem::path(MU_REPO_ROOT) / "src/bin/Data/World1/TerrainLight.OZJ");
    REQUIRE(!shipped.empty());
    std::vector<float> light(VALUES);
    std::string error;
    REQUIRE(DecodeOzj(shipped, LIGHT_MAP_SIZE, light.data(), error));

    std::vector<float> saved(VALUES);
    REQUIRE(DecodeOzj(EncodeOzj(light.data(), LIGHT_MAP_SIZE), LIGHT_MAP_SIZE, saved.data(), error));
    CHECK(MaxDifference(light, saved) <= MAX_JPEG_ERROR);
}

TEST_CASE("A light map of the wrong size or no JPEG is refused [editor][light]")
{
    constexpr int SMALL = 128;
    const std::vector<float> small(static_cast<std::size_t>(SMALL) * SMALL * CHANNELS, 0.5f);
    std::vector<float> loaded(VALUES, 0.25f);
    std::string error;
    CHECK_FALSE(DecodeOzj(EncodeOzj(small.data(), SMALL), LIGHT_MAP_SIZE, loaded.data(), error));
    CHECK(error.find("128 x 128") != std::string::npos);
    CHECK(loaded.front() == 0.25f); // left as it was

    CHECK_FALSE(DecodeOzj(Bytes(OZJ_PREFIX_BYTES, 0), LIGHT_MAP_SIZE, loaded.data(), error));
    CHECK_FALSE(DecodeOzj(Bytes(OZJ_PREFIX_BYTES + 100, 0x42), LIGHT_MAP_SIZE, loaded.data(), error));
    CHECK(EncodeOzj(nullptr, LIGHT_MAP_SIZE).empty());
}
