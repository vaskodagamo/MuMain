#pragma once

#ifdef _EDITOR

#include "NewMapPlan.h"

#include "Core/RepoMirror.h" // MirrorOutcome

#include <filesystem>
#include <string>
#include <vector>

// Writes a planned new map into the game's Data folder and copies each file into the
// repository's src/bin/Data, as every Map Editor save does. File system only.
namespace Editor::NewMap
{
struct WrittenFile
{
    std::filesystem::path relative; // Data/World83/...
    std::filesystem::path runtime;  // the file the running game reads
    Editor::Files::MirrorOutcome repo;
};

// False with the reason when a folder of `plan` already exists in the game's Data folder
// or, when `repoRoot` is not empty, in the repository's src/bin/Data: a new map never
// replaces files.
bool CheckFoldersFree(const std::filesystem::path& gameRoot, const std::filesystem::path& repoRoot,
                      const NewMapPlan& plan, std::string& error);

// Writes every file of `plan` under `gameRoot` (after CheckFoldersFree), then copies each
// into <repoRoot>/src/bin/Data (skipped when `repoRoot` is empty; `stamp` names the backup
// folder, which a new file never needs). When a file cannot be written, the folders the
// plan created are removed again and false is returned with the reason; a failed
// repository copy is reported in its WrittenFile.
bool WriteNewMap(const std::filesystem::path& gameRoot, const std::filesystem::path& repoRoot, const std::string& stamp,
                 const NewMapPlan& plan, std::vector<WrittenFile>& written, std::string& error);
} // namespace Editor::NewMap

#endif // _EDITOR
