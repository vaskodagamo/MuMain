// The engine's class rule needs the platform types (BYTE) the precompiled header brings.
#include "stdafx.h"

#include "ItemBrowse.h"

#ifdef _EDITOR

#include "EditorText.h"

#include "GameLogic/Items/ItemClassRule.h"

#include <algorithm>
#include <iterator>
#include <tuple>

namespace Editor::Items
{
static_assert(Assets::ITEM_CLASS_COUNT == MAX_CLASS, "the catalog lists the engine's base classes");

namespace
{
constexpr const char* CLASS_LABELS[MAX_CLASS] = {"DW", "DK", "Elf", "MG", "DL", "SUM", "RF"};
constexpr std::uint8_t THREE_STAGES[] = {1, 2, 3};
constexpr std::uint8_t NO_SECOND_STAGE[] = {1, 3};
constexpr const char* STATUS_DELIVERED = "delivered";

bool IsBaseClass(int baseClass)
{
    return baseClass >= 0 && baseClass < MAX_CLASS;
}

bool HasSecondClass(int baseClass)
{
    return baseClass != CLASS_DARK && baseClass != CLASS_DARK_LORD && baseClass != CLASS_RAGEFIGHTER;
}

bool CanUse(const BrowseRow& row, int baseClass, int stage)
{
    return GameLogic::Items::CanClassEquip(row.requireClass, static_cast<CLASS_TYPE>(baseClass),
                                           static_cast<std::uint8_t>(stage));
}

bool IsFullTierRange(const BrowseFilter& filter)
{
    return filter.tierMin <= LOWEST_TIER && filter.tierMax >= HIGHEST_TIER;
}

bool PassesTier(const BrowseRow& row, const BrowseFilter& filter)
{
    if (row.tier == 0)
        return IsFullTierRange(filter);
    return row.tier >= filter.tierMin && row.tier <= filter.tierMax;
}

bool PassesSearch(const BrowseRow& row, const std::string& foldedNeedle)
{
    if (foldedNeedle.empty())
        return true;
    return row.foldedName.find(foldedNeedle) != std::string::npos || row.key.find(foldedNeedle) != std::string::npos;
}

bool PassesAllButFamily(const BrowseRow& row, const BrowseFilter& filter, const std::string& foldedNeedle)
{
    if (filter.onlyWithModel && !row.hasModel)
        return false;
    if (IsBaseClass(filter.baseClass) && !CanUse(row, filter.baseClass, filter.classStage))
        return false;
    if (!PassesTier(row, filter))
        return false;
    if (filter.status && row.status != *filter.status)
        return false;
    return PassesSearch(row, foldedNeedle);
}

// Place in the family as a fraction, so rows of different families interleave by it.
double FamilyPosition(const BrowseRow& row)
{
    return row.familySize > 0 ? static_cast<double>(row.familyRank) / row.familySize : 0.0;
}

// Basic -> rare: known tiers first, models without a table row (no name) after
// the named items, dropping items before shop/event/quest items (as the
// catalog's family order has them), then tier, place in the family, name.
auto TierKey(const BrowseRow& row)
{
    return std::make_tuple(row.tier == 0, row.name.empty(), !row.dropsFromMonsters, row.tier, FamilyPosition(row),
                           row.foldedName, row.type);
}

bool TierLess(const BrowseRow& a, const BrowseRow& b)
{
    return TierKey(a) < TierKey(b);
}

// a before b in ascending order of `key`; ties fall back to the tier order.
bool Less(const BrowseRow& a, const BrowseRow& b, SortKey key)
{
    switch (key)
    {
    case SortKey::GroupIndex:
        return a.type < b.type;
    case SortKey::Name:
        return std::tie(a.foldedName, a.type) < std::tie(b.foldedName, b.type);
    case SortKey::DropLevel:
        if (a.dropLevel != b.dropLevel)
            return a.dropLevel < b.dropLevel;
        break;
    case SortKey::RequireLevel:
        if (a.requireLevel != b.requireLevel)
            return a.requireLevel < b.requireLevel;
        break;
    case SortKey::Status:
        if (a.status != b.status)
            return a.status < b.status;
        break;
    case SortKey::Tier:
        break;
    }
    return TierLess(a, b);
}

// Whether row a is listed before row b. Rows without a tier stay at the end of
// the tier order in both directions.
bool Before(const BrowseRow& a, const BrowseRow& b, const SortOrder& order)
{
    const bool aUnranked = a.tier == 0;
    const bool bUnranked = b.tier == 0;
    if (order.key == SortKey::Tier && aUnranked != bUnranked)
        return bUnranked;
    return order.descending ? Less(b, a, order.key) : Less(a, b, order.key);
}

bool PassesFamily(const BrowseRow& row, const BrowseFilter& filter)
{
    return filter.family.empty() || row.family == filter.family;
}
} // namespace

const char* StatusLabel(ItemStatus status)
{
    switch (status)
    {
    case ItemStatus::Original:
        return "original";
    case ItemStatus::Changed:
        return "changed";
    case ItemStatus::AtCodex:
        return "at Codex";
    case ItemStatus::Delivered:
        return "delivered";
    case ItemStatus::Unknown:
        break;
    }
    return "unknown";
}

BrowseRow MakeRow(const TableFacts& facts, const Assets::ItemCatalogEntry* catalog)
{
    BrowseRow row;
    row.type = facts.type;
    row.group = facts.type / Assets::ITEMS_PER_GROUP;
    row.index = facts.type % Assets::ITEMS_PER_GROUP;
    row.key = std::to_string(row.group) + "-" + std::to_string(row.index);
    row.name = facts.name.empty() && catalog != nullptr ? catalog->name : facts.name;
    row.foldedName = Editor::Text::FoldCase(row.name);
    row.requireClass = facts.requireClass;
    row.dropLevel = facts.dropLevel;
    row.requireLevel = facts.requireLevel;
    row.hasModel = facts.hasModel;
    row.catalog = catalog;
    if (catalog == nullptr)
        return row;
    row.family = catalog->family;
    row.tier = catalog->tier.value;
    row.dropsFromMonsters = catalog->tier.dropsFromMonsters;
    row.familyRank = catalog->tier.familyRank;
    row.familySize = catalog->tier.familySize;
    return row;
}

ItemStatus ComputeStatus(const Assets::ItemCatalogEntry& item,
                         const std::function<std::string(const std::string& bmd)>& currentSha256)
{
    return ComputeStatus(item, currentSha256, item.requests);
}

ItemStatus ComputeStatus(const Assets::ItemCatalogEntry& item,
                         const std::function<std::string(const std::string& bmd)>& currentSha256,
                         const std::vector<Assets::RequestRef>& scannedRequests)
{
    const auto isDelivered = [](const Assets::RequestRef& request) { return request.status == STATUS_DELIVERED; };
    if (std::any_of(scannedRequests.begin(), scannedRequests.end(), isDelivered))
        return ItemStatus::Delivered;
    if (std::any_of(scannedRequests.begin(), scannedRequests.end(), Assets::IsRequestPending))
        return ItemStatus::AtCodex;
    for (const Assets::ItemModel& model : item.models)
    {
        if (!model.originalSha256.empty() && currentSha256(model.bmd) != model.originalSha256)
            return ItemStatus::Changed;
    }
    return ItemStatus::Original;
}

std::span<const std::uint8_t> ClassStages(int baseClass)
{
    if (!IsBaseClass(baseClass))
        return {};
    if (HasSecondClass(baseClass))
        return THREE_STAGES;
    return NO_SECOND_STAGE;
}

const char* ClassLabel(int baseClass)
{
    return IsBaseClass(baseClass) ? CLASS_LABELS[baseClass] : "";
}

std::string ClassSummary(std::span<const std::uint8_t, MAX_CLASS> requireClass)
{
    std::vector<std::string> parts;
    for (int baseClass = 0; baseClass < MAX_CLASS; ++baseClass)
    {
        for (const std::uint8_t stage : ClassStages(baseClass))
        {
            if (!GameLogic::Items::CanClassEquip(requireClass, static_cast<CLASS_TYPE>(baseClass), stage))
                continue;
            parts.push_back(std::string(CLASS_LABELS[baseClass]) + " " + std::to_string(stage));
            break;
        }
    }
    return Editor::Text::Join(parts, ", ");
}

bool PassesFilter(const BrowseRow& row, const BrowseFilter& filter)
{
    return PassesFamily(row, filter) && PassesAllButFamily(row, filter, Editor::Text::FoldCase(filter.search));
}

std::vector<int> FilterAndSort(const std::vector<BrowseRow>& rows, const BrowseFilter& filter, const SortOrder& order)
{
    const std::string foldedNeedle = Editor::Text::FoldCase(filter.search);
    std::vector<int> shown;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        if (PassesFamily(rows[i], filter) && PassesAllButFamily(rows[i], filter, foldedNeedle))
            shown.push_back(i);
    }
    std::stable_sort(shown.begin(), shown.end(), [&](int a, int b) { return Before(rows[a], rows[b], order); });
    return shown;
}

std::map<std::string, int> CountFamilies(const std::vector<BrowseRow>& rows, const BrowseFilter& filter)
{
    const std::string foldedNeedle = Editor::Text::FoldCase(filter.search);
    std::map<std::string, int> counts;
    for (const BrowseRow& row : rows)
    {
        if (!row.family.empty() && PassesAllButFamily(row, filter, foldedNeedle))
            ++counts[row.family];
    }
    return counts;
}
} // namespace Editor::Items

#endif // _EDITOR
