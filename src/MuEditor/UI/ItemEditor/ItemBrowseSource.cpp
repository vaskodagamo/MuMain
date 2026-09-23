#include "stdafx.h"

#ifdef _EDITOR

#include "ItemBrowseSource.h"

#include "Assets/EditorText.h"
#include "Assets/FileDigest.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Models/ZzzBMD.h" // Models

#include <algorithm>

extern ITEM_ATTRIBUTE* ItemAttribute;

namespace Editor::ItemEditor
{
namespace
{
bool HasLoadedModel(int itemType)
{
    const BMD& model = Models[ModelTypeOf(itemType)];
    return model.NumMeshs > 0 && model.Meshs != nullptr;
}

Items::TableFacts FactsOf(int itemType)
{
    const ITEM_ATTRIBUTE& attribute = ItemAttribute[itemType];
    Items::TableFacts facts;
    facts.type = itemType;
    facts.name = StringUtils::WideToNarrow(attribute.Name);
    std::copy(std::begin(attribute.RequireClass), std::end(attribute.RequireClass), facts.requireClass.begin());
    facts.dropLevel = attribute.Level;
    facts.requireLevel = attribute.RequireLevel;
    facts.hasModel = HasLoadedModel(itemType);
    return facts;
}

const std::string& DigestOf(const std::filesystem::path& repoRoot, const std::string& repoPath, DigestCache& digests)
{
    const auto known = digests.find(repoPath);
    if (known != digests.end())
        return known->second;
    const std::string digest = Editor::Files::Sha256Hex(repoRoot / Editor::Text::Utf8Path(repoPath));
    return digests.emplace(repoPath, digest).first->second;
}
} // namespace

int ModelTypeOf(int itemType)
{
    return MODEL_ITEM + itemType;
}

std::vector<Items::BrowseRow> BuildBrowseRows(const Assets::ItemCatalog* catalog)
{
    std::vector<Items::BrowseRow> rows;
    if (ItemAttribute == nullptr)
        return rows;
    for (int type = 0; type < MAX_ITEM; ++type)
    {
        const Assets::ItemCatalogEntry* entry = catalog != nullptr ? catalog->FindByType(type) : nullptr;
        if (ItemAttribute[type].Name[0] == L'\0' && entry == nullptr)
            continue;
        rows.push_back(Items::MakeRow(FactsOf(type), entry));
    }
    return rows;
}

void ApplyStatuses(std::vector<Items::BrowseRow>& rows, const std::filesystem::path& repoRoot, DigestCache& digests)
{
    const auto currentSha256 = [&](const std::string& bmd) -> std::string
    { return DigestOf(repoRoot, bmd, digests); };
    for (Items::BrowseRow& row : rows)
    {
        if (row.catalog == nullptr || repoRoot.empty())
            continue;
        row.status = Items::ComputeStatus(*row.catalog, currentSha256);
    }
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
