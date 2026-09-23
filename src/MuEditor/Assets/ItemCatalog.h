#pragma once

#ifdef _EDITOR

#include "AssetCatalog.h" // RequestRef, ClientReview

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// The item facts the Item Editor's Browse tab shows, read from
// <repo>/assets-work/Items/catalog.json (schema "mu-item-catalog/1"), which
// tools/item_editor/build_item_catalog.py generates. The fields and the tier
// rule are documented in assets-work/Items/README.md. Paths are relative to the
// repository root and use '/'.
namespace Editor::Assets
{
// Class stages in the order of the catalog's "classes" and of the engine's
// RequireClass: DW, DK, Elf, MG, DL, SUM, RF.
constexpr int ITEM_CLASS_COUNT = 7;
using ItemClassStages = std::array<int, ITEM_CLASS_COUNT>;

// The item type is group * ITEMS_PER_GROUP + index (MAX_ITEM_INDEX in the engine).
constexpr int ITEMS_PER_GROUP = 512;

struct ItemTexture
{
    std::string name;      // as the BMD names it, e.g. sword44_R.jpg
    std::string container; // e.g. src/bin/Data/Item/sword44_R.OZJ; empty for hidden or missing ones
};

struct ItemModel
{
    std::string role;      // item, class-variant, left-hand, right-hand or inventory
    std::string className; // class-variant: the class that wears it (dl, sum, rf)
    std::string condition; // inventory: when the engine draws it (C++ condition)
    std::string bmd;       // e.g. src/bin/Data/Item/Sword20.bmd
    bool exists = true;
    int meshes = 0;
    int triangles = 0;
    std::vector<ItemTexture> textures; // sorted by name
    std::string originalRevision;
    std::string originalSha256;
};

struct ItemTier
{
    int value = 0; // 1..7; 0 = unknown
    int score = 0;
    std::string scoreSource; // openmu-drop-level or client-level
    bool dropsFromMonsters = true;
    int familyRank = 0; // 1 = most basic of its family
    int familySize = 0;
    std::string source; // computed or owner
    std::string note;
};

struct ItemBadges
{
    bool socket = false;
    bool set = false;
    bool option380 = false;
    bool excellent = false;
    int maxItemLevel = 0; // 0 = unknown
};

struct ItemCatalogEntry
{
    std::string key; // "<group>-<index>"
    int group = 0;
    int index = 0;
    std::string name; // empty for a model without a table row
    bool inTable = true;
    std::string family;
    // From the client item table when the catalog was built.
    ItemClassStages classStages{};
    int requireLevel = 0;
    int dropLevel = 0;
    int width = 0;
    int height = 0;
    bool twoHand = false;
    ItemTier tier;
    ItemBadges badges;
    std::vector<ItemModel> models; // the item's own model first
    std::string originalRevision;
    std::string originalSha256;
    std::vector<RequestRef> requests;
    std::optional<ClientReview> clientReview;
    // Armour parts (groups 7-11): the index of their set and the keys of every part
    // of it (this one included); none for other items.
    std::optional<int> armourSet;
    std::vector<std::string> armourSetParts;
    // Texture container -> the other consumers that use it too: item keys, and
    // other:<MODEL_...> for models that are not items.
    std::map<std::string, std::vector<std::string>> sharedWith;

    int Type() const { return group * ITEMS_PER_GROUP + index; }
};

struct ItemCatalog
{
    std::vector<ItemCatalogEntry> items; // by type
    std::map<std::string, int> familyCounts;

    const ItemCatalogEntry* FindByType(int type) const;
    const ItemCatalogEntry* FindByKey(const std::string& key) const;
};

std::filesystem::path ItemAssetsDir(const std::filesystem::path& repoRoot);
std::filesystem::path ItemCatalogFile(const std::filesystem::path& repoRoot);
// The owner's verdicts (Looks good / Needs work), keyed by item key; see ClientReview.h.
std::filesystem::path ItemClientReviewFile(const std::filesystem::path& repoRoot);
// The owner's picked concept image of an item (tools/item_editor/concepts.py pick).
std::filesystem::path ItemConceptImage(const std::filesystem::path& repoRoot, const std::string& key);

// Parses catalog.json text. Returns false and fills `error` when it is not an
// item catalog the editor understands.
bool ParseItemCatalog(const std::string& text, ItemCatalog& out, std::string& error);

struct ItemCatalogLoad
{
    bool fileFound = false;
    std::optional<ItemCatalog> catalog; // set when the file was found and parsed
    std::string error;                  // why catalog is empty
};

ItemCatalogLoad LoadItemCatalog(const std::filesystem::path& repoRoot);

// A request that still waits for the art builder or the owner: open, claimed or
// delivered (not accepted, rejected or withdrawn).
bool IsRequestPending(const RequestRef& request);
} // namespace Editor::Assets

#endif // _EDITOR
