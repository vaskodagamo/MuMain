#include "CaptureImage.h"

#ifdef _EDITOR

#include "turbojpeg.h"

#include <algorithm>
#include <memory>

namespace Editor::Capture
{
namespace
{
constexpr std::size_t RGB_BYTES = 3;

struct CompressorFree
{
    void operator()(void* handle) const
    {
        if (handle != nullptr)
            tjDestroy(handle);
    }
};
using Compressor = std::unique_ptr<void, CompressorFree>;

bool IsValid(const mu::FramePixels& frame)
{
    const std::size_t expected = static_cast<std::size_t>(frame.width) * frame.height * RGB_BYTES;
    return frame.width > 0 && frame.height > 0 && frame.rgb.size() == expected;
}

// Source rows or columns [first, last) that output index `index` of `outCount` covers.
void SourceSpan(std::uint32_t index, std::uint32_t outCount, std::uint32_t inCount, std::uint32_t& first,
                std::uint32_t& last)
{
    first = static_cast<std::uint32_t>(static_cast<std::uint64_t>(index) * inCount / outCount);
    last = static_cast<std::uint32_t>(static_cast<std::uint64_t>(index + 1) * inCount / outCount);
    last = std::max(last, first + 1);
}

void AveragePixel(const mu::FramePixels& frame, std::uint32_t x0, std::uint32_t x1, std::uint32_t y0, std::uint32_t y1,
                  std::uint8_t* out)
{
    std::uint64_t sum[RGB_BYTES] = {};
    for (std::uint32_t y = y0; y < y1; ++y)
    {
        const std::uint8_t* row = frame.rgb.data() + (static_cast<std::size_t>(y) * frame.width + x0) * RGB_BYTES;
        for (std::uint32_t x = x0; x < x1; ++x, row += RGB_BYTES)
        {
            for (std::size_t c = 0; c < RGB_BYTES; ++c)
                sum[c] += row[c];
        }
    }
    const std::uint64_t count = static_cast<std::uint64_t>(x1 - x0) * (y1 - y0);
    for (std::size_t c = 0; c < RGB_BYTES; ++c)
        out[c] = static_cast<std::uint8_t>((sum[c] + count / 2) / count);
}
} // namespace

mu::FramePixels DownscaleToWidth(const mu::FramePixels& frame, std::uint32_t maxWidth)
{
    if (!IsValid(frame) || maxWidth == 0 || frame.width <= maxWidth)
        return frame;

    mu::FramePixels out;
    out.width = maxWidth;
    const std::uint64_t scaledHeight =
        (static_cast<std::uint64_t>(frame.height) * maxWidth + frame.width / 2) / frame.width;
    out.height = static_cast<std::uint32_t>(std::max<std::uint64_t>(1, scaledHeight));
    out.rgb.resize(static_cast<std::size_t>(out.width) * out.height * RGB_BYTES);
    for (std::uint32_t y = 0; y < out.height; ++y)
    {
        std::uint32_t y0 = 0, y1 = 0;
        SourceSpan(y, out.height, frame.height, y0, y1);
        for (std::uint32_t x = 0; x < out.width; ++x)
        {
            std::uint32_t x0 = 0, x1 = 0;
            SourceSpan(x, out.width, frame.width, x0, x1);
            AveragePixel(frame, x0, x1, y0, y1,
                         out.rgb.data() + (static_cast<std::size_t>(y) * out.width + x) * RGB_BYTES);
        }
    }
    return out;
}

std::vector<std::uint8_t> EncodeJpeg(const mu::FramePixels& frame, int quality)
{
    if (!IsValid(frame))
        return {};
    Compressor compressor(tjInitCompress());
    if (!compressor)
        return {};

    const int width = static_cast<int>(frame.width);
    const int height = static_cast<int>(frame.height);
    unsigned long jpegSize = tjBufSize(width, height, TJSAMP_420);
    std::vector<std::uint8_t> jpeg(jpegSize);
    unsigned char* output = jpeg.data();
    const int pitch = width * static_cast<int>(RGB_BYTES);
    if (tjCompress2(compressor.get(), frame.rgb.data(), width, pitch, height, TJPF_RGB, &output, &jpegSize, TJSAMP_420,
                    quality, TJFLAG_NOREALLOC) != 0)
        return {};
    jpeg.resize(jpegSize);
    return jpeg;
}
} // namespace Editor::Capture

#endif // _EDITOR
