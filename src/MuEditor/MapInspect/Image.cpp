#include "Image.h"

#ifdef _EDITOR

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace Editor::MapInspect
{
namespace
{
constexpr int RGB_CHANNELS = 3;

std::size_t PixelOffset(const Image& image, int column, int row)
{
    return (static_cast<std::size_t>(row) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(column)) *
           static_cast<std::size_t>(image.channels);
}

bool IsInside(const Image& image, int column, int row)
{
    return column >= 0 && row >= 0 && column < image.width && row < image.height;
}
} // namespace

Image MakeImage(int width, int height, int channels)
{
    Image image;
    if (width <= 0 || height <= 0 || channels <= 0)
        return image;
    image.width = width;
    image.height = height;
    image.channels = channels;
    image.pixels.assign(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(channels), 0);
    return image;
}

void SetGrey(Image& image, int column, int row, std::uint8_t value)
{
    if (!IsInside(image, column, row))
        return;
    std::fill_n(image.pixels.begin() + static_cast<std::ptrdiff_t>(PixelOffset(image, column, row)), image.channels,
                value);
}

void SetRgb(Image& image, int column, int row, const Rgb& color)
{
    if (image.channels < RGB_CHANNELS || !IsInside(image, column, row))
        return;
    const std::size_t offset = PixelOffset(image, column, row);
    image.pixels[offset] = color.r;
    image.pixels[offset + 1] = color.g;
    image.pixels[offset + 2] = color.b;
}

Image Crop(const Image& image, int x, int y, int width, int height)
{
    const int left = std::max(x, 0);
    const int top = std::max(y, 0);
    const int right = std::min(x + width, image.width);
    const int bottom = std::min(y + height, image.height);
    if (image.IsEmpty() || right <= left || bottom <= top)
        return Image{};

    Image cropped = MakeImage(right - left, bottom - top, image.channels);
    const std::size_t rowBytes = static_cast<std::size_t>(cropped.width) * static_cast<std::size_t>(image.channels);
    for (int row = 0; row < cropped.height; ++row)
    {
        std::memcpy(cropped.pixels.data() + PixelOffset(cropped, 0, row),
                    image.pixels.data() + PixelOffset(image, left, top + row), rowBytes);
    }
    return cropped;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
