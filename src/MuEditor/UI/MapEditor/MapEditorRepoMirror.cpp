#include "MapEditorRepoMirror.h"

#ifdef _EDITOR

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace Editor::Files
{
namespace
{
// <repo>/src/bin/Data is what the build copies next to the executable.
constexpr const char* SOURCE_FOLDER = "src";
constexpr const char* BIN_FOLDER = "bin";
constexpr const char* DATA_FOLDER = "Data";
constexpr const char* GIT_ENTRY = ".git";
// <repo>/out is ignored by git.
constexpr const char* OUT_FOLDER = "out";
constexpr const char* BACKUP_FOLDER = "editor-backups";
constexpr const char* EXPORT_FOLDER = "editor-exports";
constexpr const char* PARENT_ELEMENT = "..";

constexpr std::size_t COMPARE_CHUNK_BYTES = 64 * 1024;

fs::path RepoDataDir(const fs::path& repoRoot)
{
    return repoRoot / SOURCE_FOLDER / BIN_FOLDER / DATA_FOLDER;
}

bool IsDirectory(const fs::path& path)
{
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool Exists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

fs::path Absolute(const fs::path& path)
{
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec);
    return ec ? path : absolute.lexically_normal();
}

std::optional<fs::path> FindRepoRootAbove(const fs::path& start)
{
    fs::path dir = Absolute(start);
    while (!dir.empty())
    {
        if (IsRepoRoot(dir))
            return dir;
        const fs::path parent = dir.parent_path();
        if (parent == dir)
            break; // the file system root
        dir = parent;
    }
    return std::nullopt;
}

bool CopyOver(const fs::path& from, const fs::path& to, std::string& error)
{
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    if (ec)
    {
        error = "cannot create " + PathToUtf8(to.parent_path()) + ": " + ec.message();
        return false;
    }
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        error = "cannot copy to " + PathToUtf8(to) + ": " + ec.message();
        return false;
    }
    return true;
}

// Keeps an existing backup of the same second: it already holds older bytes.
bool BackUp(const fs::path& repoFile, const fs::path& backupFile, std::string& error)
{
    if (Exists(backupFile))
        return true;
    return CopyOver(repoFile, backupFile, error);
}

bool ReadChunk(std::ifstream& stream, std::vector<char>& buffer, std::streamsize& got)
{
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    got = stream.gcount();
    return got > 0;
}
} // namespace

bool IsRepoRoot(const fs::path& dir)
{
    return IsDirectory(RepoDataDir(dir)) && Exists(dir / GIT_ENTRY);
}

RepoRootLookup LocateRepoRoot(const fs::path& overrideRoot, const std::vector<fs::path>& startDirs)
{
    if (!overrideRoot.empty())
    {
        const fs::path root = Absolute(overrideRoot);
        if (IsDirectory(RepoDataDir(root)))
            return {root, "MU_EDITOR_REPO_ROOT"};
        return {{}, "MU_EDITOR_REPO_ROOT=" + PathToUtf8(overrideRoot) + " has no src/bin/Data"};
    }

    for (const fs::path& start : startDirs)
    {
        if (const std::optional<fs::path> root = FindRepoRootAbove(start))
            return {*root, "found above " + PathToUtf8(Absolute(start))};
    }
    return {{}, "no git checkout with src/bin/Data above the game folder"};
}

bool IsDataRelative(const fs::path& dataRelative)
{
    if (dataRelative.empty() || dataRelative.is_absolute() || dataRelative.has_root_name())
        return false;
    if (*dataRelative.begin() != DATA_FOLDER || dataRelative.filename().empty())
        return false;
    for (const fs::path& element : dataRelative)
    {
        if (element == PARENT_ELEMENT)
            return false;
    }
    return std::distance(dataRelative.begin(), dataRelative.end()) > 1;
}

fs::path RepoDataFile(const fs::path& repoRoot, const fs::path& dataRelative)
{
    return repoRoot / SOURCE_FOLDER / BIN_FOLDER / dataRelative;
}

fs::path RepoBackupFile(const fs::path& repoRoot, const std::string& stamp, const fs::path& dataRelative)
{
    return repoRoot / OUT_FOLDER / BACKUP_FOLDER / stamp / dataRelative;
}

fs::path RepoExportDir(const fs::path& repoRoot)
{
    return repoRoot / OUT_FOLDER / EXPORT_FOLDER;
}

MirrorOutcome MirrorIntoRepo(const fs::path& runtimeFile, const fs::path& dataRelative, const fs::path& repoRoot,
                             const std::string& stamp)
{
    MirrorOutcome outcome;
    if (repoRoot.empty())
    {
        outcome.error = "no repository to copy into";
        return outcome;
    }
    if (!IsDataRelative(dataRelative))
    {
        outcome.error = "not a file inside the game's Data folder: " + PathToUtf8(dataRelative);
        return outcome;
    }
    if (!Exists(runtimeFile))
    {
        outcome.error = "the saved file is missing: " + PathToUtf8(runtimeFile);
        return outcome;
    }

    outcome.repoFile = RepoDataFile(repoRoot, dataRelative);
    std::error_code ec;
    if (fs::equivalent(runtimeFile, outcome.repoFile, ec))
    {
        outcome.result = RepoCopyResult::InPlace;
        return outcome;
    }

    const bool repoHadFile = Exists(outcome.repoFile);
    if (repoHadFile && SameFileContents(runtimeFile, outcome.repoFile))
    {
        outcome.result = RepoCopyResult::Unchanged;
        return outcome;
    }
    if (repoHadFile)
    {
        const fs::path backupFile = RepoBackupFile(repoRoot, stamp, dataRelative);
        if (!BackUp(outcome.repoFile, backupFile, outcome.error))
            return outcome; // never overwrite a repo file whose bytes were not saved
        outcome.backupFile = backupFile;
    }

    if (!CopyOver(runtimeFile, outcome.repoFile, outcome.error))
        return outcome;
    outcome.result = repoHadFile ? RepoCopyResult::Replaced : RepoCopyResult::Created;
    return outcome;
}

bool SameFileContents(const fs::path& a, const fs::path& b)
{
    std::error_code ec;
    const std::uintmax_t sizeA = fs::file_size(a, ec);
    if (ec)
        return false;
    const std::uintmax_t sizeB = fs::file_size(b, ec);
    if (ec || sizeA != sizeB)
        return false;

    std::ifstream streamA(a, std::ios::binary);
    std::ifstream streamB(b, std::ios::binary);
    if (!streamA || !streamB)
        return false;

    std::vector<char> bufferA(COMPARE_CHUNK_BYTES);
    std::vector<char> bufferB(COMPARE_CHUNK_BYTES);
    std::streamsize gotA = 0;
    std::streamsize gotB = 0;
    while (ReadChunk(streamA, bufferA, gotA))
    {
        if (!ReadChunk(streamB, bufferB, gotB) || gotA != gotB)
            return false;
        if (!std::equal(bufferA.begin(), bufferA.begin() + gotA, bufferB.begin()))
            return false;
    }
    return !ReadChunk(streamB, bufferB, gotB);
}
} // namespace Editor::Files

#endif // _EDITOR
