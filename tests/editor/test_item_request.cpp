#include <doctest.h>

#include "TempTree.h"

#include "Assets/CaptureImage.h"
#include "Assets/FileDigest.h"
#include "Assets/GitCheckout.h"
#include "Assets/ItemCatalog.h"
#include "Assets/ItemRequest.h"
#include "Assets/ItemRequestBrief.h"
#include "Assets/ItemRequestDecision.h"
#include "Assets/ItemRequestFolder.h"
#include "Assets/ItemRequestScan.h"
#include "Assets/ItemRenderFacts.h"
#include "Assets/RequestFolder.h"
#include "Editing/ItemCapturePlan.h"

#include <json.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace Editor::Assets;
using EditorTest::ReadText;
using EditorTest::TempTree;
using EditorTest::WriteText;
using nlohmann::json;

namespace
{
constexpr const char* BASE_COMMIT = "3d73f363f5755491d93a145fd85599c780f11d26";
constexpr const char* CREATED = "2026-09-23T10:15:00+02:00";
constexpr int CAPTURE_SIDE = 64;

// A sword whose blade texture is also used by another item and by a non-item model,
// and whose grip texture is its own.
ItemCatalogEntry Sword()
{
    ItemCatalogEntry item;
    item.key = "0-1";
    item.group = 0;
    item.index = 1;
    item.name = "Short Sword";
    item.family = "swords";
    item.tier.value = 1;
    item.classStages = {1, 1, 1, 1, 1, 1, 1};
    item.width = 1;
    item.height = 3;
    item.originalRevision = BASE_COMMIT;
    item.originalSha256 = std::string(64, 'a');
    ItemModel model;
    model.role = "item";
    model.bmd = "src/bin/Data/Item/Sword02.bmd";
    model.textures = {{"blade.jpg", "src/bin/Data/Item/blade.OZJ"},
                      {"grip.jpg", "src/bin/Data/Item/grip.OZJ"},
                      {"hide.jpg", ""}};
    model.originalSha256 = item.originalSha256;
    item.models.push_back(model);
    item.sharedWith = {{"src/bin/Data/Item/blade.OZJ", {"other:MODEL_SWORD", "12-3", "2-0"}}};
    return item;
}

ItemRequestDraft SwordDraft(ItemRequestKind kind)
{
    ItemRequestDraft draft;
    draft.id = "2026-09-23-0-1-sharper-blade";
    draft.created = CREATED;
    draft.baseCommit = BASE_COMMIT;
    draft.input.kind = kind;
    draft.input.summary = "Sharper blade";
    draft.input.details = {"Double the texture size"};
    draft.targets.push_back({Sword(), {std::string(64, 'a')}});
    return draft;
}

constexpr const char* WING_BLENDED_LINE =
    "Blended meshes of Wing01.bmd (mesh 0): the game draws them additively - paint on black (black is fully "
    "transparent, brightness becomes glow); no opaque background, no baked dark outlines";

// render-facts.json text for the Wings of Elf and a sword without an entry's must_keep line.
constexpr const char* RENDER_FACTS_TEXT = R"json({
 "schema": "mu-item-render-facts/1",
 "items": {
  "12-0": {
   "key": "12-0", "status": "verified-in-client", "summary": "Additive wings.",
   "drawn": "blended meshes 0 / effects: no level glow",
   "models": [{"role": "item", "bmd": "src/bin/Data/Item/Wing01.bmd", "meshes": [
    {"mesh": 0, "texture": "elfin_wing.jpg", "flags": [], "worn": "blended-additive",
     "dropped": "blended-additive", "inventory": "blended-additive", "evidence": ["rule:mesh-blend"]}]}],
   "request": {"effects": ["No +level glow: the engine draws it as +0 whatever its level"],
               "must_keep": ["Blended meshes of Wing01.bmd (mesh 0): the game draws them additively - paint on black (black is fully transparent, brightness becomes glow); no opaque background, no baked dark outlines"]}
  },
  "0-1": {
   "key": "0-1", "status": "from-code", "summary": "", "drawn": "opaque / effects: trail",
   "models": [{"role": "item", "bmd": "src/bin/Data/Item/Sword02.bmd", "meshes": [
    {"mesh": 0, "texture": "blade.jpg", "worn": "opaque", "dropped": null, "inventory": "opaque"}]}],
   "request": {"effects": ["A swing trail"], "must_keep": []}
  }
 }
})json";

