#include "ItemRenderFacts.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "ItemCatalog.h" // ItemAssetsDir
#include "JsonFields.h"

#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Editor::Assets
{
namespace
{
using Json::Array;
using Json::Int;
using Json::Member;
using Json::Text;
using Json::TextList;

constexpr const char* RENDER_FACTS_SCHEMA = "mu-item-render-facts/1";
constexpr const char* RENDER_FACTS_FILE_NAME = "render-facts.json";

ItemMeshDraw ParseMesh(const json& entry)
{
    ItemMeshDraw mesh;
    mesh.mesh = Int(entry, "mesh", 0);
    mesh.texture = Text(entry, "texture");
    mesh.worn = Text(entry, "worn"); // null reads as empty: the model is not used there
    mesh.dropped = Text(entry, "dropped");
    mesh.inventory = Text(entry, "inventory");
    return mesh;
}

ItemModelDraw ParseModel(const json& entry)
{
    ItemModelDraw model;
    model.role = Text(entry, "role");
    model.bmd = Text(entry, "bmd");
    for (const json& mesh : Array(entry, "meshes"))
        model.meshes.push_back(ParseMesh(mesh));
    return model;
}

ItemRenderEntry ParseItem(const std::string& key, const json& entry)
{
    ItemRenderEntry item;
    item.key = key;
    item.status = Text(entry, "status");
    item.summary = Text(entry, "summary");
    item.drawn = Text(entry, "drawn");
    for (const json& model : Array(entry, "models"))
        item.models.push_back(ParseModel(model));
    const json& request = Member(entry, "request");
    item.effects = TextList(request, "effects");
    item.mustKeep = TextList(request, "must_keep");
    return item;
}
} // namespace

const ItemRenderEntry* ItemRenderFacts::Find(const std::string& key) const
{
    const auto it = items.find(key);
    return it == items.end() ? nullptr : &it->second;
}

fs::path ItemRenderFactsFile(const fs::path& repoRoot)
{
    return ItemAssetsDir(repoRoot) / RENDER_FACTS_FILE_NAME;
}

bool ParseItemRenderFacts(const std::string& text, ItemRenderFacts& out, std::string& error)
{
    const json document = json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "render-facts.json is not valid JSON";
        return false;
    }
    if (Text(document, "schema") != RENDER_FACTS_SCHEMA)
    {
        error = std::string("render-facts.json is not schema ") + RENDER_FACTS_SCHEMA;
        return false;
    }
    out = ItemRenderFacts{};
    for (const auto& [key, entry] : Member(document, "items").items())
    {
        if (entry.is_object())
            out.items.emplace(key, ParseItem(key, entry));
    }
    return true;
}

ItemRenderFactsLoad LoadItemRenderFacts(const fs::path& repoRoot)
{
    ItemRenderFactsLoad load;
    const fs::path file = ItemRenderFactsFile(repoRoot);
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        load.error = "no render facts at " + Editor::Text::PathToUtf8(file);
        return load;
    }
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    ItemRenderFacts facts;
    if (!ParseItemRenderFacts(text, facts, load.error))
        return load;
    load.facts = std::move(facts);
    return load;
}
} // namespace Editor::Assets

#endif // _EDITOR
