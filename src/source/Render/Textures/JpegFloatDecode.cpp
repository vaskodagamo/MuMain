#include "JpegFloatDecode.h"

#include "turbojpeg.h"

#include <memory>
#include <vector>

namespace Render::Textures
{
namespace
{
constexpr std::size_t RGB_CHANNELS = 3;
constexpr float CHANNEL_MAX = 255.0f;

struct DecompressorDeleter
{
    void operator()(void* handle) const
    {
        tjDestroy(handle);
    }
};
} // namespace

bool DecodeJpegToFloats(const unsigned char* jpeg, std::size_t size, int width, int height, float* out,
                        std::string& error)
{
    if (jpeg == nullptr || size == 0 || out == nullptr)
    {
        error = "there is no image data";
        return false;
    }

    const std::unique_ptr<void, DecompressorDeleter> decompressor(tjInitDecompress());
    if (!decompressor)
    {
        error = "the JPEG decoder could not start";
        return false;
    }

    int jpegWidth = 0;
    int jpegHeight = 0;
    int subsampling = TJSAMP_444;
    int colorspace = TJCS_RGB;
    const unsigned long jpegSize = static_cast<unsigned long>(size);
    if (tjDecompressHeader3(decompressor.get(), jpeg, jpegSize, &jpegWidth, &jpegHeight, &subsampling, &colorspace) !=
        0)
    {
        error = "the data is no readable JPEG";
        return false;
    }
    if (jpegWidth != width || jpegHeight != height)
    {
        error = "the image is " + std::to_string(jpegWidth) + " x " + std::to_string(jpegHeight) + ", needs " +
                std::to_string(width) + " x " + std::to_string(height);
        return false;
    }

    const std::size_t values = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * RGB_CHANNELS;
    std::vector<unsigned char> pixels(values);
    if (tjDecompress2(decompressor.get(), jpeg, jpegSize, pixels.data(), width, 0, height, TJPF_RGB, TJFLAG_BOTTOMUP) !=
        0)
    {
        error = "the JPEG could not be decoded";
        return false;
    }

    for (std::size_t i = 0; i < values; ++i)
        out[i] = static_cast<float>(pixels[i]) / CHANNEL_MAX;
    return true;
}
} // namespace Render::Textures
