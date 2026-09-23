#include <doctest.h>

#include "TempTree.h"

#include "Assets/ItemCandidates.h"

#include <filesystem>
#include <string>
#include <vector>

using namespace Editor::Assets;
using EditorTest::TempTree;
using EditorTest::WriteText;
namespace fs = std::filesystem;

namespace
{
// The Small Axe as the catalog lists it: its model and texture, the texture also
// used by another axe, and an armour-like second model with its own texture.
ItemCatalogEntry SmallAxe()
{
    ItemCatalogEntry item;
    item.key = "1-0";
    item.group = 1;
    item.name = "Small Axe";
    ItemModel own;
    own.role = "item";
    own.bmd = "src/bin/Data/Item/Axe01.bmd";
    own.textures = {{"Axes02.jpg", "src/bin/Data/Item/Axes02.OZJ"}, {"hide.jpg", ""}};
    ItemModel variant;
    variant.role = "class-variant";
    variant.modelConstant = "MODEL_HELM_MONK + 1";
    variant.bmd = "src/bin/Data/Player/AxeMonk01.bmd";
    variant.textures = {{"monk.jpg", "src/bin/Data/Effect/monk.OZJ"}};
    item.models = {own, variant};
    item.sharedWith["src/bin/Data/Item/Axes02.OZJ"] = {"1-0", "1-3", "other:MODEL_AXE_SKILL"};
    return item;
}

void WriteFile(const fs::path& file, const std::string& bytes = "data")
{
    WriteText(file, bytes);
}
} // namespace

TEST_CASE("A/B candidates: deliveries, their files before, and the style pilot's variants")
{
    TempTree tree("mu_editor_item_candidates");
    const fs::path repo = tree.Root();
    const fs::path items = repo / "assets-work" / "Items";
    const ItemCatalogEntry axe = SmallAxe();
    WriteFile(items / "pilot" / "Axe01" / "B" / "Axe01.bmd");
    WriteFile(items / "pilot" / "Axe01" / "A" / "AXES02.ozj"); // any letter case
    WriteFile(items / "pilot" / "Axe01" / "review" / "A.png"); // no game file: not a candidate
    WriteFile(items / "pilot" / "Shield01" / "A" / "Shield01.bmd");
    const fs::path request = items / "requests" / "2026-09-23-1-0-sharper-axe";
    WriteFile(request / "delivery" / "1-0" / "exports" / "Axes02.OZJ");
    WriteFile(request / "delivery" / "1-0" / "original" / "Axes02.OZJ");
    WriteFile(items / "requests" / "2026-09-24-1-3-other" / "delivery" / "1-3" / "exports" / "Axes02.OZJ");

    const std::vector<ItemCandidate> found = FindItemCandidates(repo, axe);
    REQUIRE(found.size() == 4);
    CHECK(found[0].id == "delivery:2026-09-23-1-0-sharper-axe");
    CHECK(found[0].source == CandidateSource::Delivery);
    CHECK(found[0].folder == request / "delivery" / "1-0" / "exports");
    CHECK(found[0].requestId == "2026-09-23-1-0-sharper-axe");
    CHECK(found[1].id == "before:2026-09-23-1-0-sharper-axe");
    CHECK(found[1].source == CandidateSource::DeliveryBefore);
    CHECK(found[2].id == "pilot:Axe01/A");
    CHECK(found[2].label == "pilot A");
    CHECK(found[3].label == "pilot B");

    CHECK(FindItemCandidates(fs::path(), axe).empty());
    CHECK_FALSE(CandidateFromFolder(items / "pilot" / "Shield01" / "A", axe).has_value());
    const auto picked = CandidateFromFolder(items / "pilot" / "Axe01" / "B", axe);
    REQUIRE(picked.has_value());
    CHECK(picked->source == CandidateSource::Folder);
    CHECK(picked->label == "folder B");
}

