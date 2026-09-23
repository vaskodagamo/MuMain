#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>
#include <vector>

#include "MapEditorRepoMirror.h"

// Shared file helpers for the Map Editor's save paths.
namespace Editor::Files
{
// Portable paths into the client data tree, relative to the working directory.
// Built with std::filesystem::path operator/, so they work with std::filesystem
// on every platform (a '\\' is an ordinary file-name character on macOS/Linux).
// The engine's own loaders (LoadBitmap, AccessModel, _wfopen) still take the
// legacy backslash strings; they resolve those themselves.
std::filesystem::path DataDir();            // Data
std::filesystem::path WorldDir(int world);  // Data/World{world}
std::filesystem::path ObjectDir(int world); // Data/Object{world}

// The map files the game loads from WorldDir(world) and the Map Editor saves.
std::filesystem::path TerrainMappingFile(int world);   // Data/World{world}/EncTerrain{world}.map
std::filesystem::path TerrainAttributeFile(int world); // Data/World{world}/EncTerrain{world}.att
std::filesystem::path TerrainObjectFile(int world);    // Data/World{world}/EncTerrain{world}.obj
std::filesystem::path TerrainHeightFile(int world);    // Data/World{world}/TerrainHeight.OZB
std::filesystem::path TerrainLightFile(int world);     // Data/World{world}/TerrainLight.OZJ

// Reads a whole file through _wfopen (which resolves the path on every
// platform). Returns an empty vector if the file is missing, empty or unreadable.
std::vector<unsigned char> ReadWholeFile(const std::filesystem::path& path);

// One file an editor save wrote, and where its durable copy went.
struct SavedFile
{
    std::filesystem::path runtimeFile; // absolute: the file the running game reads
    MirrorOutcome repo;                // the copy in <repo>/src/bin/Data (see MapEditorRepoMirror.h)
    std::filesystem::path localCopy;   // without a repo: the copy next to the executable
};

// The repository saves are mirrored into, looked up once: MU_EDITOR_REPO_ROOT,
// else the first folder above the executable or working directory that holds
// src/bin/Data and .git.
const RepoRootLookup& RepoRoot();

// Call right after writing `dataRelative` (built with DataDir/WorldDir/ObjectDir,
// e.g. Data/World1/EncTerrain1.obj). Copies the file into <repo>/src/bin/Data,
// backing up the repo file it replaces in <repo>/out/editor-backups/<time>/.
// Without a repo, it keeps the old behaviour: a copy in a folder next to the
// executable (Data/World7/X -> World7/X) that the next build does not overwrite.
// Logs every path it wrote.
SavedFile MirrorSavedFile(const std::filesystem::path& dataRelative);

// For files the user uploads elsewhere (the server .att), written next to the
// executable: also copies `file` into <repo>/out/editor-exports. Returns the
// copy's absolute path, or an empty path when there is no repo or it failed.
std::filesystem::path CopyToRepoExports(const std::filesystem::path& file);

// The absolute path of a file relative to the working directory, for status lines.
std::filesystem::path AbsolutePath(const std::filesystem::path& file);

// Status-line text listing the absolute paths each saved file went to.
std::string DescribeSavedFiles(const std::vector<SavedFile>& files);

// The local time as YYYYMMDD-HHMMSS: the backup folders' names, and default names of
// files the editor writes.
std::string Timestamp();

// Opens a file or folder with the system's default app (Finder or Preview on a
// Mac, Explorer or the image viewer on Windows). False with `error` when the
// system refused.
bool OpenWithSystem(const std::filesystem::path& path, std::string& error);
} // namespace Editor::Files

#endif // _EDITOR
