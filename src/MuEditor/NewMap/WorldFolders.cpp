#include "WorldFolders.h"

#ifdef _EDITOR

#include "NewMapPlan.h"
#include "NewMapWriter.h"

#include "Assets/EditorText.h"

#include "World/MapInfra/MapNumbers.h"

#include <string>
#include <system_error>

namespace Editor::NewMap
{
namespace
{
namespace fs = std::filesystem;
namespace Numbers = World::MapNumbers;
} // namespace

std::vector<int> ExistingWorldFolders(const fs::path& gameRoot)
{
    std::vector<int> worlds;
    for (int world = Numbers::FIRST_FOLDER; world <= Numbers::LAST_FOLDER; ++world)
    {
        std::error_code ec;
        if (fs::is_directory(gameRoot / WorldFolder(world), ec))
            worlds.push_back(world);
    }
    return worlds;
}

int NextFreeMapNumber(const fs::path& gameRoot, const fs::path& repoRoot)
{
    for (int map = Numbers::FIRST_NEW_MAP; map <= Numbers::LAST_NEW_MAP; ++map)
    {
        NewMapPlan plan;
        const int world = Numbers::FolderOf(map);
        plan.folders = {WorldFolder(world), ObjectFolder(world)};
        std::string ignored;
        if (CheckFoldersFree(gameRoot, repoRoot, plan, ignored))
            return map;
    }
    return -1;
}

std::optional<fs::path> FindIgnoringCase(const fs::path& dir, std::string_view name)
{
    std::optional<fs::path> found;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec))
    {
        const std::string entryName = Editor::Text::PathToUtf8(entry.path().filename());
        if (!entry.is_regular_file(ec) || !Editor::Text::EqualIgnoringCase(entryName, name))
            continue;
        if (entryName == name)
            return entry.path();
        if (!found || entry.path() < *found)
            found = entry.path();
    }
    return found;
}
} // namespace Editor::NewMap

#endif // _EDITOR
