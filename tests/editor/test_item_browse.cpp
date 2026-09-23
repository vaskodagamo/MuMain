#include "stdafx.h"

#include <doctest.h>

#include "Assets/ItemBrowse.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace Editor::Items;
using Editor::Assets::ItemCatalog;
using Editor::Assets::ItemCatalogEntry;
using Editor::Assets::ItemModel;

namespace
{
using RequireClass = std::array<std::uint8_t, MAX_CLASS>;

constexpr RequireClass KNIGHTS = {0, 1, 0, 1, 1, 0, 0};        // DK, MG, DL from the base class
constexpr RequireClass SECOND_KNIGHTS = {0, 2, 0, 0, 0, 0, 0}; // Blade Knight and up
constexpr RequireClass DW_AND_DK = {1, 1, 0, 0, 0, 0, 0};      // MG shares it
constexpr RequireClass ELVES = {0, 0, 1, 0, 0, 0, 0};

ItemCatalogEntry Entry(int type, const std::string& family, int tier, int rank, int size, bool drops = true)
{
    ItemCatalogEntry entry;
    entry.group = type / Editor::Assets::ITEMS_PER_GROUP;
    entry.index = type % Editor::Assets::ITEMS_PER_GROUP;
    entry.family = family;
    entry.tier.value = tier;
    entry.tier.familyRank = rank;
    entry.tier.familySize = size;
    entry.tier.dropsFromMonsters = drops;
    return entry;
}

TableFacts Facts(int type, const std::string& name, const RequireClass& classes, int dropLevel = 0,
                 bool hasModel = true)
{
    TableFacts facts;
    facts.type = type;
    facts.name = name;
    facts.requireClass = classes;
    facts.dropLevel = dropLevel;
    facts.hasModel = hasModel;
    return facts;
}

// A few swords, a bow and an item without a catalog entry.
struct Fixture
{
    std::vector<ItemCatalogEntry> entries = {
        Entry(0, "swords", 1, 2, 6),        // Kris
        Entry(1, "swords", 1, 1, 6),        // Short Sword
        Entry(5, "swords", 3, 3, 6),        // Blade
        Entry(20, "swords", 7, 4, 6),       // Knight Blade
        Entry(19, "swords", 5, 6, 6, false) // Divine Sword: never drops
    };
    std::vector<BrowseRow> rows;

    Fixture()
    {
        rows.push_back(MakeRow(Facts(0, "Kris", KNIGHTS, 6), &entries[0]));
        rows.push_back(MakeRow(Facts(1, "Short Sword", KNIGHTS, 3), &entries[1]));
        rows.push_back(MakeRow(Facts(5, "Blade", DW_AND_DK, 36), &entries[2]));
        rows.push_back(MakeRow(Facts(20, "Knight Blade", SECOND_KNIGHTS, 116), &entries[3]));
        rows.push_back(MakeRow(Facts(19, "Divine Sword of Archangel", KNIGHTS, 86), &entries[4]));
        rows.push_back(MakeRow(Facts(4 * 512 + 0, "Short Bow", ELVES, 2), nullptr));
        rows.push_back(MakeRow(Facts(14 * 512 + 0, "\xC3\x89lixir", {}, 0, false), nullptr)); // Élixir
    }

    std::vector<std::string> Names(const BrowseFilter& filter, const SortOrder& order = {}) const
    {
        std::vector<std::string> names;
        for (const int row : FilterAndSort(rows, filter, order))
            names.push_back(rows[row].name);
        return names;
    }
};

BrowseFilter ForClass(int baseClass, int stage)
{
    BrowseFilter filter;
    filter.baseClass = baseClass;
    filter.classStage = stage;
    return filter;
}
} // namespace

TEST_CASE("MakeRow joins the client table and the catalog [editor][items]")
{
    Fixture fixture;
    const BrowseRow& kris = fixture.rows[0];
    CHECK(kris.key == "0-0");
    CHECK(kris.family == "swords");
    CHECK(kris.tier == 1);
    CHECK(kris.catalog == &fixture.entries[0]);
    const BrowseRow& bow = fixture.rows[5];
    CHECK(bow.key == "4-0");
    CHECK(bow.group == 4);
    CHECK(bow.family.empty());
    CHECK(bow.tier == 0);
    CHECK(bow.status == ItemStatus::Unknown);

    ItemCatalogEntry unnamed = Entry(7 * 512 + 53, "helms", 1, 1, 1);
    unnamed.name = "catalog name";
    CHECK(MakeRow(Facts(7 * 512 + 53, "", {}), &unnamed).name == "catalog name");
}

