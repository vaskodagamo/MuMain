#include <doctest.h>

#include "TempTree.h"

#include "Assets/AssetCatalog.h"
#include "Assets/CaptureImage.h"
#include "Assets/ClientReview.h"
#include "Assets/FileDigest.h"
#include "Assets/GitCheckout.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace Editor::Assets;
using EditorTest::ReadText;
using EditorTest::TempTree;
using EditorTest::WriteText;

namespace
{
constexpr const char* COMMIT_A = "7b8084737e6c4e73fcbe7679b22cb0574515c676";
constexpr const char* COMMIT_B = "0f589224a034ae5b2509e9b2793b732ab9f130af";

// Two typed models sharing a texture, one untyped model, in catalog.json's shape.
constexpr const char* SMALL_CATALOG = R"({
 "schema": "mu-world-catalog/1", "world": 1, "world_name": "Lorencia",
 "models": {
  "10": {"name": "Tree02", "type": 10, "bmd": "src/bin/Data/Object1/Tree02.bmd", "identity": "second tree",
         "in_scope": true, "exclusion": null, "status": "accepted", "client_verified": false,
         "placement_count": 3, "textures": {"tree.jpg": "src/bin/Data/Object1/tree.OZJ"},
         "shared_with": {"tree.jpg": ["Tree01"]}, "engine_controls": [], "current_sha256": "aa",
         "bmd_modified": true, "original": {"revision": "ac0f6dd8", "archive": null},
         "last_batch": null, "model_dir": null, "batches": [], "previews": {}, "requests": []},
  "9": {"name": "Tree01", "type": 9, "bmd": "src/bin/Data/Object1/Tree01.bmd", "identity": "first tree",
        "in_scope": true, "exclusion": null, "status": "accepted", "client_verified": true,
        "client_review": {"verdict": "looks-good", "note": "fine", "date": "2026-09-23"},
        "placement_count": 80,
        "textures": {"tree.jpg": "src/bin/Data/Object1/tree.OZJ", "bark.jpg": "src/bin/Data/Object1/bark.OZJ"},
        "shared_with": {"tree.jpg": ["Tree02"], "bark.jpg": []},
        "engine_controls": [{"source_row": "Tree01/02", "controls": ["BlendMesh=1", "Velocity=0.4"],
                             "requirement": "keep sway", "blend_mesh": 1, "blend_mesh_texture": "tree.jpg"}],
        "current_sha256": "bb", "bmd_modified": true,
        "original": {"revision": "ac0f6dd8", "archive": "assets-work/World1/Trees01/Tree01/original/Tree01.bmd"},
        "last_batch": "Trees01", "model_dir": "assets-work/World1/Trees01/Tree01",
        "batches": [{"name": "Trees01", "role": "primary", "agent": "trees", "notes": "assets-work/World1/Trees01/README.md",
                     "model_dir": "assets-work/World1/Trees01/Tree01", "integration_commits": ["ac16ffeb"]}],
        "previews": {"baseline": "a/baseline.png", "final": "a/final.png"},
        "requests": [{"id": "2026-09-23-tree01-greener", "status": "open", "assigned_to": null}]}
 },
 "untyped_models": {
  "Bird01": {"name": "Bird01", "type": null, "bmd": "src/bin/Data/Object1/Bird01.bmd", "identity": "bird",
             "in_scope": false, "exclusion": "animated fauna", "status": "blocked", "textures": {}}
 }
})";

Catalog ParseSmallCatalog()
{
    Catalog catalog;
    std::string error;
    REQUIRE(ParseCatalog(SMALL_CATALOG, catalog, error));
    return catalog;
}

// <root>/repo with a .git folder whose HEAD is `head`.
fs::path MakeGitRepo(const fs::path& root, const std::string& head)
{
    const fs::path repo = root / "repo";
    WriteText(repo / ".git" / "HEAD", head + "\n");
    return repo;
}

void PutBigEndian(std::string& out, std::uint32_t value, int bytes)
{
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8)
        out.push_back(static_cast<char>((value >> shift) & 0xFF));
}

std::string BytesFromHex(const std::string& hex)
{
    std::string bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        bytes.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    return bytes;
}

