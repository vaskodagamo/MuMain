#pragma once

#ifdef _EDITOR

#include "Render/Renderer/FramePixelReadback.h" // mu::FramePixels (top-down RGB)

#include <cstdint>
#include <vector>

// Turns a read-back frame into the small JPEG a regeneration request carries
// (captures are at most 1920 px wide: the repository has no LFS).
namespace Editor::Capture
{
constexpr std::uint32_t MAX_CAPTURE_WIDTH = 1920;
constexpr int CAPTURE_JPEG_QUALITY = 90;

// A copy of `frame` no wider than `maxWidth`, keeping its aspect ratio. Each
// output pixel is the average of the source pixels it covers.
mu::FramePixels DownscaleToWidth(const mu::FramePixels& frame, std::uint32_t maxWidth);

// Baseline JPEG of a top-down RGB frame; empty when the frame is empty or the
// encoder fails.
std::vector<std::uint8_t> EncodeJpeg(const mu::FramePixels& frame, int quality);

// Decodes a JPEG into a top-down RGB frame. False (and `frame` empty) when the
// bytes are not a JPEG the decoder reads.
bool DecodeJpeg(const std::vector<std::uint8_t>& jpeg, mu::FramePixels& frame);

// Adds an opaque alpha channel: top-down RGBA bytes, e.g. for a texture.
std::vector<std::uint8_t> ToRgba(const mu::FramePixels& frame);
} // namespace Editor::Capture

#endif // _EDITOR
