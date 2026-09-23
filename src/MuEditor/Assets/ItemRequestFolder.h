#pragma once

#ifdef _EDITOR

#include "ItemRequest.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Item request folders on disk: <repo>/assets-work/Items/requests/<id>/ with
// request.json, brief.md, captures/NN-*.jpg and captures/ref-*.jpg. Files only;
// no engine or UI.
namespace Editor::Assets
{
// The JPEG bytes of a request, in the order of the draft's captures and
// referenceImages.
struct ItemRequestImages
{
    std::vector<std::vector<std::uint8_t>> captures;
    std::vector<std::vector<std::uint8_t>> references;
};

// Writes the folder RequestsDir(draft.domain)/<draft.id>. Refuses a folder that
// exists already; on failure nothing is left behind and `error` says why.
bool WriteItemRequestFolder(const std::filesystem::path& repoRoot, const ItemRequestDraft& draft,
                            const ItemRequestImages& images, std::filesystem::path& folder, std::string& error);

// A reference image as a request stores it: `file` must be a JPEG; one wider than
// `maxWidth` is scaled down and encoded again, a smaller one is kept byte for byte.
bool ReadReferenceJpeg(const std::filesystem::path& file, std::uint32_t maxWidth, std::vector<std::uint8_t>& jpeg,
                       std::string& error);
} // namespace Editor::Assets

#endif // _EDITOR
