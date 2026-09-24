#include "NewMapWriter.h"

#ifdef _EDITOR

#include "Assets/EditorText.h"

#include <fstream>
#include <system_error>

namespace Editor::NewMap
{
namespace
{
namespace fs = std::filesystem;

bool Exists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

bool WriteBytes(const fs::path& file, const Bytes& bytes, std::string& error)
{
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!ec && stream)
    {
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        if (stream)
            return true;
    }
    error = "cannot write " + Editor::Text::PathToUtf8(file);
    return false;
}

// Removes what an interrupted write left: only the plan's own folders, which did not
// exist before it started.
void RemoveFolders(const fs::path& gameRoot, const NewMapPlan& plan)
{
    for (const fs::path& folder : plan.folders)
    {
        std::error_code ec;
        fs::remove_all(gameRoot / folder, ec);
    }
}

fs::path Absolute(const fs::path& path)
{
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec);
    return ec ? path : absolute.lexically_normal();
}
} // namespace

bool CheckFoldersFree(const fs::path& gameRoot, const fs::path& repoRoot, const NewMapPlan& plan, std::string& error)
{
    for (const fs::path& folder : plan.folders)
    {
        if (Exists(gameRoot / folder))
        {
            error = Editor::Text::PathToUtf8(folder) + " already exists in the game's Data folder";
            return false;
        }
        if (!repoRoot.empty() && Exists(Editor::Files::RepoDataFile(repoRoot, folder)))
        {
            error = Editor::Text::PathToUtf8(folder) + " already exists in the repository (src/bin/" +
                    Editor::Text::PathToUtf8(folder) + ")";
            return false;
        }
    }
    return true;
}

bool WriteNewMap(const fs::path& gameRoot, const fs::path& repoRoot, const std::string& stamp, const NewMapPlan& plan,
                 std::vector<WrittenFile>& written, std::string& error)
{
    if (!CheckFoldersFree(gameRoot, repoRoot, plan, error))
        return false;

    written.clear();
    for (const fs::path& folder : plan.folders)
    {
        std::error_code ec;
        fs::create_directories(gameRoot / folder, ec);
    }
    for (const PlannedFile& file : plan.files)
    {
        const fs::path runtime = Absolute(gameRoot / file.relative);
        if (!WriteBytes(runtime, file.bytes, error))
        {
            RemoveFolders(gameRoot, plan);
            written.clear();
            return false;
        }
        written.push_back({file.relative, runtime, {}});
    }
    for (WrittenFile& file : written)
    {
        if (repoRoot.empty())
            file.repo.error = "no repository to copy into";
        else
            file.repo = Editor::Files::MirrorIntoRepo(file.runtime, file.relative, repoRoot, stamp);
    }
    return true;
}
} // namespace Editor::NewMap

#endif // _EDITOR