TEST_CASE("the tier order runs basic to rare, never-dropping items last [editor][items]")
{
    Fixture fixture;
    BrowseFilter swords;
    swords.family = "swords";
    CHECK(fixture.Names(swords) ==
          std::vector<std::string>{"Short Sword", "Kris", "Blade", "Knight Blade", "Divine Sword of Archangel"});

    // A model without a table row (no name) ranked first by the catalog still comes after the named items.
    const ItemCatalogEntry unnamed = Entry(33, "swords", 1, 0, 6);
    fixture.rows.push_back(MakeRow(Facts(33, "", {}), &unnamed));
    CHECK(fixture.Names(swords).back().empty());
    fixture.rows.pop_back();

    // Without a family, rows without a tier (no catalog) come after all ranked rows,
    // in both directions.
    BrowseFilter all;
    std::vector<std::string> names = fixture.Names(all);
    REQUIRE(names.size() == 7);
    CHECK(names[5] == "Short Bow");
    names = fixture.Names(all, {SortKey::Tier, true});
    CHECK(names[0] == "Divine Sword of Archangel");
    CHECK(names[4] == "Short Sword");
    CHECK(std::vector<std::string>(names.begin() + 5, names.end()) ==
          std::vector<std::string>{"\xC3\x89lixir", "Short Bow"});
}

TEST_CASE("the class filter is the engine's rule, with the Magic Gladiator's share [editor][items]")
{
    Fixture fixture;
    constexpr int DK = CLASS_KNIGHT;
    CHECK(fixture.Names(ForClass(DK, 1)) ==
          std::vector<std::string>{"Short Sword", "Kris", "Blade", "Divine Sword of Archangel"});
    CHECK(fixture.Names(ForClass(DK, 2)).size() == 5); // Knight Blade from the Blade Knight on

    const std::vector<std::string> mg = fixture.Names(ForClass(CLASS_DARK, 1));
    CHECK(std::find(mg.begin(), mg.end(), "Blade") != mg.end()); // DW + DK item
    CHECK(std::find(mg.begin(), mg.end(), "Knight Blade") == mg.end());

    CHECK(fixture.Names(ForClass(CLASS_ELF, 3)) == std::vector<std::string>{"Short Bow"});
    CHECK(fixture.Names(ForClass(CLASS_SUMMONER, 3)).empty());
}

TEST_CASE("tier range, status, model and search filters [editor][items]")
{
    Fixture fixture;
    BrowseFilter tiers;
    tiers.tierMin = 3;
    tiers.tierMax = 5;
    // Rows without a tier pass only the full range.
    CHECK(fixture.Names(tiers) == std::vector<std::string>{"Blade", "Divine Sword of Archangel"});

    fixture.rows[2].status = ItemStatus::Changed;
    BrowseFilter changed;
    changed.status = ItemStatus::Changed;
    CHECK(fixture.Names(changed) == std::vector<std::string>{"Blade"});

    BrowseFilter withModel;
    withModel.onlyWithModel = true;
    CHECK(fixture.Names(withModel).size() == 6);

    BrowseFilter search;
    search.search = "SWORD";
    CHECK(fixture.Names(search) == std::vector<std::string>{"Short Sword", "Divine Sword of Archangel"});
    search.search = "\xC3\xA9lix"; // élix finds Élixir
    CHECK(fixture.Names(search) == std::vector<std::string>{"\xC3\x89lixir"});
    search.search = "0-19"; // the key
    CHECK(fixture.Names(search) == std::vector<std::string>{"Divine Sword of Archangel"});
}