ItemRenderFacts ParsedRenderFacts()
{
    ItemRenderFacts facts;
    std::string error;
    REQUIRE_MESSAGE(ParseItemRenderFacts(RENDER_FACTS_TEXT, facts, error), error);
    return facts;
}

ItemCatalogEntry ElfWings()
{
    ItemCatalogEntry item;
    item.key = "12-0";
    item.group = 12;
    item.index = 0;
    item.name = "Wings of Elf";
    item.family = "wings-1";
    item.tier.value = 4;
    item.classStages = {0, 0, 1, 0, 0, 0, 0};
    item.width = 3;
    item.height = 2;
    ItemModel model;
    model.role = "item";
    model.bmd = "src/bin/Data/Item/Wing01.bmd";
    model.meshes = 1;
    model.textures = {{"elfin_wing.jpg", "src/bin/Data/Item/elfin_wing.OZJ"}};
    item.models.push_back(model);
    return item;
}

std::vector<std::uint8_t> TinyJpeg(int side)
{
    mu::FramePixels frame;
    frame.width = static_cast<std::uint32_t>(side);
    frame.height = static_cast<std::uint32_t>(side);
    frame.rgb.assign(static_cast<std::size_t>(side) * side * 3, 0x80);
    return Editor::Capture::EncodeJpeg(frame, Editor::Capture::CAPTURE_JPEG_QUALITY);
}

ItemCaptureInfo Capture(const std::string& fileName, const std::string& view)
{
    ItemCaptureInfo capture;
    capture.fileName = fileName;
    capture.view = view;
    capture.width = CAPTURE_SIDE;
    capture.height = CAPTURE_SIDE;
    capture.clientCommit = "3d73f363";
    return capture;
}
} // namespace

TEST_CASE("Item scope: shared textures are frozen, the kind decides about the models [editor][item-requests]")
{
    const ItemRequestDraft draft = SwordDraft(ItemRequestKind::Upscale);
    const ItemRequestScope upscale = ComputeItemScope(ItemRequestKind::Upscale, draft.targets);
    REQUIRE(upscale.sharedTextures.size() == 1);
    CHECK(upscale.sharedTextures[0].first == "src/bin/Data/Item/blade.OZJ");
    // Item keys by group and index, then the non-item models.
    CHECK(upscale.sharedTextures[0].second == std::vector<std::string>{"0-1", "2-0", "12-3", "other:MODEL_SWORD"});
    CHECK(upscale.frozenTextures == std::vector<std::string>{"src/bin/Data/Item/blade.OZJ"});
    CHECK(upscale.ownedFiles == std::vector<std::string>{"src/bin/Data/Item/grip.OZJ"});

    const ItemRequestScope redesign = ComputeItemScope(ItemRequestKind::Redesign, draft.targets);
    CHECK(redesign.ownedFiles ==
          std::vector<std::string>{"src/bin/Data/Item/Sword02.bmd", "src/bin/Data/Item/grip.OZJ"});

    ItemRequestTarget outside = draft.targets.front();
    outside.item.models[0].textures = {{"cloth.jpg", "src/bin/Data/Player/cloth.OZJ"},
                                       {"sky.jpg", "src/bin/Data/Object1/sky.OZJ"}};
    outside.item.sharedWith.clear();
    const ItemRequestScope own = ComputeItemScope(ItemRequestKind::Repaint, {outside});
    CHECK(own.frozenTextures == std::vector<std::string>{"src/bin/Data/Object1/sky.OZJ"});
    CHECK(own.ownedFiles == std::vector<std::string>{"src/bin/Data/Player/cloth.OZJ"});
}

TEST_CASE("Item must_keep has the common lines and the kind's [editor][item-requests]")
{
    ItemOwnerInput input;
    input.kind = ItemRequestKind::Remodel;
    const std::vector<std::string> remodel = ItemMustKeep(input);
    REQUIRE(remodel.size() == 8);
    CHECK(remodel.front() == "File names and paths: no renamed, added or deleted game files");
    CHECK(remodel.back() == "Remodel: the silhouette and the function; origin, orientation, scale and mesh order");

    input.kind = ItemRequestKind::Set;
    input.setKind = ItemRequestKind::Repaint;
    const std::vector<std::string> set = ItemMustKeep(input);
    REQUIRE(set.size() == 9);
    CHECK(set[7] == "Repaint: the mesh and the UV layout");
    CHECK(set[8] == "Set: every part keeps its skeleton and actions, and the parts still read as one set");
    CHECK(PartKind(input) == ItemRequestKind::Repaint);
    CHECK(ParseItemKind("redesign") == ItemRequestKind::Redesign);
    CHECK_FALSE(ParseItemKind("repaint+remodel").has_value());
}

