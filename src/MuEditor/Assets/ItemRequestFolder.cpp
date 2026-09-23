#include "ItemRequestFolder.h"

#ifdef _EDITOR

#include "CaptureImage.h"
#include "EditorText.h"
#include "ItemRequestBrief.h"
#include "RequestFolder.h"

#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace Editor::Assets
{
namespace
{
constexpr const char* REQUEST_FILE = "request.json";
constexpr const char* BRIEF_FILE = "brief.md";
constexpr const char* CAPTURES_FOLDER = "captures/";

std::string Bytes(const std::vector<std::uint8_t>& data)
{
    return std::string(data.begin(), data.end());
}
} // namespace

bool WriteItemRequestFolder(const fs::path& repoRoot, const ItemRequestDraft& draft, const ItemRequestImages& images,
                            fs::path& folder, std::string& error)
{
    folder = RequestsDir(repoRoot, draft.domain) / Editor::Text::Utf8Path(draft.id);
    if (images.captures.size() != draft.captures.size() || images.references.size() != draft.referenceImages.size())
    {
        error = "the captures and their images do not match";
        return false;
    }
    std::vector<RequestFile> files;
    for (std::size_t i = 0; i < draft.captures.size(); ++i)
        files.push_back({CAPTURES_FOLDER + draft.captures[i].fileName, Bytes(images.captures[i])});
    for (std::size_t i = 0; i < draft.referenceImages.size(); ++i)
        files.push_back({CAPTURES_FOLDER + draft.referenceImages[i], Bytes(images.references[i])});
    files.push_back({BRIEF_FILE, BuildItemBrief(draft)});
    files.push_back({REQUEST_FILE, BuildItemRequestJson(draft)});
    return WriteNewRequestFolder(folder, files, error);
}

bool ReadReferenceJpeg(const fs::path& file, std::uint32_t maxWidth, std::vector<std::uint8_t>& jpeg,
                       std::string& error)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        error = "cannot read " + Editor::Text::PathToUtf8(file);
        return false;
    }
    jpeg.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    mu::FramePixels pixels;
    if (!Editor::Capture::DecodeJpeg(jpeg, pixels))
    {
        error = Editor::Text::PathToUtf8(file.filename()) + " is not a JPEG (export it as .jpg first)";
        return false;
    }
    if (pixels.width <= maxWidth)
        return true;
    jpeg = Editor::Capture::EncodeJpeg(Editor::Capture::DownscaleToWidth(pixels, maxWidth),
                                       Editor::Capture::CAPTURE_JPEG_QUALITY);
    if (jpeg.empty())
        error = "cannot encode a smaller copy of " + Editor::Text::PathToUtf8(file.filename());
    return !jpeg.empty();
}
} // namespace Editor::Assets

#endif // _EDITOR