// `gitDir`/index in version 2, the format `git add` writes: one entry per path of
// `repo`, recorded with the content the file has now; `stage` marks merge conflicts.
void WriteGitIndex(const fs::path& gitDir, const fs::path& repo, const std::vector<std::string>& paths,
                   std::uint32_t stage = 0)
{
    std::string index = "DIRC";
    PutBigEndian(index, 2, 4);
    PutBigEndian(index, static_cast<std::uint32_t>(paths.size()), 4);
    for (const std::string& path : paths)
    {
        const std::size_t start = index.size();
        index.append(40, '\0'); // file stat data, not read
        index += BytesFromHex(Editor::Files::GitBlobIdHex(repo / path));
        PutBigEndian(index, (stage << 12) | static_cast<std::uint32_t>(path.size()), 2);
        index += path;
        index.append(8 - (index.size() - start) % 8, '\0');
    }
    index.append(20, '\0'); // the index checksum, not read
    WriteText(gitDir / "index", index);
}

// Width and height from a JPEG's first baseline start-of-frame marker.
std::pair<int, int> JpegSize(const std::vector<std::uint8_t>& jpeg)
{
    constexpr std::uint8_t MARKER = 0xFF;
    constexpr std::uint8_t BASELINE_FRAME = 0xC0;
    for (std::size_t i = 2; i + 8 < jpeg.size(); ++i)
    {
        if (jpeg[i] == MARKER && jpeg[i + 1] == BASELINE_FRAME)
            return {(jpeg[i + 7] << 8) | jpeg[i + 8], (jpeg[i + 5] << 8) | jpeg[i + 6]};
    }
    return {0, 0};
}
} // namespace

TEST_CASE("The catalog lists typed models by type, then untyped ones [editor][assets]")
{
    const Catalog catalog = ParseSmallCatalog();
    REQUIRE(catalog.models.size() == 3);
    CHECK(catalog.world == 1);
    CHECK(catalog.worldName == "Lorencia");
    CHECK(catalog.models[0].name == "Tree01");
    CHECK(catalog.models[1].name == "Tree02");
    CHECK(catalog.models[2].name == "Bird01");
    CHECK(catalog.models[2].type == NO_TYPE);
    CHECK(catalog.FindByType(10)->name == "Tree02");
    CHECK(catalog.FindByType(NO_TYPE) == nullptr);
    CHECK(catalog.FindByName("Bird01")->exclusion == "animated fauna");
}

TEST_CASE("A catalog model keeps every fact a request copies [editor][assets]")
{
    const Catalog catalog = ParseSmallCatalog();
    const CatalogModel& tree = *catalog.FindByName("Tree01");
    CHECK(tree.placementCount == 80);
    REQUIRE(tree.textures.size() == 2);
    CHECK(tree.textures[0].name == "bark.jpg"); // sorted by texture name
    CHECK(tree.textures[1].sharedWith == std::vector<std::string>{"Tree02"});
    REQUIRE(tree.engineControls.size() == 1);
    CHECK(tree.engineControls[0].blendMesh == 1);
    CHECK(tree.engineControls[0].blendMeshTexture == std::optional<std::string>("tree.jpg"));
    CHECK(tree.original.archive == std::optional<std::string>("assets-work/World1/Trees01/Tree01/original/Tree01.bmd"));
    CHECK(tree.lastBatch == std::optional<std::string>("Trees01"));
    CHECK(catalog.FindByName("Tree02")->modelDir == std::nullopt);
    REQUIRE(tree.clientReview.has_value());
    CHECK(tree.clientReview->verdict == VERDICT_LOOKS_GOOD);
    CHECK(tree.batches.front().integrationCommits == std::vector<std::string>{"ac16ffeb"});
    CHECK(tree.finalPreview == "a/final.png");
    CHECK(tree.requests.front().assignedTo.empty());

    const auto consumers = TextureConsumers(tree);
    CHECK(consumers.at("src/bin/Data/Object1/tree.OZJ") == std::vector<std::string>{"Tree01", "Tree02"});
    CHECK(consumers.at("src/bin/Data/Object1/bark.OZJ") == std::vector<std::string>{"Tree01"});
}

