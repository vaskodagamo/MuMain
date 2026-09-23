#include "PngFile.h"

#ifdef _EDITOR

#include "Assets/EditorText.h"

// The one translation unit that builds the vendored PNG writer (ThirdParty/stb).
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <system_error>

namespace Editor::MapInspect
{
namespace
{
struct FreeDeleter
{
    void operator()(unsigned char* bytes) const
    {
        std::free(bytes);
    }
};

bool CreateParentFolder(const std::filesystem::path& file, std::string& error)
{
    const std::filesystem::path folder = file.parent_path();
    if (folder.empty())
        return true;
    std::error_code failure;
    std::filesystem::create_directories(folder, failure);
    if (failure)
    {
        error = "cannot create " + Editor::Text::PathToUtf8(folder) + ": " + failure.message();
        return false;
    }
    return true;
}
} // namespace

bool WritePng(const std::filesystem::path& file, const Image& image, std::string& error)
{
    if (image.IsEmpty())
    {
        error = "there is no image to write";
        return false;
    }
    if (!CreateParentFolder(file, error))
        return false;

    int size = 0;
    const std::unique_ptr<unsigned char, FreeDeleter> png(
        stbi_write_png_to_mem(image.pixels.data(), 0, image.width, image.height, image.channels, &size));
    if (!png)
    {
        error = "the PNG could not be encoded";
        return false;
    }

    // std::ofstream takes the path as it is (wide on Windows), unlike fopen.
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(png.get()), size);
    out.close();
    if (!out)
    {
        error = "cannot write " + Editor::Text::PathToUtf8(file);
        return false;
    }
    return true;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
