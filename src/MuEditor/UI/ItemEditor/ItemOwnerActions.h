#pragma once

#ifdef _EDITOR

#include "Assets/ItemCatalog.h"

#include <string>

// The owner's part of the Browse details panel: the quick verdict on the item as
// the client draws it (Looks good / Needs work, kept in
// assets-work/Items/client-review.json, which the catalog builder folds in) and
// Ask Codex... (the item request dialog), plus the item's live requests.
namespace Editor::ItemEditor
{
void RenderOwnerActions(const Assets::ItemCatalogEntry& item, const Assets::ItemCatalog& catalog);
} // namespace Editor::ItemEditor

#endif // _EDITOR
