#pragma once

#ifdef _EDITOR

#include "Assets/ItemBrowse.h"

#include <string>

// The right-hand panel of the Item Editor's Browse tab: the selected item's 3D
// preview (ItemPreview) and everything known about it.
namespace Editor::ItemEditor
{
// `catalogNote` says why catalog facts are missing (no checkout, no catalog, the
// item is not in it); empty when the row has its catalog entry. `filterClass` and
// `filterStage` are the Browse class filter; the preview dresses that class.
// `catalog` (may be null) gives Ask Codex the other parts of an armour set.
void RenderItemDetails(const Items::BrowseRow& row, const std::string& catalogNote, int filterClass,
                       int filterStage, const Assets::ItemCatalog* catalog);
} // namespace Editor::ItemEditor

#endif // _EDITOR