TEST_CASE("Item request.json follows the item contract for an open request [editor][item-requests]")
{
    ItemRequestDraft draft = SwordDraft(ItemRequestKind::Upscale);
    draft.input.pushAllowed = true;
    draft.captures.push_back(Capture("01-front.jpg", "turntable"));
    draft.captures.front().angle = 0.0f;
    draft.referenceImages = {"ref-concept.jpg"};
    const json request = json::parse(BuildItemRequestJson(draft));

    CHECK(request["schema"] == "mu-item-regen-request/1");
    CHECK(request["author"] == "owner (item editor)");
    CHECK_FALSE(request.contains("world"));
    CHECK(request["status_history"][0]["by"] == "item editor");
    CHECK(request["kind"] == "upscale");
    CHECK(request["supersedes"].is_null());
    const json& target = request["targets"][0];
    CHECK(target["key"] == "0-1");
    CHECK(target["classes"]["rf"] == 1);
    CHECK(target["size"] == json::array({1, 3}));
    CHECK(target["armour_set"].is_null());
    CHECK(target["models"][0]["textures"]["hide.jpg"].is_null());
    CHECK_FALSE(target["models"][0].contains("class"));
    CHECK(request["change"]["reference_images"][0] ==
          "assets-work/Items/requests/2026-09-23-0-1-sharper-blade/captures/ref-concept.jpg");
    CHECK_FALSE(request["change"].contains("set_kind"));
    CHECK(request["constraints"]["limits"] == json({{"max_texture_size", 1024}, {"max_triangles", 1500}}));
    CHECK(request["constraints"]["protected_paths"][6] == "assets-work/Items/catalog.json");
    const json& capture = request["evidence"]["captures"][0];
    CHECK(capture["file"] == "assets-work/Items/requests/2026-09-23-0-1-sharper-blade/captures/01-front.jpg");
    CHECK(capture["angle"] == 0.0);
    CHECK(capture["resolution"] == json::array({CAPTURE_SIDE, CAPTURE_SIDE}));
    CHECK(request["handoff"]["branch"] == "codex/item-req-0-1-sharper-blade");
    CHECK(request["handoff"]["worktree"] == "../MuMain-item-req-0-1-sharper-blade");
    CHECK(request["handoff"]["deliver_to"] == "assets-work/Items/requests/2026-09-23-0-1-sharper-blade/delivery/");
    CHECK(request["handoff"]["push_allowed"] == true);

    draft.input.kind = ItemRequestKind::Set;
    draft.supersedes = "2026-09-20-0-1-first-try";
    const json set = json::parse(BuildItemRequestJson(draft));
    CHECK(set["change"]["set_kind"] == "upscale");
    CHECK(set["supersedes"] == "2026-09-20-0-1-first-try");
}

TEST_CASE("render-facts.json gives each item its mesh modes, effects and must_keep lines [editor][item-requests]")
{
    const ItemRenderFacts facts = ParsedRenderFacts();
    const ItemRenderEntry* wings = facts.Find("12-0");
    REQUIRE(wings != nullptr);
    CHECK(wings->status == "verified-in-client");
    CHECK(wings->drawn == "blended meshes 0 / effects: no level glow");
    REQUIRE(wings->models.size() == 1);
    CHECK(wings->models[0].meshes[0].worn == "blended-additive");
    CHECK(wings->mustKeep == std::vector<std::string>{WING_BLENDED_LINE});
    const ItemRenderEntry* sword = facts.Find("0-1");
    REQUIRE(sword != nullptr);
    CHECK(sword->models[0].meshes[0].dropped.empty()); // null: not used there
    CHECK(facts.Find("99-0") == nullptr);

    ItemRenderFacts other;
    std::string error;
    CHECK_FALSE(ParseItemRenderFacts(R"({"schema": "mu-item-catalog/1", "items": {}})", other, error));
    CHECK(error.find("mu-item-render-facts/1") != std::string::npos);
}

