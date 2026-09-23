#pragma once

#ifdef _EDITOR

#include "ItemCatalog.h"

#include "Core/Globals/_define.h" // MAX_CLASS

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

// What the Item Editor's Browse tab lists and in which order: one row per item,
// the filters (class and stage, family, tier range, status, search, model) and
// the sort keys. No ImGui and no engine state; the tab builds the rows from the
// client item table and the item catalog (ItemCatalog.h).
namespace Editor::Items
{
// Whether the item's files still match the catalog's originals.
enum class ItemStatus
{
    Original, // every model file has the catalog's original SHA-256
    Changed,  // a model file differs from (or is missing against) its original
    AtCodex,  // a request for the item is open, claimed or delivered
    Unknown,  // no catalog entry, or not computed yet
};

const char* StatusLabel(ItemStatus status);

struct BrowseRow
{
    int type = 0; // group * 512 + index
    int group = 0;
    int index = 0;
    std::string key;        // "<group>-<index>"
    std::string name;       // UTF-8; empty for a model without a table row
    std::string foldedName; // Editor::Text::FoldCase(name), for the search
    // The client table's ITEM_ATTRIBUTE::RequireClass (the running game's data, with edits).
    std::array<std::uint8_t, MAX_CLASS> requireClass{};
    int dropLevel = 0; // ITEM_ATTRIBUTE::Level
    int requireLevel = 0;
    bool hasModel = false;
    // From the catalog; empty/0 without one.
    std::string family;
    int tier = 0; // 1..7, 0 = unknown
    bool dropsFromMonsters = true;
    int familyRank = 0;
    int familySize = 0;
    ItemStatus status = ItemStatus::Unknown;
    const Assets::ItemCatalogEntry* catalog = nullptr;
};

// The client-table facts of one item, as the engine holds them.
struct TableFacts
{
    int type = 0;
    std::string name;
    std::array<std::uint8_t, MAX_CLASS> requireClass{};
    int dropLevel = 0;
    int requireLevel = 0;
    bool hasModel = false;
};

// A row from the table facts and the item's catalog entry (may be null).
BrowseRow MakeRow(const TableFacts& facts, const Assets::ItemCatalogEntry* catalog);

// The item's status: at Codex when a request is pending, else changed when any of
// its model files' SHA-256 (`currentSha256(bmd)`, empty when the file is missing)
// differs from the catalog's original, else original.
ItemStatus ComputeStatus(const Assets::ItemCatalogEntry& item,
                         const std::function<std::string(const std::string& bmd)>& currentSha256);

constexpr int ANY_CLASS = -1;
constexpr int LOWEST_TIER = 1;
constexpr int HIGHEST_TIER = 7;

// The class stages a base class has (Magic Gladiator, Dark Lord and Rage Fighter
// skip the second class): {1, 2, 3} or {1, 3}.
std::span<const std::uint8_t> ClassStages(int baseClass);
// Short class label: DW, DK, Elf, MG, DL, SUM, RF.
const char* ClassLabel(int baseClass);

// Which classes may use the item and from which stage, e.g. "DK 2, MG 1"; by the
// engine's rule (GameLogic::Items::CanClassEquip), so it includes the Magic
// Gladiator's share of Dark Wizard + Dark Knight items. Empty when no class may.
std::string ClassSummary(std::span<const std::uint8_t, MAX_CLASS> requireClass);

struct BrowseFilter
{
    int baseClass = ANY_CLASS; // a base CLASS_TYPE (0..6), or ANY_CLASS
    int classStage = 1;        // 1..3, used with baseClass
    std::string family;        // empty: every family
    int tierMin = LOWEST_TIER; // items without a tier pass only the full range
    int tierMax = HIGHEST_TIER;
    std::optional<ItemStatus> status;
    std::string search; // any case; part of the name or of the key ("0-19")
    bool onlyWithModel = false;

    bool operator==(const BrowseFilter&) const = default;
};

enum class SortKey
{
    Tier, // named, dropping items first, then tier, place in the family, name: basic -> rare
    GroupIndex,
    Name,
    DropLevel,
    RequireLevel,
    Status,
};

struct SortOrder
{
    SortKey key = SortKey::Tier;
    bool descending = false;

    bool operator==(const SortOrder&) const = default;
};

bool PassesFilter(const BrowseRow& row, const BrowseFilter& filter);

// Indices into `rows` that pass `filter`, in `order`.
std::vector<int> FilterAndSort(const std::vector<BrowseRow>& rows, const BrowseFilter& filter, const SortOrder& order);

// Rows per family that pass every part of `filter` except the family itself
// (the counts next to the family list). Rows without a family are not counted.
std::map<std::string, int> CountFamilies(const std::vector<BrowseRow>& rows, const BrowseFilter& filter);
} // namespace Editor::Items

#endif // _EDITOR