TEST_CASE("A/B candidates: model and texture folders per variant")
{
    const ItemCatalogEntry axe = SmallAxe();
    const fs::path data = fs::path("game") / "Data";
    const auto files = ModelFilesIn(data, axe.models[1]);
    REQUIRE(files.has_value());
    CHECK(files->bmd == (data / "Player" / "AxeMonk01.bmd").make_preferred());
    // The folder of each catalog texture first, then the model's own folder.
    REQUIRE(files->textureFolders.size() == 2);
    CHECK(files->textureFolders[0] == data / "Effect");
    CHECK(files->textureFolders[1] == (data / "Player").make_preferred());

    ItemModel outside = axe.models[0];
    outside.bmd = "assets-work/Items/Axe01.bmd";
    CHECK_FALSE(ModelFilesIn(data, outside).has_value());

    TempTree tree("mu_editor_item_candidate_files");
    const fs::path candidate = tree.Root() / "B";
    WriteFile(candidate / "axe01.BMD");
    const auto withModel = CandidateModelFiles(data, axe.models[0], candidate);
    REQUIRE(withModel.has_value());
    CHECK(withModel->bmd == candidate / "axe01.BMD");
    REQUIRE(withModel->textureFolders.size() == 2);
    CHECK(withModel->textureFolders[0] == candidate);
    const auto texturesOnly = CandidateModelFiles(data, axe.models[1], candidate);
    REQUIRE(texturesOnly.has_value());
    CHECK(texturesOnly->bmd == (data / "Player" / "AxeMonk01.bmd").make_preferred());
}

TEST_CASE("A/B candidates: replaced textures and the items sharing them")
{
    TempTree tree("mu_editor_item_candidate_shared");
    const ItemCatalogEntry axe = SmallAxe();
    const fs::path candidate = tree.Root();
    CHECK(ReplacedTextures(axe, candidate).empty());
    WriteFile(candidate / "Axes02.OZJ");
    WriteFile(candidate / "Axe01.bmd");
    const std::vector<std::string> replaced = ReplacedTextures(axe, candidate);
    REQUIRE(replaced.size() == 1);
    CHECK(replaced[0] == "src/bin/Data/Item/Axes02.OZJ");
    const std::vector<std::string> partners = TexturePartners(axe, replaced);
    CHECK(partners == std::vector<std::string>{"1-3", "other:MODEL_AXE_SKILL"});
    CHECK(TexturePartners(axe, {"src/bin/Data/Effect/monk.OZJ"}).empty());
}

TEST_CASE("A/B candidates: a delivery already installed in the checkout")
{
    TempTree tree("mu_editor_item_candidate_installed");
    const ItemCatalogEntry axe = SmallAxe();
    const fs::path current = tree.Root() / "src" / "bin" / "Data";
    const fs::path delivery = tree.Root() / "exports";
    WriteFile(current / "Item" / "Axes02.OZJ", "new texture");
    WriteFile(current / "Item" / "Axe01.bmd", "model");
    CHECK_FALSE(CandidateMatchesCurrent(current, axe, delivery)); // holds nothing

    WriteFile(delivery / "Axes02.OZJ", "new texture");
    CHECK(CandidateMatchesCurrent(current, axe, delivery));
    WriteFile(delivery / "Axe01.bmd", "other model");
    CHECK_FALSE(CandidateMatchesCurrent(current, axe, delivery));
}

TEST_CASE("A/B candidates: the catalog's model constants")
{
    const auto monk = ParseModelConstant("MODEL_HELM_MONK + 1");
    REQUIRE(monk.has_value());
    CHECK(monk->first == "MODEL_HELM_MONK");
    CHECK(monk->second == 1);
    const auto mask = ParseModelConstant("MODEL_MASK_HELM");
    REQUIRE(mask.has_value());
    CHECK(mask->second == 0);
    CHECK_FALSE(ParseModelConstant("").has_value());
    CHECK_FALSE(ParseModelConstant("MODEL_").has_value());
    CHECK_FALSE(ParseModelConstant("Type == ITEM_ALE").has_value());
    CHECK_FALSE(ParseModelConstant("MODEL_EVENT + x").has_value());
}