TEST_CASE("A blended wing's request carries the render block and the blended line (golden) [editor][item-requests]")
{
    const ItemRenderFacts facts = ParsedRenderFacts();
    ItemRequestDraft draft;
    draft.id = "2026-09-24-12-0-paint-on-black";
    draft.created = CREATED;
    draft.baseCommit = BASE_COMMIT;
    draft.input.kind = ItemRequestKind::Repaint;
    draft.input.summary = "Repaint the elf wings for additive blending";
    draft.targets.push_back({ElfWings(), {std::string(64, 'b')}, *facts.Find("12-0")});
    const json constraints = json::parse(BuildItemRequestJson(draft))["constraints"];

    const json golden = json::parse(R"json({
      "must_keep": [
        "File names and paths: no renamed, added or deleted game files",
        "Origin, orientation and scale: hands, back and shields attach through the model origin (RenderLinkObject angles are hard-coded)",
        "Mesh count and mesh/material order (the engine hides and blends meshes by index)",
        "Texture name suffixes _R, _S, _H, _N (they set render flags); no new ones",
        "At most 1500 triangles per model; power-of-two textures up to 1024 px, .jpg opaque, 32-bit .tga for alpha",
        "Armour and wings: the skeleton (bone count, order, names, parents) and every action with its key count",
        "Size in the inventory (Width x Height of the item table) and a footprint that fits it",
        "Repaint: the mesh and the UV layout",
        "Blended meshes of Wing01.bmd (mesh 0): the game draws them additively - paint on black (black is fully transparent, brightness becomes glow); no opaque background, no baked dark outlines"
      ],
      "render": {
        "12-0": {
          "models": [
            {
              "role": "item",
              "bmd": "src/bin/Data/Item/Wing01.bmd",
              "meshes": [
                { "mesh": 0, "texture": "elfin_wing.jpg", "worn": "blended-additive",
                  "dropped": "blended-additive", "inventory": "blended-additive" }
              ]
            }
          ],
          "effects": ["No +level glow: the engine draws it as +0 whatever its level"]
        }
      }
    })json");
    CHECK(constraints["must_keep"] == golden["must_keep"]);
    CHECK(constraints["render"] == golden["render"]);
    // A target without render facts still gets its (empty) block, and no render line.
    ItemRequestDraft sword = SwordDraft(ItemRequestKind::Upscale);
    const json plain = json::parse(BuildItemRequestJson(sword))["constraints"];
    CHECK(plain["render"]["0-1"] == json({{"models", json::array()}, {"effects", json::array()}}));
    CHECK(plain["must_keep"].size() == 8);

    const std::string brief = BuildItemBrief(draft);
    CHECK(brief.find("## How the game draws this item") != std::string::npos);
    const std::string row = "| `Wing01.bmd` | 0 | `elfin_wing.jpg` | blended-additive | blended-additive | "
                            "blended-additive |";
    CHECK(brief.find(row) != std::string::npos);
    CHECK(brief.find("- No +level glow: the engine draws it as +0 whatever its level") != std::string::npos);
    CHECK(brief.find(std::string("- ") + WING_BLENDED_LINE) != std::string::npos);
    CHECK(brief.find("Additive wings.") != std::string::npos);
}

TEST_CASE("Item brief.md names the scope, the notes and every capture [editor][item-requests]")
{
    ItemRequestDraft draft = SwordDraft(ItemRequestKind::Upscale);
    draft.captures.push_back(Capture("01-front.jpg", "turntable"));
    draft.referenceImages = {"ref-01.jpg"};
    const std::string brief = BuildItemBrief(draft);
    CHECK(brief.find("# Item request 2026-09-23-0-1-sharper-blade") == 0);
    CHECK(brief.find("`src/bin/Data/Item/grip.OZJ` (you may replace it)") != std::string::npos);
    CHECK(brief.find("`src/bin/Data/Item/blade.OZJ` (frozen)") != std::string::npos);
    CHECK(brief.find("![01-front.jpg](captures/01-front.jpg)") != std::string::npos);
    CHECK(brief.find("![ref-01.jpg](captures/ref-01.jpg)") != std::string::npos);
    CHECK(brief.find("[ASTRA.md](../../../../ASTRA.md)") != std::string::npos);
}

