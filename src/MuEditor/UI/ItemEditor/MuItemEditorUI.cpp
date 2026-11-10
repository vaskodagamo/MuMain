#include "stdafx.h"

#ifdef _EDITOR

#include "MuItemEditorUI.h"
#include "ConceptGenerateDialog.h"
#include "ConceptLibrary.h"
#include "ConceptRefineDialog.h"
#include "ConceptsPanel.h"
#include "ItemBrowseTab.h"
#include "ItemEditorActions.h"
#include "ItemEditorPopups.h"
#include "ItemRequestDialog.h"
#include "ItemRequestWatch.h"
#include "ItemRequestsTab.h"
#include "../MuEditor/Core/ItemStudio.h"
#include "../MuEditor/Core/MuEditorCore.h"
#include "../MuEditor/UI/Common/MuEditorLayout.h"
#include "../MuEditor/UI/Common/MuEditorToolWindows.h"
#include "../MuEditor/UI/Common/MuEditorWindowIds.h"
#include "../MuEditor/UI/ItemTools/ItemSelection.h"
#include "../MuEditor/UI/ItemTools/ItemToolWindow.h"
#include "I18N/All.h"
#include "imgui.h"

namespace
{
// The tool window's size the first time it floats.
constexpr float FLOATING_WIDTH = 1500.0f;
constexpr float FLOATING_HEIGHT = 760.0f;
constexpr int NO_ITEM = -1;

struct ItemToolButton
{
    const char* label;
    bool MuEditor::ToolWindows::* open;
};

// The item tools the Item data tab opens, in the order of Windows > Items.
const ItemToolButton ITEM_TOOL_BUTTONS[] = {
    {"Item Browser", &MuEditor::ToolWindows::itemBrowser},
    {"Basics", &MuEditor::ToolWindows::itemBasics},
    {"Requirements", &MuEditor::ToolWindows::itemRequirements},
    {"Rules", &MuEditor::ToolWindows::itemRules},
    {"Categories", &MuEditor::ToolWindows::itemCategories},
    {"Model", &MuEditor::ToolWindows::itemModel},
    {"Translations", &MuEditor::ToolWindows::itemTranslations},
};
} // namespace

CMuItemEditorUI::CMuItemEditorUI()
    : m_selectedRow(NO_ITEM)
    , m_itemToolsSerial(g_ItemSelection.GetSerial())
    , m_sharedSelection(NO_ITEM)
    , m_activeTab(Tab::None)
    , m_showBrowse(false)
    , m_pBrowse(std::make_unique<CItemBrowseTab>())
    , m_pRequests(std::make_unique<CItemRequestsTab>())
{
}

CMuItemEditorUI::~CMuItemEditorUI() = default;

CMuItemEditorUI& CMuItemEditorUI::GetInstance()
{
    static CMuItemEditorUI instance;
    return instance;
}

void CMuItemEditorUI::Render(bool& showEditor)
{
    // Access external item data
    extern ITEM_ATTRIBUTE* ItemAttribute;
    if (!ItemAttribute)
        return;

    const bool docked = Editor::ItemStudio::DocksItemEditor();
    if (BeginWindow(docked, showEditor))
    {
        // Check if hovering this window OR any popup
        bool isHovering = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup) ||
                         ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
                         ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

        if (isHovering)
        {
            g_MuEditorCore.SetHoveringUI(true);
        }

        g_ItemRequestWatch.Update();
        g_ConceptLibrary.Update();
        FollowItemToolsSelection();
        RenderTabs();
        ShareSelectionWithItemTools();
        // After the tabs: a running capture reads the preview Browse drew this frame.
        g_ItemRequestDialog.Render();
        g_ConceptsPanel.RenderPopups();
        g_ConceptGenerateDialog.Render();
        g_ConceptRefineDialog.Render();

        // The popups of the item tools' save and export buttons (Item data tab)
        CItemEditorPopups::RenderAll();
    }
    ImGui::End();
}

