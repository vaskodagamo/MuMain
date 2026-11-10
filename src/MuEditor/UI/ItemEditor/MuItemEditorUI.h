#pragma once

#ifdef _EDITOR

#include <memory>

// Forward declarations
class CItemBrowseTab;
class CItemRequestsTab;

// The fork's Item Editor: Browse (every item with its 3D preview, A/B compare,
// concepts and Ask Codex) and Requests. The item data itself (names, stats,
// requirements, rules, models) is edited in upstream's item tools (Windows >
// Items), which share one selected item; this window keeps its selection in step
// with theirs, and its Item data tab opens them.
class CMuItemEditorUI
{
public:
    static CMuItemEditorUI& GetInstance();

    void Render(bool& showEditor);

private:
    CMuItemEditorUI();
    ~CMuItemEditorUI();

    // Begins the window: in the item studio (ItemStudio.h) it fills the game
    // window, else it is a tool window of the editor like the others.
    bool BeginWindow(bool docked, bool& showEditor);
    // Takes over an item the item tools selected, and gives them the one picked here.
    void FollowItemToolsSelection();
    void ShareSelectionWithItemTools();
    void RenderTabs();
    void RenderItemDataTab();

    enum class Tab
    {
        None,
        Browse,
        ItemData,
        Requests,
    };

    // The selected item (its type), shared by the tabs and the item tools; -1 = none.
    int m_selectedRow;
    // What the item tools' selection was when it was last compared.
    int m_itemToolsSerial;
    int m_sharedSelection;
    Tab m_activeTab;
    bool m_showBrowse; // switch to the Browse tab next frame (asked by the Requests tab)

    std::unique_ptr<CItemBrowseTab> m_pBrowse;
    std::unique_ptr<CItemRequestsTab> m_pRequests;
};

#define g_MuItemEditorUI CMuItemEditorUI::GetInstance()

#endif // _EDITOR
