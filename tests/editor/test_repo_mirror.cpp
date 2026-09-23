#include <doctest.h>

#include "TempTree.h"
#include "UI/MapEditor/MapEditorRepoMirror.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace Editor::Files;
using EditorTest::ReadText;
using EditorTest::WriteText;

namespace
{
constexpr const char* STAMP = "20260923-101010";
constexpr const char* OBJECT_FILE = "Data/World1/EncTerrain1.obj";

class TempTree : public EditorTest::TempTree
{
public:
    explicit TempTree(const std::string& name) : EditorTest::TempTree("mu-repo-mirror-" + name) {}
};

// <root>/repo with src/bin/Data and .git, and the game folder inside its build output.
fs::path MakeRepo(const fs::path& root)
{
    const fs::path repo = root / "repo";
    fs::create_directories(repo / "src" / "bin" / "Data" / "World1");
    fs::create_directories(repo / ".git");
    return repo;
}

fs::path GameFolder(const fs::path& repo)
{
    const fs::path game = repo / "out" / "build" / "preset" / "src" / "Release" / "Main.app" / "Contents" / "MacOS";
    fs::create_directories(game / "Data" / "World1");
    return game;
}

bool SamePath(const fs::path& a, const fs::path& b)
{
    std::error_code ec;
    return fs::equivalent(a, b, ec);
}
} // namespace

TEST_CASE("The repository is found above the game folder [editor][saves]")
{
    TempTree tree("find");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path game = GameFolder(repo);

    const RepoRootLookup found = LocateRepoRoot({}, {game});
    CHECK(SamePath(found.root, repo));
    CHECK_FALSE(found.description.empty());
}

TEST_CASE("A git worktree, whose .git is a file, counts as a repository [editor][saves]")
{
    TempTree tree("worktree");
    const fs::path repo = tree.Root() / "worktree";
    fs::create_directories(repo / "src" / "bin" / "Data");
    WriteText(repo / ".git", "gitdir: elsewhere");

    CHECK(IsRepoRoot(repo));
    CHECK(SamePath(LocateRepoRoot({}, {GameFolder(repo)}).root, repo));
}

TEST_CASE("MU_EDITOR_REPO_ROOT wins, and must hold src/bin/Data [editor][saves]")
{
    TempTree tree("override");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path other = MakeRepo(tree.Root() / "other");

    CHECK(SamePath(LocateRepoRoot(other, {GameFolder(repo)}).root, other));

    const RepoRootLookup wrong = LocateRepoRoot(tree.Root() / "missing", {GameFolder(repo)});
    CHECK(wrong.root.empty());
    CHECK(wrong.description.find("MU_EDITOR_REPO_ROOT") != std::string::npos);
}

TEST_CASE("No repository above the game folder means no root [editor][saves]")
{
    TempTree tree("none");
    const fs::path game = tree.Root() / "Main.app" / "Contents" / "MacOS";
    fs::create_directories(game);

    const RepoRootLookup none = LocateRepoRoot({}, {game});
    CHECK(none.root.empty());
    CHECK_FALSE(none.description.empty());
}

TEST_CASE("A save replaces the repo file and keeps its old bytes [editor][saves]")
{
    TempTree tree("replace");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path runtimeFile = GameFolder(repo) / OBJECT_FILE;
    WriteText(RepoDataFile(repo, OBJECT_FILE), "shipped");
    WriteText(runtimeFile, "edited");

    const MirrorOutcome first = MirrorIntoRepo(runtimeFile, OBJECT_FILE, repo, STAMP);
    CHECK(first.result == RepoCopyResult::Replaced);
    CHECK(ReadText(RepoDataFile(repo, OBJECT_FILE)) == "edited");
    CHECK(SamePath(first.backupFile, repo / "out" / "editor-backups" / STAMP / OBJECT_FILE));
    CHECK(ReadText(first.backupFile) == "shipped");

    // A second save in the same second keeps the oldest bytes in the backup.
    WriteText(runtimeFile, "edited again");
    const MirrorOutcome second = MirrorIntoRepo(runtimeFile, OBJECT_FILE, repo, STAMP);
    CHECK(second.result == RepoCopyResult::Replaced);
    CHECK(ReadText(RepoDataFile(repo, OBJECT_FILE)) == "edited again");
    CHECK(ReadText(second.backupFile) == "shipped");
}

TEST_CASE("Saving unchanged bytes touches nothing in the repo [editor][saves]")
{
    TempTree tree("unchanged");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path runtimeFile = GameFolder(repo) / OBJECT_FILE;
    WriteText(RepoDataFile(repo, OBJECT_FILE), "same");
    WriteText(runtimeFile, "same");

    const MirrorOutcome outcome = MirrorIntoRepo(runtimeFile, OBJECT_FILE, repo, STAMP);
    CHECK(outcome.result == RepoCopyResult::Unchanged);
    CHECK(outcome.backupFile.empty());
    CHECK_FALSE(fs::exists(repo / "out" / "editor-backups"));
}

TEST_CASE("A file the repo does not have yet is created [editor][saves]")
{
    TempTree tree("create");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path texture = "Data/World1/ExtTile01.OZJ";
    const fs::path runtimeFile = GameFolder(repo) / texture;
    WriteText(runtimeFile, "texture");

    const MirrorOutcome outcome = MirrorIntoRepo(runtimeFile, texture, repo, STAMP);
    CHECK(outcome.result == RepoCopyResult::Created);
    CHECK(ReadText(RepoDataFile(repo, texture)) == "texture");
    CHECK(outcome.backupFile.empty());
}

TEST_CASE("A game that reads the repo's Data directly is saved in place [editor][saves]")
{
    TempTree tree("inplace");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path repoFile = RepoDataFile(repo, OBJECT_FILE);
    WriteText(repoFile, "edited");

    const MirrorOutcome outcome = MirrorIntoRepo(repoFile, OBJECT_FILE, repo, STAMP);
    CHECK(outcome.result == RepoCopyResult::InPlace);
    CHECK(ReadText(repoFile) == "edited");
}

TEST_CASE("Only paths inside Data are mirrored [editor][saves]")
{
    CHECK(IsDataRelative(OBJECT_FILE));
    CHECK_FALSE(IsDataRelative("World1/EncTerrain1.obj"));
    CHECK_FALSE(IsDataRelative("Data"));
    CHECK_FALSE(IsDataRelative("Data/../config.ini"));
    CHECK_FALSE(IsDataRelative(fs::temp_directory_path() / "Data" / "x"));

    TempTree tree("outside");
    const fs::path repo = MakeRepo(tree.Root());
    const fs::path runtimeFile = GameFolder(repo) / "config.ini";
    WriteText(runtimeFile, "x");
    const MirrorOutcome outcome = MirrorIntoRepo(runtimeFile, "config.ini", repo, STAMP);
    CHECK(outcome.result == RepoCopyResult::Failed);
    CHECK_FALSE(outcome.error.empty());
}

TEST_CASE("Files compare by their bytes [editor][saves]")
{
    TempTree tree("compare");
    WriteText(tree.Root() / "a", "abc");
    WriteText(tree.Root() / "b", "abc");
    WriteText(tree.Root() / "c", "abd");
    WriteText(tree.Root() / "d", "abcd");

    CHECK(SameFileContents(tree.Root() / "a", tree.Root() / "b"));
    CHECK_FALSE(SameFileContents(tree.Root() / "a", tree.Root() / "c"));
    CHECK_FALSE(SameFileContents(tree.Root() / "a", tree.Root() / "d"));
    CHECK_FALSE(SameFileContents(tree.Root() / "a", tree.Root() / "missing"));
}
