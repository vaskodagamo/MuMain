#include <doctest.h>

#include "TempTree.h"

#include "Assets/RegenRequest.h"
#include "Assets/RequestBrief.h"
#include "Assets/RequestFolder.h"
#include "Assets/RequestNaming.h"

#include <json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace Editor::Assets;
using EditorTest::ReadText;
using EditorTest::TempTree;
using EditorTest::WriteText;
using nlohmann::json;

namespace
{
constexpr const char* BASE_COMMIT = "7b8084737e6c4e73fcbe7679b22cb0574515c676";
constexpr const char* ID = "2026-09-23-sign01-readable-board";

CatalogModel Model(const std::string& name, int type, std::vector<TextureLink> textures)
{
    CatalogModel model;
    model.name = name;
    model.type = type;
    model.bmd = "src/bin/Data/Object1/" + name + ".bmd";
    model.identity = name + " identity";
    model.status = "accepted";
    model.placementCount = 3;
    model.textures = std::move(textures);
    model.original = {"ac0f6dd8", "assets-work/World1/SignsBanners01/" + name + "/original/" + name + ".bmd"};
    model.lastBatch = "SignsBanners01";
    model.modelDir = "assets-work/World1/SignsBanners01/" + name;
    model.finalPreview = "assets-work/World1/coordination/final-inspection/" + name + "/final-offline.png";
    model.batches.push_back({"SignsBanners01",
                             "primary",
                             "signs",
                             "assets-work/World1/SignsBanners01/notes.md",
                             model.modelDir,
                             {"1a2b3c4d"}});
    return model;
}

// Sign01 as in the README example: notice.OZJ is shared with Sign02.
CatalogModel Sign01()
{
    CatalogModel sign = Model("Sign01", 96,
                              {{"doorknob.tga", "src/bin/Data/Object1/doorknob.OZT", {}},
                               {"notice.jpg", "src/bin/Data/Object1/notice.OZJ", {"Sign02"}},
                               {"signboard.tga", "src/bin/Data/Object1/signboard.OZT", {}}});
    sign.engineControls.push_back({"Sign01/02", {"Velocity=0.3"}, "Preserve full original sign motion", {}, {}});
    return sign;
}

CatalogModel Sign02()
{
    CatalogModel sign = Model("Sign02", 97, {{"notice.jpg", "src/bin/Data/Object1/notice.OZJ", {"Sign01"}}});
    sign.engineControls.push_back(
        {"Sign01/02", {"Velocity=0.3"}, "Preserve full original sign motion", 1, "notice.jpg"});
    return sign;
}

RequestDraft SignDraft(RequestKind kind)
{
    RequestDraft draft;
    draft.id = ID;
    draft.created = "2026-09-23T10:15:00+02:00";
    draft.baseCommit = BASE_COMMIT;
    draft.input.kind = kind;
    draft.input.summary = "Make the hanging board read as a tavern sign";
    draft.input.details = {"Stronger painted border"};
    draft.input.keep = {"Board shape"};
    RequestTarget target{Sign01(), std::string(64, 'e'), {}};
    PickedInstance picked;
    picked.position = {11647.5810546875f, 11376.6376953125f, 315.0f};
    picked.rotation = {0.0f, 0.0f, 1260.0f};
    picked.tile = {116.475810546875, 113.766376953125};
    picked.objIndex = 42;
    target.picked.push_back(picked);
    draft.targets.push_back(target);
    CaptureInfo capture;
    capture.cameraPosition = {11620.0f, 11180.0f, 620.0f};
    capture.cameraAngle = {-48.5f, 0.0f, -45.0f};
    capture.freeFly = true;
    capture.width = 1920;
    capture.height = 1080;
    capture.clientCommit = "7b808473";
    draft.captures.push_back(capture);
    return draft;
}
} // namespace

