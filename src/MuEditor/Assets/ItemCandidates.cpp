#include "ItemCandidates.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "FileDigest.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>

namespace fs = std::filesystem;

namespace Editor::Assets
{
namespace
{
constexpr std::string_view CATALOG_DATA_PREFIX = "src/bin/Data/";
constexpr std::string_view MODEL_CONSTANT_PREFIX = "MODEL_";
constexpr const char* ITEM_ROLE = "item";
constexpr const char* DELIVERY_DIR = "delivery";
constexpr const char* EXPORTS_DIR = "exports";
constexpr const char* BEFORE_DIR = "original";
constexpr const char* PILOT_DIR = "pilot";
constexpr const char* REQUESTS_DIR = "requests";

std::vector<fs::path> SubFolders(const fs::path& folder)
{
    std::vector<fs::path> folders;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, ec))
    {
        if (entry.is_directory(ec))
            folders.push_back(entry.path());
    }
    std::sort(folders.begin(), folders.end());
    return folders;
}

// The file `name` in `folder`, matched without regard to case as the client does.
fs::path FindFileIgnoringCase(const fs::path& folder, const std::string& name)
{
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, ec))
    {
        if (entry.is_regular_file(ec) && Text::EqualIgnoringCase(Text::PathToUtf8(entry.path().filename()), name))
            return entry.path();
    }
    return {};
}

std::string FileNameOf(const std::string& catalogPath)
{
    const std::size_t slash = catalogPath.rfind('/');
    return slash == std::string::npos ? catalogPath : catalogPath.substr(slash + 1);
}

// Inside src/bin/Data: "Item/Sword01.bmd"; nullopt for any other path.
std::optional<std::string> InsideData(const std::string& catalogPath)
{
    const std::string_view path = catalogPath;
    if (path.size() <= CATALOG_DATA_PREFIX.size() || path.substr(0, CATALOG_DATA_PREFIX.size()) != CATALOG_DATA_PREFIX)
        return std::nullopt;
    const std::string_view inside = path.substr(CATALOG_DATA_PREFIX.size());
    if (inside.find("..") != std::string_view::npos)
        return std::nullopt;
    return std::string(inside);
}

// The game file names of every model of `item`: model files and texture files.
std::vector<std::string> ItemFileNames(const ItemCatalogEntry& item)
{
    std::vector<std::string> names;
    for (const ItemModel& model : item.models)
    {
        names.push_back(FileNameOf(model.bmd));
        for (const ItemTexture& texture : model.textures)
        {
            if (!texture.container.empty())
                names.push_back(FileNameOf(texture.container));
        }
    }
    return names;
}

bool HoldsItemFiles(const fs::path& folder, const ItemCatalogEntry& item)
{
    const std::vector<std::string> names = ItemFileNames(item);
    return std::any_of(names.begin(), names.end(),
                       [&](const std::string& name) { return !FindFileIgnoringCase(folder, name).empty(); });
}

void AddDeliveries(const fs::path& repoRoot, const ItemCatalogEntry& item, std::vector<ItemCandidate>& out)
{
    for (const fs::path& request : SubFolders(ItemAssetsDir(repoRoot) / REQUESTS_DIR))
    {
        const std::string id = Text::PathToUtf8(request.filename());
        const fs::path delivery = request / DELIVERY_DIR / Text::Utf8Path(item.key);
        const fs::path exports = delivery / EXPORTS_DIR;
        if (HoldsItemFiles(exports, item))
            out.push_back({"delivery:" + id, "delivery " + id, CandidateSource::Delivery, exports, id});
        const fs::path before = delivery / BEFORE_DIR;
        if (HoldsItemFiles(before, item))
            out.push_back({"before:" + id, "before " + id, CandidateSource::DeliveryBefore, before, id});
    }
}

void AddPilotVariants(const fs::path& repoRoot, const ItemCatalogEntry& item, std::vector<ItemCandidate>& out)
{
    const auto own = std::find_if(item.models.begin(), item.models.end(),
                                  [](const ItemModel& model) { return model.role == ITEM_ROLE; });
    if (own == item.models.end())
        return;
    const std::string stem = Text::PathToUtf8(Text::Utf8Path(FileNameOf(own->bmd)).stem());
    for (const fs::path& model : SubFolders(ItemAssetsDir(repoRoot) / PILOT_DIR))
    {
        const std::string modelName = Text::PathToUtf8(model.filename());
        if (!Text::EqualIgnoringCase(modelName, stem))
            continue;
        for (const fs::path& variant : SubFolders(model))
        {
            if (!HoldsItemFiles(variant, item))
                continue;
            const std::string variantName = Text::PathToUtf8(variant.filename());
            out.push_back({"pilot:" + modelName + "/" + variantName, "pilot " + variantName, CandidateSource::Pilot,
                           variant, ""});
        }
    }
}

bool SameBytes(const fs::path& a, const fs::path& b)
{
    const std::string digest = Files::Sha256Hex(a);
    return !digest.empty() && digest == Files::Sha256Hex(b);
}

