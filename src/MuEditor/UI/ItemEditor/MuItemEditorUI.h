#pragma once

#ifdef _EDITOR

#include <map>
#include <memory>
#include <string>

// Forward declarations
class CItemBrowseTab;
class CItemEditorTable;

class CMuItemEditorUI
{
public:
    static CMuItemEditorUI& GetInstance();

    void Render(bool& showEditor);
    void ClearSearch() { m_szItemSearchBuffer[0] = '\0'; }
    void SaveColumnPreferences();
    void LoadColumnPreferences();

private:
    CMuItemEditorUI();
    ~CMuItemEditorUI();

    // Where the window goes this frame; returns the window flags. In the item
    // studio (ItemStudio.h) it fills the client area under the toolbar.
    int PlaceWindow(bool docked);
    void KeepWindowBetweenToolbarAndConsole();
    void RenderTabs();
    void RenderStatsTable();
    void RenderSearchBar();
    void RenderColumnVisibilityMenu();

    enum class Tab
    {
        None,
        Browse,
        StatsTable,
    };

    char m_szItemSearchBuffer[256];

    // Column visibility state (cached from config)
    std::map<std::string, bool> m_columnVisibility;

    // The selected item (its type), shared by the Browse and Stats table tabs; -1 = none.
    int m_selectedRow;
    Tab m_activeTab;
    bool m_wasDocked;

    // Column freezing state
    bool m_bFreezeColumns;

    // Table renderer
    CItemEditorTable* m_pTable;
    std::unique_ptr<CItemBrowseTab> m_pBrowse;
};

#define g_MuItemEditorUI CMuItemEditorUI::GetInstance()

#endif // _EDITOR
