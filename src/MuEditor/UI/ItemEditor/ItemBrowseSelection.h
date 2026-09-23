#pragma once

#ifdef _EDITOR

#include "Assets/ItemBrowse.h"
#include "Editing/ItemSelection.h"

#include <vector>

// The bar above Browse's list or grid that works on the selected items: how many
// are selected, a chip per item (click to take it out), Select all shown, Clear,
// and "Generate concepts (N)...", which opens the concepts dialog for them.
namespace Editor::ItemEditor::BrowseSelection
{
// `rows` is sorted by item type (BuildBrowseRows); `shownTypes` is the list's order.
// `selectedType` is the Item Editor's primary item, kept in step with the selection.
void RenderBar(Editor::Editing::ItemSelection& selection, const std::vector<Items::BrowseRow>& rows,
               const std::vector<int>& shownTypes, int& selectedType);

// The row of `type` in `rows` (sorted by type), or null.
const Items::BrowseRow* FindRow(const std::vector<Items::BrowseRow>& rows, int type);
} // namespace Editor::ItemEditor::BrowseSelection

#endif // _EDITOR
