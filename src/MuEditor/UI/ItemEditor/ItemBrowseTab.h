#pragma once

#ifdef _EDITOR

#include "ItemBrowseSource.h"

#include "Assets/ItemBrowse.h"
#include "Assets/ItemCatalog.h"

#include <map>
#include <string>
#include <vector>

// The Item Editor's Browse tab: every item as a list or a thumbnail grid,
// filtered by class and stage (the engine's equip rule), family, tier, status
// and name, sorted basic -> rare or by another key, with the selected item's
// facts on the right. The selection is shared with the Stats table tab.
class CItemBrowseTab
{
public:
    // Draws the tab. `selectedType` is the Item Editor's selected item (-1: none).
    void Render(int& selectedType);

    // The tab was switched to: re-read the client table (the Stats table may have
    // changed names or classes) and scroll to the selected item.
    void OnShown();

private:
    void EnsureCatalog();
    void RebuildRows();
    void RefreshStatuses();
    void UpdateShown();
    std::string CatalogNote() const;
    const Editor::Items::BrowseRow* RowOfType(int type) const;

    void RenderFilterPanel();
    void RenderClassFilter();
    void RenderStageChoice();
    void RenderTierAndStatusFilter();
    void RenderFamilyList();
    void RenderResultBar();
    void RenderList(int& selectedType);
    void RenderListRow(const Editor::Items::BrowseRow& row, int& selectedType, float thumbSize);
    void RenderGrid(int& selectedType);
    void RenderGridTile(const Editor::Items::BrowseRow& row, int& selectedType, float tileSize);
    void RenderDetailsPanel(int selectedType);
    void Select(int type, int& selectedType);

    bool m_catalogLoaded = false;
    Editor::Assets::ItemCatalogLoad m_load;
    std::string m_repoNote; // why there is no catalog path (no checkout found)
    std::vector<std::string> m_families; // in item group order

    std::vector<Editor::Items::BrowseRow> m_rows;
    std::vector<int> m_shown; // indices into m_rows
    std::map<std::string, int> m_familyCounts;
    Editor::ItemEditor::DigestCache m_digests;
    bool m_rowsBuilt = false;
    bool m_shownDirty = true;

    Editor::Items::BrowseFilter m_filter;
    Editor::Items::SortOrder m_order;
    Editor::Items::BrowseFilter m_shownFilter;
    Editor::Items::SortOrder m_shownOrder;
    char m_search[128] = {};
    int m_statusChoice = 0; // index into the status choices (0 = any)
    bool m_gridView = false;

    int m_lastSelected = -1;
    bool m_scrollToSelected = false;
};

#endif // _EDITOR