TEST_CASE("A missing or foreign catalog is reported, not guessed [editor][assets]")
{
    TempTree tree("mu-asset-catalog-missing");
    const CatalogLoad none = LoadCatalog(tree.Root(), 3);
    CHECK_FALSE(none.fileFound);
    CHECK_FALSE(none.catalog.has_value());

    WriteText(CatalogFile(tree.Root(), 3), R"({"schema": "something-else"})");
    const CatalogLoad foreign = LoadCatalog(tree.Root(), 3);
    CHECK(foreign.fileFound);
    CHECK_FALSE(foreign.catalog.has_value());
    CHECK_FALSE(foreign.error.empty());
}

#ifdef MU_REPO_ROOT
TEST_CASE("The repository's own World1 catalog loads [editor][assets]")
{
    const CatalogLoad load = LoadCatalog(MU_REPO_ROOT, 1);
    REQUIRE(load.catalog.has_value());
    const Catalog& catalog = *load.catalog;
    CHECK(catalog.models.size() > 100);
    const CatalogModel* tree = catalog.FindByName("Tree01");
    REQUIRE(tree != nullptr);
    CHECK(tree->type == 0);
    CHECK(tree->bmd == "src/bin/Data/Object1/Tree01.bmd");
    CHECK_FALSE(tree->textures.empty());
}
#endif

TEST_CASE("Client verdicts are merged into client-review.json [editor][assets]")
{
    TempTree tree("mu-asset-client-review");
    const fs::path file = ClientReviewFile(tree.Root(), 1);
    std::string error;
    REQUIRE(RecordClientReview(file, "Tree02", {VERDICT_NEEDS_WORK, "too dark", "2026-09-23"}, error));
    REQUIRE(RecordClientReview(file, "Tree01", {VERDICT_LOOKS_GOOD, "", "2026-09-23"}, error));
    REQUIRE(RecordClientReview(file, "Tree02", {VERDICT_LOOKS_GOOD, "fixed", "2026-09-24"}, error));

    const std::string text = ReadText(file);
    CHECK(text.find("\"Tree01\"") < text.find("\"Tree02\"")); // sorted keys
    const ClientReviews reviews = ReadClientReviews(file, error);
    REQUIRE(reviews.size() == 2);
    CHECK(reviews.at("Tree02").note == "fixed");
    CHECK(reviews.at("Tree02").date == "2026-09-24");
    CHECK(reviews.at("Tree01").verdict == VERDICT_LOOKS_GOOD);

    WriteText(file, "not json");
    CHECK_FALSE(RecordClientReview(file, "Tree01", {VERDICT_LOOKS_GOOD, "", "2026-09-23"}, error));
    CHECK(ReadText(file) == "not json");
}

TEST_CASE("HEAD is read through a branch, loose refs before packed ones [editor][git]")
{
    TempTree tree("mu-git-branch");
    const fs::path repo = MakeGitRepo(tree.Root(), "ref: refs/heads/feat/world-editor");
    WriteText(repo / ".git" / "packed-refs", std::string("# pack-refs with: peeled fully-peeled sorted\n") + COMMIT_B +
                                                 " refs/heads/feat/world-editor\n" + COMMIT_B +
                                                 " refs/remotes/origin/main\n");

    Editor::Git::HeadInfo head = Editor::Git::ReadHead(repo);
    CHECK(head.commit == COMMIT_B);
    CHECK(head.branch == "feat/world-editor");

    WriteText(repo / ".git" / "refs" / "heads" / "feat" / "world-editor", std::string(COMMIT_A) + "\n");
    head = Editor::Git::ReadHead(repo);
    CHECK(head.commit == COMMIT_A);
    CHECK_FALSE(Editor::Git::IsOriginBranchTip(repo, COMMIT_A));
    CHECK(Editor::Git::IsOriginBranchTip(repo, COMMIT_B));

    // A loose remote ref replaces the stale packed line of the same name.
    WriteText(repo / ".git" / "refs" / "remotes" / "origin" / "main", std::string(COMMIT_A) + "\n");
    CHECK(Editor::Git::IsOriginBranchTip(repo, COMMIT_A));
    CHECK_FALSE(Editor::Git::IsOriginBranchTip(repo, COMMIT_B));
}

