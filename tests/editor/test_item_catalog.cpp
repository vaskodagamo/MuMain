#include <doctest.h>

#include "TempTree.h"

#include "Assets/ItemCatalog.h"

#include <string>

using namespace Editor::Assets;
using EditorTest::TempTree;
using EditorTest::WriteText;

namespace
{
// Two items in catalog.json's shape: a sword with one model and a request, and a
// model without a table row (no name, zero table facts).
constexpr const char* SMALL_ITEM_CATALOG = R"({
 "schema": "mu-item-catalog/1",
 "items": {
  "7-53": {"key": "7-53", "group": 7, "index": 53, "name": null, "in_table": false, "family": "helms",
           "table": {"classes": {"dw": 0, "dk": 0, "elf": 0, "mg": 0, "dl": 0, "sum": 0, "rf": 0}},
           "tier": {"value": 1, "family_rank": 61, "family_size": 61, "drops_from_monsters": true},
           "models": [], "requests": [], "client_review": null},
  "0-19": {"key": "0-19", "group": 0, "index": 19, "name": "Divine Sword of Archangel", "in_table": true,
           "family": "swords",
           "table": {"classes": {"dk": 1, "dl": 1, "dw": 0, "elf": 0, "mg": 1, "rf": 0, "sum": 0},
                     "drop_level": 86, "require_level": 0, "width": 1, "height": 4, "two_hand": false},
           "tier": {"value": 5, "score": 86, "score_source": "openmu-drop-level", "drops_from_monsters": false,
                    "family_rank": 33, "family_size": 34, "source": "computed", "note": null},
           "badges": {"excellent": true, "max_item_level": 15, "option380": false, "set": false, "socket": false},
           "models": [{"role": "item", "bmd": "src/bin/Data/Item/Sword20.bmd", "exists": true,
                       "structure": {"meshes": 2, "triangles": 252},
                       "textures": {"sword44_R.jpg": "src/bin/Data/Item/sword44_R.OZJ", "hide.jpg": null},
                       "original": {"revision": "3d73f36", "sha256": "3458"}},
                      {"role": "class-variant", "class": "dl", "bmd": "src/bin/Data/Player/X.bmd"}],
           "original": {"revision": "3d73f36", "sha256": "3458"},
           "requests": [{"id": "2026-09-23-0-19-glow", "status": "claimed", "assigned_to": "codex-1"}],
           "client_review": {"verdict": "needs-work", "note": "dull", "date": "2026-09-23"}}
 }
})";
} // namespace

TEST_CASE("ParseItemCatalog reads items, sorted by type [editor][items]")
{
    ItemCatalog catalog;
    std::string error;
    REQUIRE(ParseItemCatalog(SMALL_ITEM_CATALOG, catalog, error));
    REQUIRE(catalog.items.size() == 2);
    CHECK(catalog.items[0].key == "0-19");
    CHECK(catalog.items[1].key == "7-53");
    CHECK(catalog.familyCounts == std::map<std::string, int>{{"helms", 1}, {"swords", 1}});

    const ItemCatalogEntry* sword = catalog.FindByType(19);
    REQUIRE(sword != nullptr);
    CHECK(sword->name == "Divine Sword of Archangel");
    CHECK(sword->classStages == ItemClassStages{0, 1, 0, 1, 1, 0, 0});
    CHECK(sword->dropLevel == 86);
    CHECK(sword->height == 4);
    CHECK(sword->tier.value == 5);
    CHECK_FALSE(sword->tier.dropsFromMonsters);
    CHECK(sword->tier.familyRank == 33);
    CHECK(sword->tier.scoreSource == "openmu-drop-level");
    CHECK(sword->badges.excellent);
    CHECK(sword->badges.maxItemLevel == 15);
    REQUIRE(sword->models.size() == 2);
    CHECK(sword->models[0].bmd == "src/bin/Data/Item/Sword20.bmd");
    CHECK(sword->models[0].triangles == 252);
    REQUIRE(sword->models[0].textures.size() == 2);
    CHECK(sword->models[0].textures[0].name == "hide.jpg");
    CHECK(sword->models[0].textures[0].container.empty());
    CHECK(sword->models[0].originalSha256 == "3458");
    CHECK(sword->models[1].className == "dl");
    CHECK(sword->originalRevision == "3d73f36");
    REQUIRE(sword->requests.size() == 1);
    CHECK(sword->requests[0].assignedTo == "codex-1");
    REQUIRE(sword->clientReview.has_value());
    CHECK(sword->clientReview->verdict == "needs-work");

    const ItemCatalogEntry* helm = catalog.FindByType(7 * ITEMS_PER_GROUP + 53);
    REQUIRE(helm != nullptr);
    CHECK(helm->name.empty());
    CHECK_FALSE(helm->inTable);
    CHECK(catalog.FindByType(1) == nullptr);
}

TEST_CASE("ParseItemCatalog refuses other files [editor][items]")
{
    ItemCatalog catalog;
    std::string error;
    CHECK_FALSE(ParseItemCatalog("not json", catalog, error));
    CHECK(error.find("not valid JSON") != std::string::npos);
    CHECK_FALSE(ParseItemCatalog(R"({"schema": "mu-world-catalog/1", "models": {}})", catalog, error));
    CHECK(error.find("mu-item-catalog/1") != std::string::npos);
}

TEST_CASE("LoadItemCatalog says why there is no catalog [editor][items]")
{
    TempTree tree("mu_item_catalog_test");
    ItemCatalogLoad load = LoadItemCatalog(tree.Root());
    CHECK_FALSE(load.fileFound);
    CHECK_FALSE(load.catalog.has_value());
    CHECK(load.error.find("no item catalog") != std::string::npos);

    WriteText(ItemCatalogFile(tree.Root()), SMALL_ITEM_CATALOG);
    load = LoadItemCatalog(tree.Root());
    CHECK(load.fileFound);
    REQUIRE(load.catalog.has_value());
    CHECK(load.catalog->items.size() == 2);
}

TEST_CASE("only open, claimed and delivered requests are pending [editor][items]")
{
    for (const char* status : {"open", "claimed", "delivered"})
        CHECK(IsRequestPending({"id", status, ""}));
    for (const char* status : {"accepted", "rejected", "withdrawn"})
        CHECK_FALSE(IsRequestPending({"id", status, ""}));
}

#ifdef MU_REPO_ROOT
TEST_CASE("the repository's item catalog loads [editor][items]")
{
    const ItemCatalogLoad load = LoadItemCatalog(MU_REPO_ROOT);
    REQUIRE_MESSAGE(load.catalog.has_value(), load.error);
    const ItemCatalog& catalog = *load.catalog;
    CHECK(catalog.items.size() > 800);
    CHECK(catalog.familyCounts.count("swords") == 1);

    const ItemCatalogEntry* kris = catalog.FindByType(0);
    REQUIRE(kris != nullptr);
    CHECK(kris->name == "Kris");
    CHECK(kris->family == "swords");
    CHECK(kris->tier.value == 1);
    REQUIRE_FALSE(kris->models.empty());
    CHECK(kris->models[0].bmd == "src/bin/Data/Item/Sword01.bmd");
    CHECK(kris->models[0].triangles > 0);
    CHECK(kris->models[0].originalSha256.size() == 64);

    const ItemCatalogEntry* helm = catalog.FindByType(7 * ITEMS_PER_GROUP);
    REQUIRE(helm != nullptr);
    REQUIRE_FALSE(helm->models.empty());
    CHECK(helm->models[0].bmd.find("src/bin/Data/Player/") == 0);
}
#endif
