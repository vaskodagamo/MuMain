#include "stdafx.h"

#ifdef _EDITOR

#include "EditorFiles.h"

#include "Assets/EditorText.h"
#include "Core/Utilities/StringUtils.h"
#include "UI/Console/MuEditorConsoleUI.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_stdinc.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace Editor::Files
{
namespace
{
// The client data tree's root folder.
constexpr const wchar_t* DATA_FOLDER = L"Data";

// Overrides the repository the saves are mirrored into.
constexpr const char* REPO_ROOT_VARIABLE = "MU_EDITOR_REPO_ROOT";
// Backup folder names: out/editor-backups/20260922-235959.
constexpr const char* BACKUP_STAMP_FORMAT = "%Y%m%d-%H%M%S";
constexpr size_t BACKUP_STAMP_CHARS = 32;

// Our messages are UTF-8; std::filesystem converts UTF-8 to wide text on every platform.
std::wstring Utf8ToWide(const std::string& text)
{
    return fs::path(std::u8string(text.begin(), text.end())).wstring();
}

void Log(const std::wstring& message)
{
    g_ErrorReport.Write(L"%ls\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor(StringUtils::WideToNarrow(message.c_str()));
}

// The working directory (the executable's folder on macOS) and the folder SDL
// reports for the executable (inside Main.app on macOS).
std::vector<fs::path> RepoSearchStarts()
{
    std::vector<fs::path> starts;
    std::error_code ec;
    const fs::path workingDir = fs::current_path(ec);
    if (!ec)
        starts.push_back(workingDir);
    if (const char* basePath = SDL_GetBasePath())
        starts.push_back(fs::path(std::u8string(reinterpret_cast<const char8_t*>(basePath))));
    return starts;
}

RepoRootLookup LookUpRepoRoot()
{
    // SDL_getenv gives UTF-8 on every platform; getenv would give the ANSI code page on Windows.
    const char* overrideRoot = SDL_getenv(REPO_ROOT_VARIABLE);
    const fs::path overridePath = (overrideRoot != nullptr) ? Editor::Text::Utf8Path(overrideRoot) : fs::path();
    RepoRootLookup lookup = LocateRepoRoot(overridePath, RepoSearchStarts());
    if (lookup.root.empty())
        Log(L"[Editor] Saves stay in the game's Data folder: " + Utf8ToWide(lookup.description));
    else
        Log(L"[Editor] Saves are also copied into the repository " + lookup.root.wstring());
    return lookup;
}

// Without a repository: the old behaviour, a copy next to the executable that the
// next build's asset copy does not touch (Data/World7/X -> World7/X).
fs::path CopyNextToExecutable(const fs::path& dataRelative)
{
    const fs::path copy = dataRelative.lexically_relative(DataDir());
    std::error_code ec;
    fs::create_directories(copy.parent_path(), ec);
    fs::copy_file(dataRelative, copy, fs::copy_options::overwrite_existing, ec);
    if (ec)
        return {};
    return AbsolutePath(copy);
}

void LogSavedFile(const SavedFile& saved)
{
    Log(L"[Editor] Saved " + saved.runtimeFile.wstring());
    const MirrorOutcome& repo = saved.repo;
    if (!repo.backupFile.empty())
        Log(L"[Editor]   old repo file kept in " + repo.backupFile.wstring());
    if (repo.result == RepoCopyResult::Created || repo.result == RepoCopyResult::Replaced)
        Log(L"[Editor]   copied into the repository: " + repo.repoFile.wstring());
    else if (repo.result == RepoCopyResult::Unchanged)
        Log(L"[Editor]   repository file already identical: " + repo.repoFile.wstring());
    else if (repo.result == RepoCopyResult::InPlace)
        Log(L"[Editor]   the game reads the repository's Data folder, saved in place");
    else if (!saved.localCopy.empty())
        Log(L"[Editor]   no repository, copy kept next to the game: " + saved.localCopy.wstring());
    else
        Log(L"[Editor]   NOT copied into the repository: " + Utf8ToWide(repo.error));
}

std::string DescribeRepoCopy(const SavedFile& saved)
{
    const MirrorOutcome& repo = saved.repo;
    switch (repo.result)
    {
    case RepoCopyResult::Replaced:
        return "\n  repo:   " + PathToUtf8(repo.repoFile) + "\n  backup: " + PathToUtf8(repo.backupFile);
    case RepoCopyResult::Created:
        return "\n  repo:   " + PathToUtf8(repo.repoFile) +
               " (new file: git ignores new files in src/bin, use git add -f)";
    case RepoCopyResult::Unchanged:
        return "\n  repo:   " + PathToUtf8(repo.repoFile) + " (already identical)";
    case RepoCopyResult::InPlace:
        return "\n  repo:   the game reads the repository's Data folder directly";
    case RepoCopyResult::Failed:
        break;
    }
    if (!saved.localCopy.empty())
        return "\n  no repo copy (" + RepoRoot().description + ")\n  copy:   " + PathToUtf8(saved.localCopy);
    return "\n  repo copy FAILED: " + repo.error;
}

// file:///Users/me/My%20Repo: every byte outside the unreserved URL characters
// (and the '/' and ':' of the path) as %XX of its UTF-8 encoding.
std::string FileUrl(const fs::path& absolute)
{
    constexpr const char* HEX_DIGITS = "0123456789ABCDEF";
    constexpr int HIGH_NIBBLE_SHIFT = 4;
    constexpr unsigned LOW_NIBBLE_MASK = 0x0F;
    const std::u8string generic = absolute.generic_u8string();
    const std::string path(generic.begin(), generic.end());
    std::string url = path.rfind('/', 0) == 0 ? "file://" : "file:///";
    for (const char c : path)
    {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (std::isalnum(byte) || std::strchr("-._~/:", c) != nullptr)
        {
            url.push_back(c);
            continue;
        }
        url.push_back('%');
        url.push_back(HEX_DIGITS[byte >> HIGH_NIBBLE_SHIFT]);
        url.push_back(HEX_DIGITS[byte & LOW_NIBBLE_MASK]);
    }
    return url;
}
} // namespace

fs::path DataDir()
{
    return DATA_FOLDER;
}

// MSVC's STL has a non-standard std::ifstream/ofstream(std::wstring, ...)
// extension; libstdc++ (GCC/MinGW) has no such overload, so this must use
// the wide-path FILE* API (_wfopen) that the rest of the editors' file
// I/O already uses, rather than iostreams, to build with both compilers.
std::vector<unsigned char> ReadWholeFile(const fs::path& path)
{
    FILE* fp = _wfopen(path.wstring().c_str(), L"rb");
    if (fp == nullptr)
        return {};

    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(fp);
        return {};
    }

    std::vector<unsigned char> data(static_cast<size_t>(size));
    const size_t read = fread(data.data(), 1, data.size(), fp);
    fclose(fp);
    if (read != data.size())
        return {};
    return data;
}

const RepoRootLookup& RepoRoot()
{
    static const RepoRootLookup lookup = LookUpRepoRoot();
    return lookup;
}

fs::path AbsolutePath(const fs::path& file)
{
    std::error_code ec;
    const fs::path absolute = fs::absolute(file, ec);
    return ec ? file : absolute.lexically_normal();
}

SavedFile MirrorSavedFile(const fs::path& dataRelative)
{
    SavedFile saved;
    saved.runtimeFile = AbsolutePath(dataRelative);
    const RepoRootLookup& repoRoot = RepoRoot();
    if (repoRoot.root.empty())
        saved.localCopy = CopyNextToExecutable(dataRelative);
    else
        saved.repo = MirrorIntoRepo(saved.runtimeFile, dataRelative, repoRoot.root, BackupStamp());
    LogSavedFile(saved);
    return saved;
}

fs::path CopyToRepoExports(const fs::path& file)
{
    const RepoRootLookup& repoRoot = RepoRoot();
    if (repoRoot.root.empty())
        return {};

    const fs::path exportDir = RepoExportDir(repoRoot.root);
    const fs::path copy = exportDir / file.filename();
    std::error_code ec;
    fs::create_directories(exportDir, ec);
    fs::copy_file(file, copy, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        Log(L"[Editor] Could not copy " + file.filename().wstring() + L" into " + exportDir.wstring());
        return {};
    }
    Log(L"[Editor] Export copied to " + copy.wstring());
    return copy;
}

std::string DescribeSavedFiles(const std::vector<SavedFile>& files)
{
    std::string text;
    for (const SavedFile& saved : files)
    {
        if (!text.empty())
            text += "\n";
        text += "Saved " + PathToUtf8(saved.runtimeFile.filename()) + "\n  game:   " + PathToUtf8(saved.runtimeFile) +
                DescribeRepoCopy(saved);
    }
    return text;
}

bool OpenWithSystem(const fs::path& path, std::string& error)
{
    const std::string url = FileUrl(AbsolutePath(path));
    if (SDL_OpenURL(url.c_str()))
        return true;
    error = "could not open " + url + ": " + SDL_GetError();
    return false;
}

std::string BackupStamp()
{
    const time_t now = time(nullptr);
    tm local{};
    localtime_s(&local, &now);
    char text[BACKUP_STAMP_CHARS] = {};
    strftime(text, sizeof(text), BACKUP_STAMP_FORMAT, &local);
    return text;
}
} // namespace Editor::Files

#endif // _EDITOR
