#pragma once

#ifdef _EDITOR

#include "Assets/ItemBrowse.h"

#include <string>

// The right-hand panel of the Item Editor's Browse tab: everything known about
// the selected item, with the area where the 3D preview will go.
namespace Editor::ItemEditor
{
// `catalogNote` says why catalog facts are missing (no checkout, no catalog, the
// item is not in it); empty when the row has its catalog entry.
void RenderItemDetails(const Items::BrowseRow& row, const std::string& catalogNote);
} // namespace Editor::ItemEditor

#endif // _EDITOR
