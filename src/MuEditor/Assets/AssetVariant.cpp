#include "AssetVariant.h"

#ifdef _EDITOR

#include "AssetCatalog.h"
#include "EditorText.h"
#include "FileDigest.h"

#include <json.hpp>

#include <fstream>
#include <iterator>
#include <string_view>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Editor::Assets
{
namespace
{
// Catalog paths are relative to the repository and use '/'.
constexpr std::string_view CATALOG_DATA_PREFIX = "src/bin/Data/";
constexpr const char* DATA_DIR = "Data";
constexpr const char* MANIFEST_FILE_NAME = "manifest.json";
constexpr const char* ITEMS_MANIFEST_FILE_NAME = "items-manifest.json";
constexpr const char* ITEMS_OPTION = " --items";
constexpr const char* MANIFEST_SCHEMA = "mu-ab-variant/1";
constexpr const char* MATERIALIZE_SCRIPT = "tools/world_editor/materialize_variant.py";
#ifdef _WIN32
constexpr const char* PYTHON_COMMAND = "py -3";
#else
constexpr const char* PYTHON_COMMAND = "python3";
#endif

// <repo>/out/ab/<variant>: out/ is not tracked by git.
fs::path VariantFolder(const fs::path& repoRoot, AssetVariant variant)
{
    return repoRoot / "out" / "ab" / VariantName(variant);
}

bool ReadManifest(const fs::path& file, json& out)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return false;
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    out = json::parse(text, nullptr, false);
    return !out.is_discarded() && out.is_object() && out.value("schema", "") == MANIFEST_SCHEMA;
}
} // namespace

const char* VariantName(AssetVariant variant)
{
    switch (variant)
    {
    case AssetVariant::Original:
        return "original";
    case AssetVariant::AsBuilt:
        return "as built";
    case AssetVariant::Candidate:
        return "candidate";
    case AssetVariant::Current:
        break;
    }
    return "current";
}

fs::path VariantDataRoot(const fs::path& repoRoot, AssetVariant variant)
{
    switch (variant)
    {
    case AssetVariant::Current:
        return repoRoot / "src" / "bin" / DATA_DIR;
    case AssetVariant::Original:
        return VariantFolder(repoRoot, variant) / DATA_DIR;
    case AssetVariant::AsBuilt:
    case AssetVariant::Candidate:
        break;
    }
    return {};
}

std::optional<fs::path> VariantFile(const fs::path& repoRoot, AssetVariant variant, const std::string& catalogPath)
{
    const std::string_view path = catalogPath;
    if (path.size() <= CATALOG_DATA_PREFIX.size() || path.substr(0, CATALOG_DATA_PREFIX.size()) != CATALOG_DATA_PREFIX)
        return std::nullopt;
    const std::string_view inside = path.substr(CATALOG_DATA_PREFIX.size());
    if (inside.find("..") != std::string_view::npos)
        return std::nullopt;
    fs::path file = VariantDataRoot(repoRoot, variant) / Editor::Text::Utf8Path(inside);
    return file.make_preferred();
}

std::string MaterializeCommand(const fs::path& repoRoot, AssetVariant variant, int world)
{
    const fs::path script = (repoRoot / Editor::Text::Utf8Path(MATERIALIZE_SCRIPT)).make_preferred();
    return std::string(PYTHON_COMMAND) + " \"" + Editor::Text::PathToUtf8(script) + "\" " + VariantName(variant) +
           " --world " + std::to_string(world);
}

std::string MaterializeItemsCommand(const fs::path& repoRoot)
{
    const fs::path script = (repoRoot / Editor::Text::Utf8Path(MATERIALIZE_SCRIPT)).make_preferred();
    return std::string(PYTHON_COMMAND) + " \"" + Editor::Text::PathToUtf8(script) + "\" " +
           VariantName(AssetVariant::Original) + ITEMS_OPTION;
}

VariantState CheckVariant(const fs::path& repoRoot, AssetVariant variant, int world)
{
    if (variant == AssetVariant::Current)
        return VariantState::Ready;
    json manifest;
    if (!ReadManifest(VariantFolder(repoRoot, variant) / MANIFEST_FILE_NAME, manifest))
        return VariantState::Missing;
    if (manifest.value("world", 0) != world)
        return VariantState::Missing;
    const std::string catalogDigest = Editor::Files::Sha256Hex(CatalogFile(repoRoot, world));
    if (catalogDigest.empty() || manifest.value("catalog_sha256", "") != catalogDigest)
        return VariantState::OutOfDate;
    return VariantState::Ready;
}

VariantState CheckItemOriginals(const fs::path& repoRoot, const std::vector<std::pair<std::string, std::string>>& originals)
{
    json manifest;
    if (!ReadManifest(VariantFolder(repoRoot, AssetVariant::Original) / ITEMS_MANIFEST_FILE_NAME, manifest))
        return VariantState::Missing;
    const json& models = manifest.contains("models") ? manifest["models"] : json::object();
    for (const auto& [catalogPath, sha256] : originals)
    {
        const std::string_view path = catalogPath;
        if (path.substr(0, CATALOG_DATA_PREFIX.size()) != CATALOG_DATA_PREFIX)
            continue;
        const std::string key = std::string(DATA_DIR) + "/" + std::string(path.substr(CATALOG_DATA_PREFIX.size()));
        const auto model = models.find(key);
        if (model == models.end() || !model->is_object() || model->value("sha256", "") != sha256)
            return VariantState::OutOfDate;
    }
    return VariantState::Ready;
}
} // namespace Editor::Assets

#endif // _EDITOR
