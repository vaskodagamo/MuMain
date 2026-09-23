#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>

// Reads which commit a checkout is on straight from its .git files, without
// running git. A regeneration request records it as base_commit.
namespace Editor::Git
{
struct HeadInfo
{
    std::string commit; // 40 lower-case hex characters; empty when it could not be read
    std::string branch; // e.g. "main" or "feat/world-editor"; empty for a detached HEAD
    std::string error;  // why commit is empty
};

// The commit HEAD points at in the checkout at `repoRoot`. Follows a symbolic
// HEAD to its branch (a loose ref file wins over packed-refs, as in git) and
// understands a git worktree, whose .git is a file naming its own git folder.
HeadInfo ReadHead(const std::filesystem::path& repoRoot);

// True when a branch of the origin remote (refs/remotes/origin/...) points at
// exactly `commit`, i.e. the commit was on origin at the last fetch or push. Other
// remotes (the upstream project) do not count: workers fetch from origin only.
bool IsOriginBranchTip(const std::filesystem::path& repoRoot, const std::string& commit);

// True when the working-tree file `repoRelative` ('/' between folders, e.g.
// "src/bin/Data/World1/EncTerrain1.obj") holds exactly the content git's index
// records for it, i.e. `git status` does not list it as modified. The index
// normally equals the checked-out commit, so the file is then the one at HEAD.
// False when it differs, is not tracked or has a merge conflict, and when the
// index is in a form this does not read (index version 4, a split index).
bool IsFileUnchanged(const std::filesystem::path& repoRoot, const std::string& repoRelative);
} // namespace Editor::Git

#endif // _EDITOR
