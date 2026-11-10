#include "stdafx.h"

#ifdef _EDITOR

#include "ItemEditorFiles.h"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace fs = std::filesystem;

namespace Editor::Files
{
namespace
{
constexpr const wchar_t* ITEMS_FOLDER = L"Items";
constexpr const wchar_t* EFFECTS_FOLDER = L"Effects";
// What the item tools save outside Data/Items (docs/item-data.md).
constexpr const wchar_t* ITEM_EFFECT_FILES[] = {L"GlowColors.json", L"RenderStyles.json"};
// File times are coarser than the clock on some file systems.
constexpr std::chrono::seconds FILE_TIME_SLACK{2};

// The data files (relative to the working directory, starting with Data) a save of the item tools may have written.
std::vector<fs::path> SavedItemDataFiles()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(ItemDataDir(), ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file(ec))
            files.push_back(it->path());
    }
    for (const wchar_t* name : ITEM_EFFECT_FILES)
    {
        const fs::path file = DataDir() / EFFECTS_FOLDER / name;
        if (fs::is_regular_file(file, ec))
            files.push_back(file);
    }
    std::sort(files.begin(), files.end());
    return files;
}
} // namespace

fs::path ItemDataDir()
{
    return DataDir() / ITEMS_FOLDER;
}

std::vector<SavedFile> MirrorSavedItemData(fs::file_time_type savedSince)
{
    std::vector<SavedFile> mirrored;
    const RepoRootLookup& repoRoot = RepoRoot();
    if (repoRoot.root.empty())
        return mirrored;

    for (const fs::path& file : SavedItemDataFiles())
    {
        // Only what this save wrote and the checkout does not already hold.
        std::error_code ec;
        const fs::file_time_type written = fs::last_write_time(file, ec);
        if (ec || written + FILE_TIME_SLACK < savedSince)
            continue;
        if (SameFileContents(AbsolutePath(file), RepoDataFile(repoRoot.root, file)))
            continue;
        mirrored.push_back(MirrorSavedFile(file));
    }
    return mirrored;
}
} // namespace Editor::Files

#endif // _EDITOR