TEST_CASE("Slugs take the summary's first content words [editor][requests]")
{
    CHECK(MakeSlug("Make the hanging board read as a tavern sign") == "make-hanging-board-read");
    CHECK(MakeSlug("Greener LEAVES, please!") == "greener-leaves-please");
    CHECK(MakeSlug("The a of") == "the-a-of");
    CHECK(MakeSlug("!!!") == "request");
}

TEST_CASE("Request ids are unique per model and slug on any date [editor][requests]")
{
    CHECK(MakeRequestId("2026-09-23", "Sign01", "readable-board", {}) == ID);
    const std::vector<std::string> existing = {"2026-09-20-sign01-readable-board",
                                               "2026-09-21-sign01-readable-board-2"};
    CHECK(MakeRequestId("2026-09-23", "Sign01", "readable-board", existing) == "2026-09-23-sign01-readable-board-3");
    CHECK(MakeRequestId("2026-09-23", "Sign01", "a-b-c-d-e", {"2026-01-01-sign01-a-b-c-d-e"}) ==
          "2026-09-23-sign01-a-b-c-d-2");
    const std::string longWord(40, 'x');
    const std::string id = MakeRequestId("2026-09-23", "Sign01", longWord + "-" + longWord, {});
    CHECK(id == "2026-09-23-sign01-" + longWord);
}

TEST_CASE("A long id that collides keeps its number within 80 characters [editor][requests]")
{
    const std::string slug = MakeSlug("longwordnumberone longwordnumbertwo longwordnumber3 alphabeta");
    const std::string first = MakeRequestId("2026-09-23", "Stone02", slug, {});
    REQUIRE(first.size() == 80);
    // Filed the same day, or on another day: the worker branch name collides either way.
    for (const std::string& existing : {first, "2026-09-22" + first.substr(10)})
    {
        const std::string second = MakeRequestId("2026-09-23", "Stone02", slug, {existing});
        CHECK(second == "2026-09-23-stone02-longwordnumberone-longwordnumbertwo-longwordnumber3-2");
    }

    const std::string word(95, 'w');
    const std::string cut = MakeRequestId("2026-09-23", "Stone02", word, {});
    CHECK(cut == "2026-09-23-stone02-" + std::string(61, 'w'));
    const std::string cutAgain = MakeRequestId("2026-09-23", "Stone02", word, {cut});
    CHECK(cutAgain == "2026-09-23-stone02-" + std::string(59, 'w') + "-2");
}

TEST_CASE("A new variant gets the next free number of its family [editor][requests]")
{
    CHECK(NextVariantName("Sign01", {"sign01", "sign02"}) == "Sign03");
    CHECK(NextVariantName("Tree10", {"tree01", "tree10"}) == "Tree02");
    CHECK(NextVariantName("House", {}).empty());
}

TEST_CASE("Timestamps are RFC 3339 with the local offset [editor][requests]")
{
    std::tm local{};
    local.tm_year = 2026 - 1900;
    local.tm_mon = 8;
    local.tm_mday = 23;
    local.tm_hour = 10;
    local.tm_min = 15;
    const Timestamp east = FormatTimestamp(local, 120);
    CHECK(east.date == "2026-09-23");
    CHECK(east.dateTime == "2026-09-23T10:15:00+02:00");
    CHECK(FormatTimestamp(local, -330).dateTime == "2026-09-23T10:15:00-05:30");
    CHECK(CurrentTimestamp().dateTime.size() == std::string("2026-09-23T10:15:00+02:00").size());
}

