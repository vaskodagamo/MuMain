#pragma once

#include <cstddef>
#include <string>

namespace Render::Textures
{
// Decodes a JPEG of exactly `width` x `height` pixels into `out`: width * height * 3
// floats (red, green, blue, 0..1), bottom row first, the layout of the terrain light
// map. False, with the reason in `error` and nothing written, when the data is no
// JPEG or has another size; a larger image would not fit into `out`.
bool DecodeJpegToFloats(const unsigned char* jpeg, std::size_t size, int width, int height, float* out,
                        std::string& error);
} // namespace Render::Textures
