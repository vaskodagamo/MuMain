#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// How the game draws each item, read from <repo>/assets-work/Items/render-facts.json
// (schema "mu-item-render-facts/1"), which tools/item_editor/render_facts.py generates
// from the engine's item render code. The modes and the request rules are documented in
// assets-work/Items/README.md ("How the game draws items") and
// assets-work/Items/requests/README.md ("How the game draws the item").
namespace Editor::Assets
{
// One mesh: its draw mode (opaque, alpha-test, blended-additive, blended-alpha,
// blended-subtract, hidden) when the item is worn, lying on the ground and in the
// inventory; empty where the model is not used (JSON null).
struct ItemMeshDraw
{
    int mesh = 0;
    std::string texture;
    std::string worn;
    std::string dropped;
    std::string inventory;
};

struct ItemModelDraw
{
    std::string role; // item, class-variant, left-hand, right-hand or inventory (as in the catalog)
    std::string bmd;
    std::vector<ItemMeshDraw> meshes;
};

struct ItemRenderEntry
{
    std::string key;
    std::string status;  // verified-in-client, from-code or unverified
    std::string summary; // written by hand for the items checked closely; may be empty
    std::string drawn;   // one line: which meshes blend, which effects the engine adds
    std::vector<ItemModelDraw> models;
    std::vector<std::string> effects;  // request.effects: what the engine adds, one sentence each
    std::vector<std::string> mustKeep; // request.must_keep: the render lines of a request
};

struct ItemRenderFacts
{
    std::map<std::string, ItemRenderEntry> items; // by item key

    const ItemRenderEntry* Find(const std::string& key) const;
};

std::filesystem::path ItemRenderFactsFile(const std::filesystem::path& repoRoot);

// Parses render-facts.json text. Returns false and fills `error` when it is not a
// render facts file the editor understands.
bool ParseItemRenderFacts(const std::string& text, ItemRenderFacts& out, std::string& error);

struct ItemRenderFactsLoad
{
    std::optional<ItemRenderFacts> facts; // set when the file was found and parsed
    std::string error;                    // why facts is empty
};

ItemRenderFactsLoad LoadItemRenderFacts(const std::filesystem::path& repoRoot);
} // namespace Editor::Assets

#endif // _EDITOR