// Compares the candidate's copy of `catalogPath`, if it has one; counts it in `compared`.
bool MatchesIfPresent(const fs::path& currentDataRoot, const std::string& catalogPath, const fs::path& candidateFolder,
                      int& compared)
{
    const std::optional<std::string> inside = InsideData(catalogPath);
    const fs::path candidate = FindFileIgnoringCase(candidateFolder, FileNameOf(catalogPath));
    if (!inside || candidate.empty())
        return true;
    ++compared;
    return SameBytes(candidate, currentDataRoot / Text::Utf8Path(*inside));
}
} // namespace

std::vector<ItemCandidate> FindItemCandidates(const fs::path& repoRoot, const ItemCatalogEntry& item)
{
    std::vector<ItemCandidate> candidates;
    if (repoRoot.empty())
        return candidates;
    AddDeliveries(repoRoot, item, candidates);
    AddPilotVariants(repoRoot, item, candidates);
    return candidates;
}

std::optional<ItemCandidate> CandidateFromFolder(const fs::path& folder, const ItemCatalogEntry& item)
{
    if (!HoldsItemFiles(folder, item))
        return std::nullopt;
    const std::string path = Text::PathToUtf8(folder);
    return ItemCandidate{"folder:" + path, "folder " + Text::PathToUtf8(folder.filename()), CandidateSource::Folder,
                         folder, ""};
}

std::optional<ModelFiles> ModelFilesIn(const fs::path& dataRoot, const ItemModel& model)
{
    const std::optional<std::string> bmd = InsideData(model.bmd);
    if (!bmd)
        return std::nullopt;
    ModelFiles files;
    files.bmd = (dataRoot / Text::Utf8Path(*bmd)).make_preferred();
    for (const ItemTexture& texture : model.textures)
    {
        const std::optional<std::string> container = InsideData(texture.container);
        if (!container)
            continue;
        const fs::path folder = (dataRoot / Text::Utf8Path(*container)).parent_path();
        if (std::find(files.textureFolders.begin(), files.textureFolders.end(), folder) == files.textureFolders.end())
            files.textureFolders.push_back(folder);
    }
    const fs::path own = files.bmd.parent_path();
    if (std::find(files.textureFolders.begin(), files.textureFolders.end(), own) == files.textureFolders.end())
        files.textureFolders.push_back(own);
    return files;
}

std::optional<ModelFiles> CandidateModelFiles(const fs::path& currentDataRoot, const ItemModel& model,
                                              const fs::path& candidateFolder)
{
    std::optional<ModelFiles> files = ModelFilesIn(currentDataRoot, model);
    if (!files)
        return std::nullopt;
    const fs::path candidateModel = FindFileIgnoringCase(candidateFolder, FileNameOf(model.bmd));
    if (!candidateModel.empty())
        files->bmd = candidateModel;
    files->textureFolders.insert(files->textureFolders.begin(), candidateFolder);
    return files;
}

std::vector<std::string> ReplacedTextures(const ItemCatalogEntry& item, const fs::path& candidateFolder)
{
    std::set<std::string> replaced;
    for (const ItemModel& model : item.models)
    {
        for (const ItemTexture& texture : model.textures)
        {
            if (!texture.container.empty() && !FindFileIgnoringCase(candidateFolder, FileNameOf(texture.container)).empty())
                replaced.insert(texture.container);
        }
    }
    return {replaced.begin(), replaced.end()};
}

std::vector<std::string> TexturePartners(const ItemCatalogEntry& item, const std::vector<std::string>& textures)
{
    std::set<std::string> partners;
    for (const std::string& texture : textures)
    {
        const auto shared = item.sharedWith.find(texture);
        if (shared == item.sharedWith.end())
            continue;
        for (const std::string& partner : shared->second)
        {
            if (partner != item.key)
                partners.insert(partner);
        }
    }
    return {partners.begin(), partners.end()};
}

bool CandidateMatchesCurrent(const fs::path& currentDataRoot, const ItemCatalogEntry& item, const fs::path& candidateFolder)
{
    int compared = 0;
    for (const ItemModel& model : item.models)
    {
        if (!MatchesIfPresent(currentDataRoot, model.bmd, candidateFolder, compared))
            return false;
        for (const ItemTexture& texture : model.textures)
        {
            if (!texture.container.empty() && !MatchesIfPresent(currentDataRoot, texture.container, candidateFolder, compared))
                return false;
        }
    }
    return compared > 0;
}

std::optional<std::pair<std::string, int>> ParseModelConstant(const std::string& text)
{
    const std::size_t plus = text.find('+');
    const std::string name = Text::Trim(text.substr(0, plus));
    const bool nameOk = name.size() > MODEL_CONSTANT_PREFIX.size() &&
                        std::string_view(name).substr(0, MODEL_CONSTANT_PREFIX.size()) == MODEL_CONSTANT_PREFIX &&
                        std::all_of(name.begin(), name.end(), [](unsigned char c) {
                            return std::isupper(c) || std::isdigit(c) || c == '_';
                        });
    if (!nameOk)
        return std::nullopt;
    if (plus == std::string::npos)
        return std::make_pair(name, 0);
    const std::string offset = Text::Trim(text.substr(plus + 1));
    if (offset.empty() || !std::all_of(offset.begin(), offset.end(), [](unsigned char c) { return std::isdigit(c); }))
        return std::nullopt;
    return std::make_pair(name, std::stoi(offset));
}
} // namespace Editor::Assets

#endif // _EDITOR
