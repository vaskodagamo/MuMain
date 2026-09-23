#pragma once

#ifdef _EDITOR

#include "Image.h"

#include <filesystem>
#include <string>

namespace Editor::MapInspect
{
// Writes `image` (grey or RGB) as a PNG file, creating missing folders. False, with
// the reason in `error`, when the image is empty or the file cannot be written.
bool WritePng(const std::filesystem::path& file, const Image& image, std::string& error);
} // namespace Editor::MapInspect

#endif // _EDITOR