bool CMuItemEditorUI::BeginWindow(bool docked, bool& showEditor)
{
    char title[MuEditor::WindowIds::TitleCapacity];
    MuEditor::WindowIds::Title(title, I18N::Editor::ItemEditor, MuEditor::WindowIds::ItemStudio);

    if (docked)
    {
        // The studio: fills the game window and follows its resizes; the editor
        // window with the other tools is beside it. With the editor's windows as OS
        // windows of their own, ImGui places windows on the desktop, so the game
        // window's place comes from its viewport.
        const ImGuiViewport* game = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(game->ID);
        ImGui::SetNextWindowPos(game->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(game->Size, ImGuiCond_Always);
        return ImGui::Begin(title, &showEditor,
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus);
    }

    // A tool window like upstream's: a tab beside the item tools the first time,
    // then wherever it was left (imgui.ini).
    ImGui::SetNextWindowSize(ImVec2(FLOATING_WIDTH, FLOATING_HEIGHT), ImGuiCond_FirstUseEver);
    MuEditor::Layout::DockBesideItemTools(MuEditor::WindowIds::ItemStudio);
    MuEditor::Layout::PrepareToolWindow();
    const bool drawn = ImGui::Begin(title, &showEditor,
                                    ImGuiWindowFlags_NoCollapse |
                                        MuEditor::Layout::ToolWindowBarFlags(MuEditor::WindowIds::ItemStudio));
    if (drawn)
        MuEditor::Layout::RenderToolWindowBar();
    return drawn;
}

void CMuItemEditorUI::FollowItemToolsSelection()
{
    // An item picked in one of the item tools (their browser, a number in another
    // tool) is selected here too, unless it is the one this window gave them.
    if (g_ItemSelection.GetSerial() == m_itemToolsSerial)
        return;
    m_itemToolsSerial = g_ItemSelection.GetSerial();
    const int picked = g_ItemSelection.GetItemType();
    if (picked == MuEditor::Items::ItemSelection::NoItem || picked == m_selectedRow)
        return;
    m_selectedRow = picked;
    m_sharedSelection = picked;
}

void CMuItemEditorUI::ShareSelectionWithItemTools()
{
    if (m_selectedRow == m_sharedSelection)
        return;
    m_sharedSelection = m_selectedRow;
    if (m_selectedRow < 0 || m_selectedRow == g_ItemSelection.GetItemType())
        return;
    g_ItemSelection.Select(m_selectedRow);
    m_itemToolsSerial = g_ItemSelection.GetSerial();
}

void CMuItemEditorUI::RenderTabs()
{
    if (!ImGui::BeginTabBar("ItemEditorTabs"))
        return;
    const ImGuiTabItemFlags browseFlags = m_showBrowse ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    m_showBrowse = false;
    if (ImGui::BeginTabItem("Browse", nullptr, browseFlags))
    {
        if (m_activeTab != Tab::Browse)
            m_pBrowse->OnShown();
        m_activeTab = Tab::Browse;
        m_pBrowse->Render(m_selectedRow);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Item data"))
    {
        m_activeTab = Tab::ItemData;
        RenderItemDataTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Requests"))
    {
        m_activeTab = Tab::Requests;
        m_pRequests->Render(m_selectedRow, m_showBrowse, m_pBrowse->Catalog());
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

void CMuItemEditorUI::RenderItemDataTab()
{
    ImGui::TextWrapped("Names, stats, requirements, rules and models are item data in Data/Items (JSON), edited in "
                       "the item tools of the editor window (Windows > Items). They show the item selected here, "
                       "and an item picked there is selected here.");
    ImGui::Spacing();
    MuEditor::Items::ToolWindow::RenderSelectedItem(m_selectedRow);
    ImGui::Spacing();

    MuEditor::ToolWindows& windows = g_MuEditorCore.GetToolWindows();
    bool first = true;
    for (const ItemToolButton& tool : ITEM_TOOL_BUTTONS)
    {
        if (!first)
            ImGui::SameLine();
        first = false;
        if (ImGui::Button(tool.label))
        {
            windows.*(tool.open) = true;
            if (m_selectedRow >= 0)
            {
                // Selecting it again makes the tools show it (scroll to it, clear a filter).
                g_ItemSelection.Select(m_selectedRow);
                m_itemToolsSerial = g_ItemSelection.GetSerial();
            }
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Opens the item tool in the editor window, on the selected item");

    ImGui::Separator();
    ImGui::TextWrapped("Save Items writes the item data next to the game and copies every file it changed into the "
                       "checkout (src/bin/Data), with a backup of what it replaces in out/editor-backups.");
    CItemEditorActions::RenderSaveButton();
}

#endif // _EDITOR
