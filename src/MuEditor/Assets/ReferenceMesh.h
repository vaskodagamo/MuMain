#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>
#include <vector>

// A reference mesh for an item request: a high-poly .glb from a 3D generator that
// shows the wanted look. It is tens of megabytes, so it never enters git: Ask
// Codex copies it next to the checkouts, <repo>/../item-sources/<item key>/, where
// every worker worktree (a sibling folder) finds it under the same path, and the
// request's "What to change" names it with the steps that turn it into a game model.
namespace Editor::Assets::ReferenceMesh
{
std::filesystem::path Folder(const std::filesystem::path& repoRoot, const std::string& itemKey);

// Copies `source` into Folder() (a file already there is kept as is). Returns the
// stored path, or an empty path with `error` set.
std::filesystem::path Store(const std::filesystem::path& source, const std::filesystem::path& repoRoot,
                            const std::string& itemKey, std::string& error);

// The "What to change" lines for a stored mesh, one per line.
std::vector<std::string> DetailLines(const std::filesystem::path& stored);

// The summary for a request built from a mesh; it also names the request folder,
// so it differs from the concept request's summary.
std::string Summary(const std::string& itemName);
} // namespace Editor::Assets::ReferenceMesh

#endif // _EDITOR
