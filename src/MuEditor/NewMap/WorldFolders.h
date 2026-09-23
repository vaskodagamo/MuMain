#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

// The map folders a game's Data folder holds, for pickers and for the next free number,
// and its files as the client finds them.
namespace Editor::NewMap
{
// The numbers N of the Data/World{N} folders under `gameRoot` (1 to 255), ascending.
std::vector<int> ExistingWorldFolders(const std::filesystem::path& gameRoot);

// The lowest new map number (82 to 254) whose World and Object folders exist neither in
// the game's Data folder nor, when `repoRoot` is not empty, in the repository; -1 when
// every one is taken.
int NextFreeMapNumber(const std::filesystem::path& gameRoot, const std::filesystem::path& repoRoot);

// The file of `dir` named `name` in any letter case, as the client resolves names (the
// repository keeps Data/Gate.bmd as gate.bmd, for example); the exact spelling wins when
// several match. nullopt when there is none.
std::optional<std::filesystem::path> FindIgnoringCase(const std::filesystem::path& dir, std::string_view name);
} // namespace Editor::NewMap

#endif // _EDITOR
