#pragma once

#ifdef _EDITOR

#include <string>
#include <vector>
#include <map>

// Forward declaration
class CItemEditorColumns;

// Handles the main table rendering for the Item Editor
// Delegates column rendering to CItemEditorColumns
class CItemEditorTable
{
public:
    CItemEditorTable();
    ~CItemEditorTable();

    // Main render function. `searchFilter` is folded with Editor::Text::FoldCase.
    void Render(const std::string& searchFilter,
                std::map<std::string, bool>& columnVisibility,
                int& selectedRow,
                bool freezeColumns);

    // Request scroll to a specific item index
    static void RequestScrollToIndex(int index);

    // Force rebuild of filtered list (call after data changes)
    void InvalidateFilter();

private:
    // The filtered row of a pending RequestScrollToIndex() (which it selects), or -1.
    int TakeScrollRequest(int& selectedRow);

    // Filter state
    std::vector<int> m_filteredItems;
    std::string m_lastSearchFilter;
    bool m_isInitialized;

    // Column renderer
    CItemEditorColumns* m_pColumns;

    // Scroll request state
    static int s_scrollToIndex;
};

#endif // _EDITOR
