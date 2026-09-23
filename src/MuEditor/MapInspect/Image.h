#pragma once

#ifdef _EDITOR

#include <cstdint>
#include <vector>

namespace Editor::MapInspect
{
struct Rgb
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

// An 8-bit image, rows top to bottom, `channels` bytes per pixel (1 = grey, 3 = RGB).
struct Image
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;

    bool IsEmpty() const
    {
        return width <= 0 || height <= 0 || pixels.empty();
    }
};

// A black image of the given size.
Image MakeImage(int width, int height, int channels);

void SetGrey(Image& image, int column, int row, std::uint8_t value);
void SetRgb(Image& image, int column, int row, const Rgb& color);

// The part of `image` from (x, y), `width` x `height` pixels, clipped to the image.
// Empty when nothing of it lies inside.
Image Crop(const Image& image, int x, int y, int width, int height);
} // namespace Editor::MapInspect

#endif // _EDITOR
