#pragma once

#ifdef _EDITOR

#include "Assets/EditorText.h"

#include <filesystem>
#include <string>
#include <vector>

// Where the editors keep durable copies of what they save.
//
// The game reads its files from the Data folder next to the executable, which the
// build copies from the repository's src/bin/Data and overwrites whenever
// anything in src/bin changes. So every save is written there (the running game
// sees it at once) and then copied into <repo>/src/bin/Data, the copy git tracks.
// The repo file it replaces is kept first in
// <repo>/out/editor-backups/<YYYYMMDD-HHMMSS>/Data/... (out/ is not in git).
//
// Only the file system is touched here (no engine or UI), so it can be tested on
// its own; EditorFiles wires it to the editor.
namespace Editor::Files
{
// The repository the editor mirrors into, or an empty root with the reason.
struct RepoRootLookup
{
    std::filesystem::path root; // absolute; empty when there is none
    std::string description;    // how it was found, or why none was, for the UI and the log
};

enum class RepoCopyResult
{
    Failed,    // nothing was copied; see MirrorOutcome::error
    Created,   // the repo had no file of that name; this is a new file
    Replaced,  // the repo file had other bytes; they were backed up first
    Unchanged, // the repo file already held exactly these bytes
    InPlace,   // the game's Data folder is the repo's src/bin/Data, so the save went there directly
};

// What happened to one saved file.
struct MirrorOutcome
{
    RepoCopyResult result = RepoCopyResult::Failed;
    std::filesystem::path repoFile;   // the file in <repo>/src/bin/Data
    std::filesystem::path backupFile; // where the replaced bytes went (Replaced only)
    std::string error;                // why the copy or the backup failed (Failed only)
};

// True when `dir` holds src/bin/Data and a .git entry (a folder in a clone, a
// file in a git worktree).
bool IsRepoRoot(const std::filesystem::path& dir);

// `overrideRoot` (from MU_EDITOR_REPO_ROOT) wins when not empty and must hold
// src/bin/Data. Otherwise each start folder is walked up to the first ancestor
// IsRepoRoot accepts; the build output lives inside the repository, so the
// executable's or the working directory's ancestors reach it.
RepoRootLookup LocateRepoRoot(const std::filesystem::path& overrideRoot,
                              const std::vector<std::filesystem::path>& startDirs);

// `dataRelative` is a relative path inside the game's Data folder that starts
// with "Data", e.g. Data/World1/EncTerrain1.obj.
bool IsDataRelative(const std::filesystem::path& dataRelative);
std::filesystem::path RepoDataFile(const std::filesystem::path& repoRoot, const std::filesystem::path& dataRelative);
std::filesystem::path RepoBackupFile(const std::filesystem::path& repoRoot, const std::string& stamp,
                                     const std::filesystem::path& dataRelative);
// <repo>/out/editor-exports, for files that are uploaded elsewhere (the server .att).
std::filesystem::path RepoExportDir(const std::filesystem::path& repoRoot);

// Copies the freshly saved `runtimeFile` over RepoDataFile(repoRoot, dataRelative).
// A repo file with other bytes is copied to RepoBackupFile(repoRoot, stamp, ..)
// first; a backup already there for the same stamp is kept, so it always holds
// the oldest bytes of that second.
MirrorOutcome MirrorIntoRepo(const std::filesystem::path& runtimeFile, const std::filesystem::path& dataRelative,
                             const std::filesystem::path& repoRoot, const std::string& stamp);

// True when both files exist and hold the same bytes.
bool SameFileContents(const std::filesystem::path& a, const std::filesystem::path& b);

// The path as UTF-8 text, for ImGui and the logs.
using Editor::Text::PathToUtf8;
} // namespace Editor::Files

#endif // _EDITOR
