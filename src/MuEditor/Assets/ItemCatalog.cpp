#include "ItemCatalog.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "JsonFields.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Editor::Assets
{
namespace
{
// Hide Editor::Text (the namespace) behind the JSON field readers.
using Json::Array;
using Json::Bool;
using Json::Int;
using Json::Member;
using Json::Text;

constexpr const char* ITEM_CATALOG_SCHEMA = "mu-item-catalog/1";
constexpr const char* ASSETS_FOLDER = "assets-work";
constexpr const char* ITEMS_FOLDER = "Items";
constexpr const char* CATALOG_FILE_NAME = "catalog.json";
constexpr const char* CLIENT_REVIEW_FILE_NAME = "client-review.json";
constexpr const char* CONCEPTS_FOLDER = "concepts";
constexpr const char* CONCEPT_FILE_NAME = "concept.jpg";

// The catalog's class keys in RequireClass order.
constexpr const char* CLASS_KEYS[ITEM_CLASS_COUNT] = {"dw", "dk", "elf", "mg", "dl", "sum", "rf"};

// Request states that are finished; every other state still waits for someone.
constexpr const char* FINISHED_REQUEST_STATES[] = {"accepted", "rejected", "withdrawn"};

std::vector<ItemTexture> ParseTextures(const json& model)
{
    std::vector<ItemTexture> textures;
    for (const auto& [name, container] : Member(model, "textures").items())
        textures.push_back({name, container.is_string() ? container.get<std::string>() : std::string()});
    return textures; // json objects iterate their keys sorted
}

ItemModel ParseModel(const json& entry)
{
    ItemModel model;
    model.role = Text(entry, "role");
    model.className = Text(entry, "class");
    model.modelConstant = Text(entry, "model");
    model.condition = Text(entry, "condition");
    model.bmd = Text(entry, "bmd");
    model.exists = Bool(entry, "exists", true);
    const json& structure = Member(entry, "structure");
    model.meshes = Int(structure, "meshes", 0);
    model.triangles = Int(structure, "triangles", 0);
    model.textures = ParseTextures(entry);
    const json& original = Member(entry, "original");
    model.originalRevision = Text(original, "revision");
    model.originalSha256 = Text(original, "sha256");
    return model;
}

void ParseTable(const json& table, ItemCatalogEntry& item)
{
    const json& classes = Member(table, "classes");
    for (int i = 0; i < ITEM_CLASS_COUNT; ++i)
        item.classStages[i] = Int(classes, CLASS_KEYS[i], 0);
    item.requireLevel = Int(table, "require_level", 0);
    item.dropLevel = Int(table, "drop_level", 0);
    item.width = Int(table, "width", 0);
    item.height = Int(table, "height", 0);
    item.twoHand = Bool(table, "two_hand", false);
}

ItemTier ParseTier(const json& tier)
{
    ItemTier parsed;
    parsed.value = Int(tier, "value", 0);
    parsed.score = Int(tier, "score", 0);
    parsed.scoreSource = Text(tier, "score_source");
    parsed.dropsFromMonsters = Bool(tier, "drops_from_monsters", true);
    parsed.familyRank = Int(tier, "family_rank", 0);
    parsed.familySize = Int(tier, "family_size", 0);
    parsed.source = Text(tier, "source");
    parsed.note = Text(tier, "note");
    return parsed;
}

ItemBadges ParseBadges(const json& badges)
{
    ItemBadges parsed;
    parsed.socket = Bool(badges, "socket", false);
    parsed.set = Bool(badges, "set", false);
    parsed.option380 = Bool(badges, "option380", false);
    parsed.excellent = Bool(badges, "excellent", false);
    parsed.maxItemLevel = Int(badges, "max_item_level", 0);
    return parsed;
}

std::optional<ClientReview> ParseClientReview(const json& entry)
{
    const auto it = entry.find("client_review");
    if (it == entry.end() || !it->is_object())
        return std::nullopt;
    return ClientReview{Text(*it, "verdict"), Text(*it, "note"), Text(*it, "date")};
}

void ParseArmourSet(const json& entry, ItemCatalogEntry& item)
{
    const auto set = entry.find("armour_set");
    if (set == entry.end() || !set->is_object())
        return;
    item.armourSet = Int(*set, "index", 0);
    item.armourSetParts = Json::TextList(*set, "parts");
}

std::map<std::string, std::vector<std::string>> ParseSharedWith(const json& entry)
{
    std::map<std::string, std::vector<std::string>> shared;
    const json& sharedWith = Member(entry, "shared_with");
    for (const auto& [container, consumers] : sharedWith.items())
        shared[container] = Json::TextList(sharedWith, container.c_str());
    return shared;
}

ItemCatalogEntry ParseItem(const json& entry)
{
    ItemCatalogEntry item;
    item.key = Text(entry, "key");
    item.group = Int(entry, "group", 0);
    item.index = Int(entry, "index", 0);
    item.name = Text(entry, "name");
    item.inTable = Bool(entry, "in_table", true);
    item.family = Text(entry, "family");
    ParseTable(Member(entry, "table"), item);
    item.tier = ParseTier(Member(entry, "tier"));
    item.badges = ParseBadges(Member(entry, "badges"));
    for (const json& model : Array(entry, "models"))
        item.models.push_back(ParseModel(model));
    const json& original = Member(entry, "original");
    item.originalRevision = Text(original, "revision");
    item.originalSha256 = Text(original, "sha256");
    for (const json& request : Array(entry, "requests"))
        item.requests.push_back({Text(request, "id"), Text(request, "status"), Text(request, "assigned_to")});
    item.clientReview = ParseClientReview(entry);
    ParseArmourSet(entry, item);
    item.sharedWith = ParseSharedWith(entry);
    return item;
}
} // namespace

const ItemCatalogEntry* ItemCatalog::FindByType(int type) const
{
    const auto it = std::lower_bound(items.begin(), items.end(), type,
                                     [](const ItemCatalogEntry& item, int value) { return item.Type() < value; });
    return it != items.end() && it->Type() == type ? &*it : nullptr;
}

const ItemCatalogEntry* ItemCatalog::FindByKey(const std::string& key) const
{
    const std::size_t dash = key.find('-');
    if (dash == std::string::npos)
        return nullptr;
    const int group = std::atoi(key.substr(0, dash).c_str());
    const int index = std::atoi(key.substr(dash + 1).c_str());
    const ItemCatalogEntry* item = FindByType(group * ITEMS_PER_GROUP + index);
    return item != nullptr && item->key == key ? item : nullptr;
}

fs::path ItemAssetsDir(const fs::path& repoRoot)
{
    return repoRoot / ASSETS_FOLDER / ITEMS_FOLDER;
}

fs::path ItemCatalogFile(const fs::path& repoRoot)
{
    return ItemAssetsDir(repoRoot) / CATALOG_FILE_NAME;
}

fs::path ItemClientReviewFile(const fs::path& repoRoot)
{
    return ItemAssetsDir(repoRoot) / CLIENT_REVIEW_FILE_NAME;
}

fs::path ItemConceptImage(const fs::path& repoRoot, const std::string& key)
{
    return ItemAssetsDir(repoRoot) / CONCEPTS_FOLDER / Editor::Text::Utf8Path(key) / CONCEPT_FILE_NAME;
}

bool ParseItemCatalog(const std::string& text, ItemCatalog& out, std::string& error)
{
    const json document = json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "catalog.json is not valid JSON";
        return false;
    }
    if (Text(document, "schema") != ITEM_CATALOG_SCHEMA)
    {
        error = std::string("catalog.json is not schema ") + ITEM_CATALOG_SCHEMA;
        return false;
    }
    out = ItemCatalog{};
    for (const auto& [key, entry] : Member(document, "items").items())
    {
        if (entry.is_object())
            out.items.push_back(ParseItem(entry));
    }
    std::sort(out.items.begin(), out.items.end(),
              [](const ItemCatalogEntry& a, const ItemCatalogEntry& b) { return a.Type() < b.Type(); });
    for (const ItemCatalogEntry& item : out.items)
        ++out.familyCounts[item.family];
    return true;
}

ItemCatalogLoad LoadItemCatalog(const fs::path& repoRoot)
{
    ItemCatalogLoad load;
    const fs::path file = ItemCatalogFile(repoRoot);
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        load.error = "no item catalog at " + Editor::Text::PathToUtf8(file);
        return load;
    }
    load.fileFound = true;
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    ItemCatalog catalog;
    if (!ParseItemCatalog(text, catalog, load.error))
        return load;
    load.catalog = std::move(catalog);
    return load;
}

bool IsRequestPending(const RequestRef& request)
{
    return std::none_of(std::begin(FINISHED_REQUEST_STATES), std::end(FINISHED_REQUEST_STATES),
                        [&request](const char* state) { return request.status == state; });
}
} // namespace Editor::Assets

#endif // _EDITOR