TEST_CASE("Only origin's branches count as pushed, not another remote's [editor][git]")
{
    TempTree tree("mu-git-upstream");
    const fs::path repo = MakeGitRepo(tree.Root(), COMMIT_A);
    WriteText(repo / ".git" / "refs" / "remotes" / "upstream" / "main", std::string(COMMIT_A) + "\n");
    WriteText(repo / ".git" / "packed-refs",
              std::string(COMMIT_B) + " refs/remotes/upstream/feature\n" + COMMIT_B + " refs/remotes/originals/main\n");
    CHECK_FALSE(Editor::Git::IsOriginBranchTip(repo, COMMIT_A));
    CHECK_FALSE(Editor::Git::IsOriginBranchTip(repo, COMMIT_B));

    WriteText(repo / ".git" / "refs" / "remotes" / "origin" / "feat" / "x", std::string(COMMIT_A) + "\n");
    CHECK(Editor::Git::IsOriginBranchTip(repo, COMMIT_A));
}

TEST_CASE("A detached HEAD and a git worktree are understood [editor][git]")
{
    TempTree tree("mu-git-worktree");
    const fs::path detached = MakeGitRepo(tree.Root() / "detached", COMMIT_A);
    const Editor::Git::HeadInfo head = Editor::Git::ReadHead(detached);
    CHECK(head.commit == COMMIT_A);
    CHECK(head.branch.empty());

    // main/.git holds the refs; the worktree's .git file names .git/worktrees/wt.
    const fs::path common = tree.Root() / "main" / ".git";
    WriteText(common / "refs" / "heads" / "codex" / "work", std::string(COMMIT_B) + "\n");
    WriteText(common / "worktrees" / "wt" / "HEAD", "ref: refs/heads/codex/work\n");
    WriteText(common / "worktrees" / "wt" / "commondir", "../..\n");
    const fs::path worktree = tree.Root() / "wt";
    WriteText(worktree / ".git", "gitdir: " + (common / "worktrees" / "wt").generic_string() + "\n");

    const Editor::Git::HeadInfo worktreeHead = Editor::Git::ReadHead(worktree);
    CHECK(worktreeHead.commit == COMMIT_B);
    CHECK(worktreeHead.branch == "codex/work");
}

TEST_CASE("A branch without commits and a missing .git give an error [editor][git]")
{
    TempTree tree("mu-git-errors");
    const fs::path repo = MakeGitRepo(tree.Root(), "ref: refs/heads/new");
    const Editor::Git::HeadInfo unborn = Editor::Git::ReadHead(repo);
    CHECK(unborn.commit.empty());
    CHECK_FALSE(unborn.error.empty());

    const Editor::Git::HeadInfo none = Editor::Git::ReadHead(tree.Root() / "nothing");
    CHECK(none.commit.empty());
    CHECK_FALSE(none.error.empty());
}

TEST_CASE("A file counts as unchanged only when git's index holds its content [editor][git]")
{
    TempTree tree("mu-git-index");
    const fs::path repo = MakeGitRepo(tree.Root(), COMMIT_A);
    const std::string objects = "src/bin/Data/World1/EncTerrain1.obj";
    WriteText(repo / "README.md", "hello\n");
    WriteText(repo / objects, std::string("\x01\x00\x02records", 10));
    WriteGitIndex(repo / ".git", repo, {"README.md", objects});

    CHECK(Editor::Git::IsFileUnchanged(repo, "README.md"));
    CHECK(Editor::Git::IsFileUnchanged(repo, objects));
    CHECK_FALSE(Editor::Git::IsFileUnchanged(repo, "untracked.txt"));

    WriteText(repo / objects, "saved by the editor");
    CHECK_FALSE(Editor::Git::IsFileUnchanged(repo, objects));
    CHECK(Editor::Git::IsFileUnchanged(repo, "README.md"));

    // A merge conflict, an index cut short and a format this does not read all say "changed".
    WriteGitIndex(repo / ".git", repo, {"README.md"}, 2);
    CHECK_FALSE(Editor::Git::IsFileUnchanged(repo, "README.md"));
    WriteGitIndex(repo / ".git", repo, {"README.md"});
    std::string index = ReadText(repo / ".git" / "index");
    WriteText(repo / ".git" / "index", index.substr(0, 70));
    CHECK_FALSE(Editor::Git::IsFileUnchanged(repo, "README.md"));
    index[7] = 4; // version 4 compresses the paths
    WriteText(repo / ".git" / "index", index);
    CHECK_FALSE(Editor::Git::IsFileUnchanged(repo, "README.md"));
}