TEST_CASE("An item request folder holds the captures, references, brief and request [editor][item-requests]")
{
    TempTree tree("mu_item_request_folder");
    ItemRequestDraft draft = SwordDraft(ItemRequestKind::Upscale);
    draft.captures.push_back(Capture("01-front.jpg", "turntable"));
    draft.referenceImages = {"ref-concept.jpg"};
    ItemRequestImages images;
    images.captures.push_back(TinyJpeg(CAPTURE_SIDE));
    images.references.push_back(TinyJpeg(CAPTURE_SIDE));

    fs::path folder;
    std::string error;
    REQUIRE(WriteItemRequestFolder(tree.Root(), draft, images, folder, error));
    CHECK(folder == tree.Root() / "assets-work" / "Items" / "requests" / draft.id);
    CHECK(fs::exists(folder / "captures" / "01-front.jpg"));
    CHECK(fs::exists(folder / "captures" / "ref-concept.jpg"));
    CHECK(ReadText(folder / "request.json") == BuildItemRequestJson(draft));
    CHECK(ReadText(folder / "brief.md") == BuildItemBrief(draft));

    CHECK_FALSE(WriteItemRequestFolder(tree.Root(), draft, images, folder, error));
    CHECK(error.find("exists already") != std::string::npos);
    draft.id = "2026-09-23-0-1-other";
    images.captures.clear();
    CHECK_FALSE(WriteItemRequestFolder(tree.Root(), draft, images, folder, error));
    CHECK_FALSE(fs::exists(folder));
}

TEST_CASE("A reference image is kept as it is, or scaled down when too wide [editor][item-requests]")
{
    TempTree tree("mu_item_reference_image");
    const std::vector<std::uint8_t> small = TinyJpeg(CAPTURE_SIDE);
    WriteText(tree.Root() / "small.jpg", std::string(small.begin(), small.end()));
    std::vector<std::uint8_t> jpeg;
    std::string error;
    REQUIRE(ReadReferenceJpeg(tree.Root() / "small.jpg", CAPTURE_SIDE, jpeg, error));
    CHECK(jpeg == small);

    REQUIRE(ReadReferenceJpeg(tree.Root() / "small.jpg", CAPTURE_SIDE / 2, jpeg, error));
    mu::FramePixels decoded;
    REQUIRE(Editor::Capture::DecodeJpeg(jpeg, decoded));
    CHECK(decoded.width == CAPTURE_SIDE / 2);

    WriteText(tree.Root() / "picture.png", "\x89PNG not a jpeg");
    CHECK_FALSE(ReadReferenceJpeg(tree.Root() / "picture.png", CAPTURE_SIDE, jpeg, error));
    CHECK(error.find("not a JPEG") != std::string::npos);
}

TEST_CASE("The requests scan reads status, verdict and delivery of every folder [editor][item-requests]")
{
    TempTree tree("mu_item_request_scan");
    ItemRequestDraft draft = SwordDraft(ItemRequestKind::Redesign);
    draft.input.keep = {"Blade length"};
    fs::path folder;
    std::string error;
    REQUIRE(WriteItemRequestFolder(tree.Root(), draft, {}, folder, error));
    const fs::path requests = folder.parent_path();
    WriteText(requests / "2026-09-24-6-0-broken" / "request.json", "{ not json");
    fs::create_directories(requests / "not-a-request");

    const std::string before = RequestsFingerprint(requests);
    std::vector<ItemRequestSummary> scanned = ScanItemRequests(requests);
    REQUIRE(scanned.size() == 2);
    const ItemRequestSummary& sword = scanned[0];
    CHECK(sword.id == draft.id);
    CHECK(sword.status == "open");
    CHECK(sword.kind == "redesign");
    CHECK(sword.branch == "codex/item-req-0-1-sharper-blade");
    CHECK(sword.targetKeys == std::vector<std::string>{"0-1"});
    CHECK(sword.keep == std::vector<std::string>{"Blade length"});
    CHECK_FALSE(sword.deliveryPresent);
    CHECK_FALSE(sword.ownerDecision.has_value());
    CHECK(scanned[1].problem == "request.json is not a JSON object");
    CHECK(IsLiveRequestStatus(sword.status));
    CHECK(RequestsByItem(scanned).at("0-1").front().status == "open");

    fs::create_directories(folder / "delivery" / "0-1");
    WriteText(folder / "owner-decision.json", R"({"verdict": "reject", "notes": "too shiny", "date": "2026-09-24"})");
    CHECK(RequestsFingerprint(requests) != before);
    scanned = ScanItemRequests(requests);
    CHECK(scanned[0].deliveryPresent);
    REQUIRE(scanned[0].ownerDecision.has_value());
    CHECK(scanned[0].ownerDecision->notes == "too shiny");
}