TEST_CASE("Shared textures are frozen unless every consumer is a target [editor][requests]")
{
    std::vector<RequestTarget> targets = {{Sign01(), "", {}}};
    RequestScope repaint = ComputeScope(RequestKind::Repaint, targets);
    REQUIRE(repaint.sharedTextures.size() == 1);
    CHECK(repaint.sharedTextures[0].first == "src/bin/Data/Object1/notice.OZJ");
    CHECK(repaint.sharedTextures[0].second == std::vector<std::string>{"Sign01", "Sign02"});
    CHECK(repaint.frozenTextures == std::vector<std::string>{"src/bin/Data/Object1/notice.OZJ"});
    CHECK(repaint.ownedFiles ==
          std::vector<std::string>{"src/bin/Data/Object1/doorknob.OZT", "src/bin/Data/Object1/signboard.OZT"});

    CHECK(ComputeScope(RequestKind::Remodel, targets).ownedFiles ==
          std::vector<std::string>{"src/bin/Data/Object1/Sign01.bmd"});
    CHECK(ComputeScope(RequestKind::RepaintRemodel, targets).ownedFiles.size() == 3);
    CHECK(ComputeScope(RequestKind::NewVariant, targets).ownedFiles.empty());

    targets.push_back({Sign02(), "", {}});
    const RequestScope both = ComputeScope(RequestKind::Repaint, targets);
    CHECK(both.frozenTextures.empty());
    CHECK(both.ownedFiles.size() == 3);
}

TEST_CASE("request.json follows the contract for an open request [editor][requests]")
{
    const json request = json::parse(BuildRequestJson(SignDraft(RequestKind::Repaint)));
    CHECK(request["schema"] == "mu-regen-request/1");
    CHECK(request["id"] == ID);
    CHECK(request["world"] == 1);
    CHECK(request["status"] == "open");
    CHECK(request["status_history"].size() == 1);
    CHECK(request["status_history"][0]["at"] == request["created"]);
    CHECK(request["kind"] == "repaint");
    CHECK(request["base_commit"] == BASE_COMMIT);
    CHECK(request["supersedes"].is_null());
    CHECK(request["result"].is_null());
    CHECK(request["decision"].is_null());

    const json& target = request["targets"][0];
    CHECK(target["model"] == "Sign01");
    CHECK(target["type"] == 96);
    CHECK(target["textures"]["notice.jpg"] == "src/bin/Data/Object1/notice.OZJ");
    CHECK(target["picked_instances"][0]["obj_index"] == 42);
    CHECK(target["picked_instances"][0]["position"][0] == 11647.5810546875);
    CHECK(target["picked_instances"][0]["tile"][0] == 116.475810546875);
    CHECK(target["original"]["revision"] == "ac0f6dd8");

    const json& constraints = request["constraints"];
    CHECK(constraints["frozen_textures"] == json::array({"src/bin/Data/Object1/notice.OZJ"}));
    CHECK(constraints["protected_paths"][0] == "src/bin/Data/World1/");
    CHECK(constraints["engine_controls"][0]["model"] == "Sign01");
    CHECK_FALSE(constraints["engine_controls"][0].contains("blend_mesh"));
    CHECK(constraints["must_keep"].size() == 7);
    CHECK_FALSE(request["change"].contains("new_model"));

    const json& capture = request["evidence"]["captures"][0];
    CHECK(capture["file"] == std::string("assets-work/World1/requests/") + ID + "/captures/01-current.jpg");
    CHECK(capture["resolution"] == json::array({1920, 1080}));
    CHECK(capture["hero_tile"].is_null());
    CHECK(capture["camera"]["free_fly"] == true);
    CHECK_FALSE(capture["camera"].contains("distance"));
    CHECK(request["evidence"]["offline_previews"].size() == 1);

    const json& handoff = request["handoff"];
    CHECK(handoff["repo"] == "vaskodagamo/MuMain");
    CHECK(handoff["branch"] == "codex/lorencia-req-sign01-readable-board");
    CHECK(handoff["worktree"] == "../MuMain-lorencia-req-sign01-readable-board");
    CHECK(handoff["deliver_to"] == std::string("assets-work/World1/requests/") + ID + "/delivery/");
    CHECK(handoff["push_allowed"] == false);
    CHECK(handoff["claimed_by"].is_null());
    CHECK(handoff["start_commit"].is_null());
}

