#include "TerrainLightFile.h"

#ifdef _EDITOR

#include "turbojpeg.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace Editor::LightMap
{
namespace
{
constexpr int RGB_CHANNELS = 3;
constexpr float BYTE_MAX = 255.0f;

struct HandleFree
{
    void operator()(void* handle) const
    {
        if (handle != nullptr)
            tjDestroy(handle);
    }
};
using TurboHandle = std::unique_ptr<void, HandleFree>;

std::size_t ValueCount(int size)
{
    return static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * RGB_CHANNELS;
}

std::vector<unsigned char> ToBytes(const float* rgb, int size)
{
    std::vector<unsigned char> bytes(ValueCount(size));
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<unsigned char>(std::lround(std::clamp(rgb[i], 0.0f, 1.0f) * BYTE_MAX));
    return bytes;
}

// Bottom row first, as the loader decodes it (TJFLAG_BOTTOMUP both ways).
std::vector<std::uint8_t> CompressJpeg(const std::vector<unsigned char>& pixels, int size)
{
    const TurboHandle handle(tjInitCompress());
    if (!handle)
        return {};
    std::vector<unsigned char> jpeg(tjBufSize(size, size, TJSAMP_444));
    unsigned char* jpegData = jpeg.data();
    unsigned long jpegSize = static_cast<unsigned long>(jpeg.size());
    const int result = tjCompress2(handle.get(), pixels.data(), size, 0, size, TJPF_RGB, &jpegData, &jpegSize,
                                   TJSAMP_444, LIGHT_MAP_JPEG_QUALITY, TJFLAG_BOTTOMUP | TJFLAG_NOREALLOC);
    if (result != 0)
        return {};
    jpeg.resize(jpegSize);
    return jpeg;
}
} // namespace

std::vector<std::uint8_t> EncodeOzj(const float* rgb, int size)
{
    if (rgb == nullptr || size <= 0)
        return {};
    const std::vector<std::uint8_t> jpeg = CompressJpeg(ToBytes(rgb, size), size);
    if (jpeg.size() < OZJ_PREFIX_BYTES)
        return {};
    std::vector<std::uint8_t> file(jpeg.begin(), jpeg.begin() + OZJ_PREFIX_BYTES);
    file.insert(file.end(), jpeg.begin(), jpeg.end());
    return file;
}

bool DecodeOzj(const std::vector<std::uint8_t>& file, int size, float* rgb, std::string& error)
{
    if (file.size() <= OZJ_PREFIX_BYTES)
    {
        error = "the file is shorter than the 24-byte .OZJ prefix";
        return false;
    }
    const TurboHandle handle(tjInitDecompress());
    if (!handle)
    {
        error = "the JPEG decoder could not start";
        return false;
    }

    unsigned char* jpeg = const_cast<unsigned char*>(file.data()) + OZJ_PREFIX_BYTES;
    const auto jpegSize = static_cast<unsigned long>(file.size() - OZJ_PREFIX_BYTES);
    int width = 0;
    int height = 0;
    int subsampling = 0;
    int colorspace = 0;
    if (tjDecompressHeader3(handle.get(), jpeg, jpegSize, &width, &height, &subsampling, &colorspace) != 0)
    {
        error = "not a JPEG after the .OZJ prefix";
        return false;
    }
    if (width != size || height != size)
    {
        error = "the light map is " + std::to_string(width) + " x " + std::to_string(height) + " pixels, not " +
                std::to_string(size) + " x " + std::to_string(size);
        return false;
    }

    std::vector<unsigned char> pixels(ValueCount(size));
    if (tjDecompress2(handle.get(), jpeg, jpegSize, pixels.data(), width, 0, height, TJPF_RGB, TJFLAG_BOTTOMUP) != 0)
    {
        error = "the JPEG could not be decoded";
        return false;
    }
    for (std::size_t i = 0; i < pixels.size(); ++i)
        rgb[i] = static_cast<float>(pixels[i]) / BYTE_MAX;
    return true;
}
} // namespace Editor::LightMap

#endif // _EDITOR