TEST_CASE("Withdrawing changes only status, decision and one history entry [editor][item-requests]")
{
    const std::string filed = BuildItemRequestJson(SwordDraft(ItemRequestKind::Upscale));
    std::string text;
    std::string error;
    REQUIRE(WithdrawnRequestText(filed, "2026-09-24T09:00:00+02:00", "wrong item", text, error));
    json withdrawn = json::parse(text);
    CHECK(withdrawn["status"] == "withdrawn");
    CHECK(withdrawn["decision"] == json({{"status", "withdrawn"},
                                         {"at", "2026-09-24T09:00:00+02:00"},
                                         {"by", "owner (item editor)"},
                                         {"reason", "wrong item"},
                                         {"ledger_entry", nullptr}}));
    REQUIRE(withdrawn["status_history"].size() == 2);
    CHECK(withdrawn["status_history"][1]["note"] == "wrong item");

    json original = json::parse(filed);
    for (const char* key : {"status", "decision", "status_history"})
    {
        withdrawn.erase(key);
        original.erase(key);
    }
    CHECK(withdrawn == original);
    CHECK_FALSE(WithdrawnRequestText(text, "2026-09-24T10:00:00+02:00", "", text, error));
    CHECK(error.find("withdrawn request cannot be withdrawn") != std::string::npos);
}

TEST_CASE("Accept and reject write owner-decision.json for a delivered request only [editor][item-requests]")
{
    TempTree tree("mu_item_owner_decision");
    WriteText(tree.Root() / "request.json", R"({"status": "open"})");
    std::string error;
    const OwnerDecision reject{OWNER_VERDICT_REJECT, "grip too thin", "2026-09-24"};
    CHECK_FALSE(WriteOwnerDecision(tree.Root(), reject, error));
    CHECK_FALSE(fs::exists(tree.Root() / "owner-decision.json"));

    WriteText(tree.Root() / "request.json", R"({"status": "delivered"})");
    REQUIRE(WriteOwnerDecision(tree.Root(), reject, error));
    const json decision = json::parse(ReadText(tree.Root() / "owner-decision.json"));
    CHECK(decision == json({{"verdict", "reject"}, {"notes", "grip too thin"}, {"date", "2026-09-24"}}));
    CHECK(ReadText(tree.Root() / "request.json") == R"({"status": "delivered"})");
    CHECK_FALSE(WriteOwnerDecision(tree.Root(), {"maybe", "", "2026-09-24"}, error));
}

TEST_CASE("The capture plan: turntable, inventory, worn and +level glow [editor][item-requests]")
{
    using namespace Editor::Preview;
    const std::vector<CaptureShot> excellent = PlanItemCaptures(true, FaceYawDegrees(8));
    REQUIRE(excellent.size() == 9);
    CHECK(excellent[0].slug == "front");
    CHECK(excellent[0].angle == 0.0f);
    CHECK(excellent[0].yawDegrees == 270.0f); // armour faces -Y like the character
    CHECK(excellent[1].angle == 90.0f);
    CHECK(excellent[1].yawDegrees == 0.0f);
    CHECK(excellent[2].angle == 180.0f);
    CHECK(excellent[3].angle == 45.0f);
    CHECK(excellent[3].yawDegrees == 315.0f);
    CHECK(excellent[4].view == CaptureView::Inventory);
    CHECK(excellent[5].requestView == "equipped-front");
    CHECK(excellent[5].onlyWhenWorn);
    CHECK(excellent[6].slug == "glow-0-exc");
    CHECK(excellent[8].level == 13);
    CHECK(excellent[8].excellent);

    // Swords and shields show their broad face from +X: that is their front.
    CHECK(FaceYawDegrees(0) == 0.0f);
    CHECK(FaceYawDegrees(6) == 0.0f);
    CHECK(FaceYawDegrees(12) == 270.0f);
    const std::vector<CaptureShot> plain = PlanItemCaptures(false, FaceYawDegrees(6));
    REQUIRE(plain.size() == 8);
    CHECK(plain[0].yawDegrees == 0.0f);
    CHECK(plain[2].yawDegrees == 180.0f);
    CHECK(plain[2].angle == 180.0f);
    CHECK(plain[3].yawDegrees == 45.0f);
    CHECK(plain[6].slug == "glow-9");
    CHECK_FALSE(plain[7].excellent);
    CHECK(CaptureFileName(7, "glow-9") == "07-glow-9.jpg");
}