TEST_CASE("A new-variant request names its model and owns nothing [editor][requests]")
{
    RequestDraft draft = SignDraft(RequestKind::NewVariant);
    draft.newModel = "Sign03";
    const json request = json::parse(BuildRequestJson(draft));
    CHECK(request["change"]["new_model"] == "Sign03");
    CHECK(request["constraints"]["owned_files"].empty());
}

TEST_CASE("Engine control rows keep their optional mesh fields [editor][requests]")
{
    RequestDraft draft = SignDraft(RequestKind::Repaint);
    draft.targets.push_back({Sign02(), std::string(64, 'f'), {}});
    const json request = json::parse(BuildRequestJson(draft));
    const json& row = request["constraints"]["engine_controls"][1];
    CHECK(row["model"] == "Sign02");
    CHECK(row["blend_mesh"] == 1);
    CHECK(row["blend_mesh_texture"] == "notice.jpg");
    CHECK(request["constraints"]["frozen_textures"].empty());
}

TEST_CASE("brief.md links the contract, the notes and the capture [editor][requests]")
{
    const std::string brief = BuildBrief(SignDraft(RequestKind::Repaint));
    CHECK(brief.find(std::string("# Regeneration request ") + ID) == 0);
    CHECK(brief.find("](../../../../ASTRA.md)") != std::string::npos);
    CHECK(brief.find("(../../../../assets-work/World1/SignsBanners01/notes.md)") != std::string::npos);
    CHECK(brief.find("![In-client view](captures/01-current.jpg)") != std::string::npos);
    CHECK(brief.find("`src/bin/Data/Object1/notice.OZJ`, shared with Sign02 (frozen)") != std::string::npos);
    CHECK(brief.find("## Must keep") != std::string::npos);
}

TEST_CASE("A request folder is written once and never half [editor][requests]")
{
    TempTree tree("mu-request-folder");
    const fs::path repo = tree.Root();
    const RequestDraft draft = SignDraft(RequestKind::Repaint);
    const std::vector<std::uint8_t> jpeg = {0xFF, 0xD8, 0xFF, 0xD9};

    fs::path folder;
    std::string error;
    REQUIRE(WriteRequestFolder(repo, draft, jpeg, folder, error));
    CHECK(folder == RequestsDir(repo, 1) / ID);
    CHECK(ReadText(folder / "request.json") == BuildRequestJson(draft));
    CHECK(ReadText(folder / "brief.md") == BuildBrief(draft));
    CHECK(ReadText(folder / "captures" / "01-current.jpg").size() == jpeg.size());
    CHECK(ExistingRequestIds(repo, 1) == std::vector<std::string>{ID});

    CHECK_FALSE(WriteRequestFolder(repo, draft, jpeg, folder, error)); // exists already
    CHECK_FALSE(error.empty());

    RequestDraft other = draft;
    other.id = "2026-09-23-sign01-other";
    CHECK_FALSE(WriteRequestFolder(repo, other, {}, folder, error)); // capture without image
    CHECK_FALSE(fs::exists(RequestsDir(repo, 1) / other.id));
}

TEST_CASE("Names a new variant must avoid come from the catalog, the data and requests [editor][requests]")
{
    TempTree tree("mu-request-names");
    const fs::path repo = tree.Root();
    WriteText(repo / "src" / "bin" / "Data" / "Object1" / "Sign05.bmd", "bmd");
    WriteText(RequestsDir(repo, 1) / "2026-09-22-sign01-broken" / "request.json",
              R"({"change": {"new_model": "Sign04"}})");
    Catalog catalog;
    catalog.models.push_back(Sign01());

    const std::set<std::string> taken = TakenModelNames(repo, 1, catalog);
    CHECK(taken.contains("sign01"));
    CHECK(taken.contains("sign04"));
    CHECK(taken.contains("sign05"));
    CHECK(NextVariantName("Sign01", taken) == "Sign02");
}
