#pragma once

#ifdef _EDITOR

#include "Assets/ItemBrowse.h"
#include "Assets/ItemCatalog.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// Where the Browse tab's rows come from: the running client's item table
// (ItemAttribute, with the Stats table's edits), its loaded models (Models[]),
// the item catalog and the model files in the checkout.
namespace Editor::ItemEditor
{
// The Models[] slot of an item: MODEL_ITEM + group * 512 + index.
int ModelTypeOf(int itemType);

// One row per named item in the client table, plus the catalog's models without
// a table row. `catalog` may be null (no checkout or no catalog.json).
std::vector<Items::BrowseRow> BuildBrowseRows(const Assets::ItemCatalog* catalog);

// SHA-256 of the checkout's files by repository path ("src/bin/Data/Item/Sword01.bmd").
using DigestCache = std::unordered_map<std::string, std::string>;

// Sets each row's status from the catalog and the files under `repoRoot`,
// hashing a file only the first time `digests` sees it.
void ApplyStatuses(std::vector<Items::BrowseRow>& rows, const std::filesystem::path& repoRoot, DigestCache& digests);
} // namespace Editor::ItemEditor

#endif // _EDITOR