TEST_CASE("The item catalog gives armour sets, shared textures and keys [editor][item-requests]")
{
    constexpr const char* CATALOG = R"({"schema": "mu-item-catalog/1", "items": {
      "7-1": {"key": "7-1", "group": 7, "index": 1, "name": "Dragon Helm", "family": "helms",
              "armour_set": {"index": 1, "name": "Dragon", "parts": ["7-1", "8-1"]},
              "shared_with": {"src/bin/Data/Player/a.OZJ": ["8-1", "other:MODEL_BODY"]}}}})";
    ItemCatalog catalog;
    std::string error;
    REQUIRE(ParseItemCatalog(CATALOG, catalog, error));
    const ItemCatalogEntry* helm = catalog.FindByKey("7-1");
    REQUIRE(helm != nullptr);
    CHECK(helm->armourSet == 1);
    CHECK(helm->armourSetParts == std::vector<std::string>{"7-1", "8-1"});
    CHECK(helm->sharedWith.at("src/bin/Data/Player/a.OZJ") == std::vector<std::string>{"8-1", "other:MODEL_BODY"});
    CHECK(catalog.FindByKey("8-1") == nullptr);
    CHECK(catalog.FindByKey("junk") == nullptr);
    CHECK(IsArmourGroup(helm->group));
}

#if defined(MU_REPO_ROOT) && !defined(_WIN32)
#include <sys/wait.h> // WEXITSTATUS

namespace
{
// Validates `folders` with the repository's validate_request.py against a scratch
// tree `root` (its src/ links to the checkout); prints the report, returns the exit code.
constexpr const char* VALIDATE_SCRIPT = R"PY(
import sys
from pathlib import Path
root, repo = Path(sys.argv[1]), Path(sys.argv[2])
sys.dont_write_bytecode = True
sys.path.insert(0, str(repo / 'assets-work' / 'Items' / 'requests'))
import validate_request as validator
reference = validator.load_reference(root, root / 'assets-work' / 'Items', git_root=repo)
failed = 0
for folder in sys.argv[3:]:
    report = validator.validate(Path(folder), reference)
    for message in report.errors:
        print(Path(folder).name + ': ' + message)
    failed += bool(report.errors)
sys.exit(1 if failed else 0)
)PY";

bool HasPython()
{
    return std::system("python3 -c 'import sys' >/dev/null 2>&1") == 0;
}

int RunValidator(const fs::path& root, const std::vector<fs::path>& folders, std::string& report)
{
    std::string command = "python3 -c \"$MU_VALIDATE_SCRIPT\" '" + root.string() + "' '" + MU_REPO_ROOT + "'";
    for (const fs::path& folder : folders)
        command += " '" + folder.string() + "'";
    setenv("MU_VALIDATE_SCRIPT", VALIDATE_SCRIPT, 1);
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr)
        return -1;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        report += buffer;
    const int status = pclose(pipe);
    return WEXITSTATUS(status);
}

ItemRequestTarget RepoTarget(const ItemCatalogEntry& item, const ItemRenderFacts& facts)
{
    ItemRequestTarget target{item, {}, std::nullopt};
    for (const ItemModel& model : item.models)
        target.modelSha256.push_back(Editor::Files::Sha256Hex(fs::path(MU_REPO_ROOT) / model.bmd));
    const ItemRenderEntry* render = facts.Find(item.key);
    if (render != nullptr)
        target.render = *render;
    return target;
}

