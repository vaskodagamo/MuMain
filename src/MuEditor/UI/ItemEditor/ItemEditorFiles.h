#pragma once

#ifdef _EDITOR

#include "Core/EditorFiles.h"

#include <filesystem>
#include <vector>

// The item data the item tools save, mirrored into the checkout. The game and the
// item tools read and write the JSON item data next to the executable
// (Data/Items, and the glow colors and render styles in Data/Effects); the build
// copies src/bin/Data over that folder again, so a save is only durable once the
// changed files are in <repo>/src/bin/Data too. The save, mirror and read helpers
// every editor shares are in Core/EditorFiles.h.
namespace Editor::Files
{
std::filesystem::path ItemDataDir(); // Data/Items

// Call right after the item tools saved (CItemDataHandler::Save) with the time
// taken just before the save: copies every item data file the save wrote (written
// since then; a save rewrites only the files whose content changed) and whose
// bytes differ from the checkout's copy into <repo>/src/bin/Data, backing up what
// it replaces (MirrorSavedFile). Files the save left alone are not copied, so a
// build folder older than the checkout never overwrites it. Without a checkout
// nothing is copied. Returns the files it mirrored.
std::vector<SavedFile> MirrorSavedItemData(std::filesystem::file_time_type savedSince);
} // namespace Editor::Files

#endif // _EDITOR