TEST_CASE("the other sort keys, both directions [editor][items]")
{
    Fixture fixture;
    BrowseFilter swords;
    swords.family = "swords";
    CHECK(fixture.Names(swords, {SortKey::GroupIndex, false}).front() == "Kris");
    CHECK(fixture.Names(swords, {SortKey::GroupIndex, true}).front() == "Knight Blade");
    CHECK(fixture.Names(swords, {SortKey::Name, false}).front() == "Blade");
    CHECK(fixture.Names(swords, {SortKey::DropLevel, false}).front() == "Short Sword");
    CHECK(fixture.Names(swords, {SortKey::DropLevel, true}).front() == "Knight Blade");

    for (BrowseRow& row : fixture.rows)
        row.status = ItemStatus::Original;
    fixture.rows[4].status = ItemStatus::AtCodex;
    fixture.rows[2].status = ItemStatus::Changed;
    const std::vector<std::string> byStatus = fixture.Names(swords, {SortKey::Status, true});
    CHECK(byStatus[0] == "Divine Sword of Archangel");
    CHECK(byStatus[1] == "Blade");
}

TEST_CASE("family counts follow every filter but the family [editor][items]")
{
    Fixture fixture;
    BrowseFilter filter = ForClass(CLASS_KNIGHT, 1);
    filter.family = "bows"; // ignored for the counts
    CHECK(CountFamilies(fixture.rows, filter) == std::map<std::string, int>{{"swords", 4}});
}

TEST_CASE("status: at Codex, then changed, then original [editor][items]")
{
    ItemCatalogEntry item = Entry(0, "swords", 1, 1, 1);
    ItemModel model;
    model.bmd = "src/bin/Data/Item/Sword01.bmd";
    model.originalSha256 = "aa";
    item.models.push_back(model);

    std::map<std::string, std::string> files = {{"src/bin/Data/Item/Sword01.bmd", "aa"}};
    const auto sha = [&files](const std::string& bmd) { return files[bmd]; };
    CHECK(ComputeStatus(item, sha) == ItemStatus::Original);

    files["src/bin/Data/Item/Sword01.bmd"] = "bb";
    CHECK(ComputeStatus(item, sha) == ItemStatus::Changed);
    files.clear(); // a missing file is a change too
    CHECK(ComputeStatus(item, sha) == ItemStatus::Changed);

    item.requests.push_back({"2026-09-23-0-0-upscale", "delivered", "codex-1"});
    CHECK(ComputeStatus(item, sha) == ItemStatus::AtCodex);
    item.requests[0].status = "accepted";
    CHECK(ComputeStatus(item, sha) == ItemStatus::Changed);
}

TEST_CASE("class stages and the class summary [editor][items]")
{
    CHECK(ClassStages(CLASS_KNIGHT).size() == 3);
    CHECK(ClassStages(CLASS_DARK).size() == 2);
    CHECK(ClassStages(CLASS_DARK_LORD).back() == 3);
    CHECK(ClassStages(ANY_CLASS).empty());
    CHECK(std::string(ClassLabel(CLASS_SUMMONER)) == "SUM");
    CHECK(ClassSummary(SECOND_KNIGHTS) == "DK 2");
    CHECK(ClassSummary(DW_AND_DK) == "DW 1, DK 1, MG 1");
    CHECK(ClassSummary(RequireClass{}).empty());
}

#ifdef MU_REPO_ROOT
TEST_CASE("the real catalog: DK stage 1 swords run Short Sword, Kris ... [editor][items]")
{
    const Editor::Assets::ItemCatalogLoad load = Editor::Assets::LoadItemCatalog(MU_REPO_ROOT);
    REQUIRE_MESSAGE(load.catalog.has_value(), load.error);
    std::vector<BrowseRow> rows;
    for (const ItemCatalogEntry& entry : load.catalog->items)
    {
        // The table facts the catalog recorded stand in for the running client's table.
        RequireClass classes{};
        std::copy(entry.classStages.begin(), entry.classStages.end(), classes.begin());
        rows.push_back(MakeRow(Facts(entry.Type(), entry.name, classes, entry.dropLevel), &entry));
    }
    BrowseFilter filter = ForClass(CLASS_KNIGHT, 1);
    filter.family = "swords";
    std::vector<std::string> names;
    for (const int row : FilterAndSort(rows, filter, {}))
        names.push_back(rows[row].name);
    REQUIRE(names.size() > 10);
    CHECK(names[0] == "Short Sword");
    CHECK(names[1] == "Kris");
    CHECK(std::find(names.begin(), names.end(), "Knight Blade") == names.end()); // Blade Knight only
    CHECK(names.back() == "Divine Sword of Archangel");                       // never drops
}
#endif
