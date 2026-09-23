#include "AssetCatalog.h"

#ifdef _EDITOR

#include "EditorText.h"

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Editor::Assets
{
namespace
{
constexpr const char* CATALOG_SCHEMA = "mu-world-catalog/1";
constexpr const char* ASSETS_FOLDER = "assets-work";
constexpr const char* WORLD_PREFIX = "World";
constexpr const char* CATALOG_FILE_NAME = "catalog.json";

std::string Text(const json& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::optional<std::string> OptionalText(const json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
        return std::nullopt;
    return it->get<std::string>();
}

std::vector<std::string> TextList(const json& object, const char* key)
{
    std::vector<std::string> values;
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array())
        return values;
    for (const json& value : *it)
    {
        if (value.is_string())
            values.push_back(value.get<std::string>());
    }
    return values;
}

const json& Member(const json& object, const char* key)
{
    static const json empty = json::object();
    const auto it = object.find(key);
    return it != object.end() && it->is_object() ? *it : empty;
}

const json& Array(const json& object, const char* key)
{
    static const json empty = json::array();
    const auto it = object.find(key);
    return it != object.end() && it->is_array() ? *it : empty;
}

EngineControl ParseEngineControl(const json& row)
{
    EngineControl control;
    control.sourceRow = Text(row, "source_row");
    control.controls = TextList(row, "controls");
    control.requirement = Text(row, "requirement");
    const auto blend = row.find("blend_mesh");
    if (blend != row.end() && blend->is_number_integer())
        control.blendMesh = blend->get<int>();
    control.blendMeshTexture = OptionalText(row, "blend_mesh_texture");
    return control;
}

std::vector<TextureLink> ParseTextures(const json& entry)
{
    std::vector<TextureLink> textures;
    const json& sharedWith = Member(entry, "shared_with");
    for (const auto& [name, container] : Member(entry, "textures").items())
    {
        if (!container.is_string())
            continue;
        textures.push_back({name, container.get<std::string>(), TextList(sharedWith, name.c_str())});
    }
    return textures; // json objects iterate their keys sorted
}

BatchInfo ParseBatch(const json& batch)
{
    BatchInfo info;
    info.name = Text(batch, "name");
    info.role = Text(batch, "role");
    info.agent = Text(batch, "agent");
    info.notes = Text(batch, "notes");
    info.modelDir = OptionalText(batch, "model_dir");
    info.integrationCommits = TextList(batch, "integration_commits");
    return info;
}

std::optional<ClientReview> ParseClientReview(const json& entry)
{
    const auto it = entry.find("client_review");
    if (it == entry.end() || !it->is_object())
        return std::nullopt;
    return ClientReview{Text(*it, "verdict"), Text(*it, "note"), Text(*it, "date")};
}

void ParseHistory(const json& entry, CatalogModel& model)
{
    for (const json& batch : Array(entry, "batches"))
        model.batches.push_back(ParseBatch(batch));
    const json& previews = Member(entry, "previews");
    model.baselinePreview = Text(previews, "baseline");
    model.finalPreview = Text(previews, "final");
    for (const json& request : Array(entry, "requests"))
        model.requests.push_back({Text(request, "id"), Text(request, "status"), Text(request, "assigned_to")});
}

CatalogModel ParseModel(const json& entry)
{
    CatalogModel model;
    model.name = Text(entry, "name");
    const auto type = entry.find("type");
    model.type = type != entry.end() && type->is_number_integer() ? type->get<int>() : NO_TYPE;
    model.bmd = Text(entry, "bmd");
    model.identity = Text(entry, "identity");
    model.inScope = entry.value("in_scope", true);
    model.exclusion = Text(entry, "exclusion");
    model.status = Text(entry, "status");
    model.clientVerified = entry.value("client_verified", false);
    model.clientReview = ParseClientReview(entry);
    model.placementCount = entry.value("placement_count", 0);
    model.textures = ParseTextures(entry);
    for (const json& row : Array(entry, "engine_controls"))
        model.engineControls.push_back(ParseEngineControl(row));
    model.currentSha256 = Text(entry, "current_sha256");
    model.bmdModified = entry.value("bmd_modified", false);
    const json& original = Member(entry, "original");
    model.original = {Text(original, "revision"), OptionalText(original, "archive")};
    model.lastBatch = OptionalText(entry, "last_batch");
    model.modelDir = OptionalText(entry, "model_dir");
    ParseHistory(entry, model);
    return model;
}

bool ByTypeThenName(const CatalogModel& a, const CatalogModel& b)
{
    const bool aTyped = a.type != NO_TYPE;
    const bool bTyped = b.type != NO_TYPE;
    if (aTyped != bTyped)
        return aTyped;
    if (aTyped && a.type != b.type)
        return a.type < b.type;
    return a.name < b.name;
}
} // namespace

const CatalogModel* Catalog::FindByType(int type) const
{
    if (type == NO_TYPE)
        return nullptr;
    const auto it =
        std::find_if(models.begin(), models.end(), [type](const CatalogModel& m) { return m.type == type; });
    return it != models.end() ? &*it : nullptr;
}

const CatalogModel* Catalog::FindByName(const std::string& name) const
{
    const auto it =
        std::find_if(models.begin(), models.end(), [&name](const CatalogModel& m) { return m.name == name; });
    return it != models.end() ? &*it : nullptr;
}

fs::path WorldAssetsDir(const fs::path& repoRoot, int world)
{
    return repoRoot / ASSETS_FOLDER / (WORLD_PREFIX + std::to_string(world));
}

fs::path CatalogFile(const fs::path& repoRoot, int world)
{
    return WorldAssetsDir(repoRoot, world) / CATALOG_FILE_NAME;
}

bool ParseCatalog(const std::string& text, Catalog& out, std::string& error)
{
    const json document = json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "catalog.json is not valid JSON";
        return false;
    }
    if (Text(document, "schema") != CATALOG_SCHEMA)
    {
        error = std::string("catalog.json is not schema ") + CATALOG_SCHEMA;
        return false;
    }
    out = Catalog{};
    try
    {
        out.world = document.value("world", 0);
        out.worldName = Text(document, "world_name");
        for (const char* section : {"models", "untyped_models"})
        {
            for (const auto& [key, entry] : Member(document, section).items())
            {
                if (entry.is_object())
                    out.models.push_back(ParseModel(entry));
            }
        }
    }
    catch (const json::exception& exception)
    {
        error = std::string("catalog.json has an unexpected value: ") + exception.what();
        return false;
    }
    std::sort(out.models.begin(), out.models.end(), ByTypeThenName);
    return true;
}

CatalogLoad LoadCatalog(const fs::path& repoRoot, int world)
{
    CatalogLoad load;
    const fs::path file = CatalogFile(repoRoot, world);
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        load.error = "no catalog at " + Editor::Text::PathToUtf8(file);
        return load;
    }
    load.fileFound = true;
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    Catalog catalog;
    if (!ParseCatalog(text, catalog, load.error))
        return load;
    load.catalog = std::move(catalog);
    return load;
}

std::map<std::string, std::vector<std::string>> TextureConsumers(const CatalogModel& model)
{
    std::map<std::string, std::vector<std::string>> consumers;
    for (const TextureLink& texture : model.textures)
    {
        std::set<std::string> names(texture.sharedWith.begin(), texture.sharedWith.end());
        names.insert(model.name);
        std::vector<std::string>& list = consumers[texture.container];
        list.insert(list.end(), names.begin(), names.end());
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
    return consumers;
}
} // namespace Editor::Assets

#endif // _EDITOR