// A scratch tree the validator accepts as the repository: src/ links to the
// checkout, assets-work/Items holds copies of the catalog and the owner files.
void PrepareScratchRepository(const fs::path& root)
{
    const fs::path items = root / "assets-work" / "Items";
    fs::create_directories(items);
    fs::create_directory_symlink(fs::path(MU_REPO_ROOT) / "src", root / "src");
    for (const char* name : {"catalog.json", "tiers.json", "assignments.json", "render-facts.json"})
        fs::copy_file(fs::path(MU_REPO_ROOT) / "assets-work" / "Items" / name, items / name);
}

fs::path WriteRepoRequest(const fs::path& root, ItemRequestDraft draft, bool withConcept)
{
    ItemRequestImages images;
    const std::vector<std::string> views = {"turntable", "inventory", "glow"};
    for (std::size_t i = 0; i < views.size(); ++i)
    {
        const std::string fileName = Editor::Preview::CaptureFileName(static_cast<int>(i) + 1, views[i]);
        draft.captures.push_back(Capture(fileName, views[i]));
        images.captures.push_back(TinyJpeg(CAPTURE_SIDE));
    }
    if (withConcept)
    {
        draft.referenceImages.push_back("ref-concept.jpg");
        images.references.push_back(TinyJpeg(CAPTURE_SIDE));
    }
    fs::path folder;
    std::string error;
    REQUIRE_MESSAGE(WriteItemRequestFolder(root, draft, images, folder, error), error);
    return folder;
}
} // namespace

TEST_CASE("validate_request.py accepts the requests the editor writes [editor][item-requests]")
{
    const Editor::Git::HeadInfo head = Editor::Git::ReadHead(MU_REPO_ROOT);
    const ItemCatalogLoad load = LoadItemCatalog(MU_REPO_ROOT);
    const ItemRenderFactsLoad renderLoad = LoadItemRenderFacts(MU_REPO_ROOT);
    if (!HasPython() || head.commit.empty() || !load.catalog || !renderLoad.facts)
    {
        MESSAGE("python3, the checkout's HEAD, catalog.json or render-facts.json is missing; the validator run is "
                "skipped");
        return;
    }
    TempTree tree("mu_item_request_validator");
    PrepareScratchRepository(tree.Root());
    const ItemCatalog& catalog = *load.catalog;

    std::vector<fs::path> folders;
    const auto add = [&](const std::vector<std::string>& keys, ItemRequestKind kind, ItemRequestKind setKind,
                         const std::string& slug, bool withConcept)
    {
        ItemRequestDraft draft;
        draft.id = "2026-09-23-" + keys.front() + "-" + slug;
        draft.created = CREATED;
        draft.baseCommit = head.commit;
        draft.input.kind = kind;
        draft.input.setKind = setKind;
        draft.input.summary = "Test request " + slug;
        for (const std::string& key : keys)
        {
            const ItemCatalogEntry* item = catalog.FindByKey(key);
            REQUIRE_MESSAGE(item != nullptr, key);
            draft.targets.push_back(RepoTarget(*item, *renderLoad.facts));
        }
        folders.push_back(WriteRepoRequest(tree.Root(), draft, withConcept));
    };
    add({"0-1"}, ItemRequestKind::Upscale, ItemRequestKind::Upscale, "upscale", false);
    add({"6-0"}, ItemRequestKind::Redesign, ItemRequestKind::Upscale, "redesign", true);
    // Blended and cut-out meshes: the render lines of must_keep.
    add({"12-0"}, ItemRequestKind::Repaint, ItemRequestKind::Upscale, "additive", false);
    add({"12-36"}, ItemRequestKind::Remodel, ItemRequestKind::Upscale, "storm", false);
    const ItemCatalogEntry* helm = catalog.FindByKey("7-1");
    REQUIRE(helm != nullptr);
    add(helm->armourSetParts, ItemRequestKind::Set, ItemRequestKind::Remodel, "set", false);
    // Items whose textures other items or models share: the frozen and shared lists.
    int shared = 0;
    for (const ItemCatalogEntry& item : catalog.items)
    {
        if (item.sharedWith.empty() || item.models.empty() || shared == 3)
            continue;
        add({item.key}, ItemRequestKind::Repaint, ItemRequestKind::Upscale, "shared", false);
        ++shared;
    }
    // A withdrawn request stays valid.
    std::string error;
    REQUIRE(WithdrawRequest(folders.front(), "2026-09-24T09:00:00+02:00", "test", error));

    std::string report;
    CHECK_MESSAGE(RunValidator(tree.Root(), folders, report) == 0, report);
}
#endif