TEST_CASE("A git worktree's own index is read [editor][git]")
{
    TempTree tree("mu-git-worktree-index");
    const fs::path common = tree.Root() / "main" / ".git";
    const fs::path worktreeGit = common / "worktrees" / "wt";
    WriteText(worktreeGit / "HEAD", std::string(COMMIT_A) + "\n");
    WriteText(worktreeGit / "commondir", "../..\n");
    const fs::path worktree = tree.Root() / "wt";
    WriteText(worktree / ".git", "gitdir: " + worktreeGit.generic_string() + "\n");
    WriteText(worktree / "README.md", "hello\n");
    WriteGitIndex(worktreeGit, worktree, {"README.md"});
    CHECK(Editor::Git::IsFileUnchanged(worktree, "README.md"));
}

TEST_CASE("Git blob ids match git hash-object [editor][git]")
{
    TempTree tree("mu-git-blob");
    WriteText(tree.Root() / "hello.txt", "hello\n");
    WriteText(tree.Root() / "empty.txt", "");
    CHECK(Editor::Files::GitBlobIdHex(tree.Root() / "hello.txt") == "ce013625030ba8dba906f756967f9e9ca394464a");
    CHECK(Editor::Files::GitBlobIdHex(tree.Root() / "empty.txt") == "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");
    CHECK(Editor::Files::GitBlobIdHex(tree.Root() / "missing").empty());
}

TEST_CASE("File SHA-256 matches the standard test vector [editor][assets]")
{
    TempTree tree("mu-sha256");
    WriteText(tree.Root() / "abc.txt", "abc");
    CHECK(Editor::Files::Sha256Hex(tree.Root() / "abc.txt") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Editor::Files::Sha256Hex(tree.Root() / "missing").empty());
}

TEST_CASE("A capture is shrunk to 1920 px and encoded as a JPEG of that size [editor][capture]")
{
    mu::FramePixels frame;
    frame.width = 3840;
    frame.height = 20;
    frame.rgb.resize(static_cast<std::size_t>(frame.width) * frame.height * 3);
    for (std::size_t pixel = 0; pixel < frame.rgb.size() / 3; ++pixel)
    {
        const bool odd = (pixel % frame.width) % 2 == 1;
        frame.rgb[pixel * 3] = odd ? 200 : 100; // every output pixel averages one of each
        frame.rgb[pixel * 3 + 1] = 50;
        frame.rgb[pixel * 3 + 2] = 0;
    }

    const mu::FramePixels small = Editor::Capture::DownscaleToWidth(frame, Editor::Capture::MAX_CAPTURE_WIDTH);
    CHECK(small.width == 1920);
    CHECK(small.height == 10);
    CHECK(small.rgb[0] == 150);
    CHECK(small.rgb[1] == 50);

    const mu::FramePixels same = Editor::Capture::DownscaleToWidth(small, Editor::Capture::MAX_CAPTURE_WIDTH);
    CHECK(same.width == 1920);

    const std::vector<std::uint8_t> jpeg = Editor::Capture::EncodeJpeg(small, Editor::Capture::CAPTURE_JPEG_QUALITY);
    REQUIRE(jpeg.size() > 4);
    CHECK(jpeg[0] == 0xFF);
    CHECK(jpeg[1] == 0xD8);
    CHECK(JpegSize(jpeg) == std::make_pair(1920, 10));
    CHECK(Editor::Capture::EncodeJpeg(mu::FramePixels{}, Editor::Capture::CAPTURE_JPEG_QUALITY).empty());
}
