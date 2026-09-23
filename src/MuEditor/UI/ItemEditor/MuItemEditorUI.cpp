#include "stdafx.h"

#ifdef _EDITOR

#include "MuItemEditorUI.h"
#include "ConceptGenerateDialog.h"
#include "ConceptLibrary.h"
#include "ConceptRefineDialog.h"
#include "ConceptsPanel.h"
#include "ItemBrowseTab.h"
#include "ItemEditorTable.h"
#include "ItemRequestDialog.h"
#include "ItemRequestWatch.h"
#include "ItemRequestsTab.h"
#include "ItemEditorActions.h"
#include "ItemEditorPopups.h"
#include "Data/GameData/ItemData/ItemFieldMetadata.h"
#include "../MuEditor/Config/MuEditorConfig.h"
#include "../MuEditor/Core/ItemStudio.h"
#include "../MuEditor/Core/MuEditorCore.h"
#include "../MuEditor/UI/Common/MuEditorUI.h"
#include "I18N/All.h"
#include "Assets/EditorText.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#ifdef _WIN32
#include <direct.h>
#endif
#include <filesystem>

#include "imgui_internal.h"
#include "../MuEditor/UI/Console/MuEditorConsoleUI.h"

namespace
{
// The floating window's size (first use, and when it leaves the studio's full-window layout).
constexpr float FLOATING_WIDTH = 1500.0f;
constexpr float FLOATING_HEIGHT = 760.0f;
constexpr float FLOATING_MARGIN = 20.0f;
constexpr float MIN_WIDTH = 400.0f;
constexpr float MIN_HEIGHT = 300.0f;
} // namespace

CMuItemEditorUI::CMuItemEditorUI()
    : m_selectedRow(-1)
    , m_activeTab(Tab::None)
    , m_showBrowse(false)
    , m_wasDocked(false)
    , m_bFreezeColumns(false)
    , m_pTable(nullptr)
    , m_pBrowse(std::make_unique<CItemBrowseTab>())
    , m_pRequests(std::make_unique<CItemRequestsTab>())
{
    memset(m_szItemSearchBuffer, 0, sizeof(m_szItemSearchBuffer));
    m_pTable = new CItemEditorTable();

    // Initialize column visibility - AUTO-GENERATED from metadata
    m_columnVisibility["Index"] = true;  // Special column (not in metadata)

    // Get all fields from metadata and set default visibility
    const ItemFieldDescriptor* fields = GetFieldDescriptors(); const int fieldCount = GetFieldCount();
    for (int i = 0; i < fieldCount; ++i)
    {
        // Default commonly used columns to visible, rest to hidden
        bool defaultVisible = false;

        // Check for commonly used fields
        if (strcmp(fields[i].name, "Name") == 0 ||
            strcmp(fields[i].name, "Level") == 0 ||
            strcmp(fields[i].name, "DamageMin") == 0 ||
            strcmp(fields[i].name, "DamageMax") == 0 ||
            strcmp(fields[i].name, "Defense") == 0 ||
            strcmp(fields[i].name, "WeaponSpeed") == 0 ||
            strcmp(fields[i].name, "Durability") == 0 ||
            strcmp(fields[i].name, "RequireStrength") == 0 ||
            strcmp(fields[i].name, "RequireDexterity") == 0 ||
            strcmp(fields[i].name, "RequireEnergy") == 0 ||
            strcmp(fields[i].name, "RequireVitality") == 0 ||
            strcmp(fields[i].name, "RequireCharisma") == 0)
        {
            defaultVisible = true;
        }

        m_columnVisibility[fields[i].name] = defaultVisible;
    }

    // Load column preferences from file (will override defaults if file exists)
    LoadColumnPreferences();
}

CMuItemEditorUI::~CMuItemEditorUI()
{
    // Save column preferences when destroying
    SaveColumnPreferences();

    if (m_pTable)
    {
        delete m_pTable;
        m_pTable = nullptr;
    }
}

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
    const ImGuiWindowFlags flags = PlaceWindow(docked);
    if (ImGui::Begin(I18N::Editor::ItemEditor, &showEditor, flags))
    {
        if (!docked)
            KeepWindowBetweenToolbarAndConsole();

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
        RenderTabs();
        // After the tabs: a running capture reads the preview Browse drew this frame.
        g_ItemRequestDialog.Render();
        g_ConceptsPanel.RenderPopups();
        g_ConceptGenerateDialog.Render();
        g_ConceptRefineDialog.Render();

        // Render all popups
        CItemEditorPopups::RenderAll();
    }
    ImGui::End();
}

int CMuItemEditorUI::PlaceWindow(bool docked)
{
    ImGuiIO& io = ImGui::GetIO();
    const float top = g_MuEditorUI.ToolbarHeight();
    const float consoleHeight = g_MuEditorCore.IsShowingConsole() ? CMuEditorConsoleUI::HEIGHT : 0.0f;
    const float bottom = io.DisplaySize.y - consoleHeight;
    const bool leftStudio = m_wasDocked && !docked;
    m_wasDocked = docked;

    if (docked)
    {
        // Fills the client area under the toolbar and follows window resizes; the
        // other editors' windows open on top of it.
        ImGui::SetNextWindowPos(ImVec2(0.0f, top), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, bottom - top), ImGuiCond_Always);
        return ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
               ImGuiWindowFlags_NoBringToFrontOnFocus;
    }

    // A window the map shows around: the first time, and when the Map Editor opens in the studio.
    const ImGuiCond placement = leftStudio ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(FLOATING_MARGIN, top + FLOATING_MARGIN), placement);
    ImGui::SetNextWindowSize(ImVec2(FLOATING_WIDTH, FLOATING_HEIGHT), placement);
    ImGui::SetNextWindowSizeConstraints(ImVec2(MIN_WIDTH, MIN_HEIGHT), ImVec2(io.DisplaySize.x, bottom - top));
    return ImGuiWindowFlags_NoCollapse;
}

void CMuItemEditorUI::KeepWindowBetweenToolbarAndConsole()
{
    ImGuiIO& io = ImGui::GetIO();
    const float availableTop = g_MuEditorUI.ToolbarHeight();
    const float availableBottom = io.DisplaySize.y - CMuEditorConsoleUI::HEIGHT;
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();

    if (windowPos.y < availableTop)
        ImGui::SetWindowPos(ImVec2(windowPos.x, availableTop));
    if (windowPos.y + windowSize.y > availableBottom)
        ImGui::SetWindowPos(ImVec2(windowPos.x, availableBottom - windowSize.y));
    if (windowPos.x < 0)
        ImGui::SetWindowPos(ImVec2(0, windowPos.y));
    if (windowPos.x + windowSize.x > io.DisplaySize.x)
        ImGui::SetWindowPos(ImVec2(io.DisplaySize.x - windowSize.x, windowPos.y));
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
    if (ImGui::BeginTabItem("Stats table"))
    {
        if (m_activeTab != Tab::StatsTable && m_selectedRow >= 0)
            CItemEditorTable::RequestScrollToIndex(m_selectedRow); // the item picked in Browse
        m_activeTab = Tab::StatsTable;
        RenderStatsTable();
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

void CMuItemEditorUI::RenderStatsTable()
{
    // Render action buttons (Save, Export S6E3, Export CSV)
    CItemEditorActions::RenderAllButtons();
    ImGui::Separator();

    RenderSearchBar();
    ImGui::SameLine();
    RenderColumnVisibilityMenu();
    ImGui::SameLine();
    ImGui::Checkbox(I18N::Editor::FreezeIndexName, &m_bFreezeColumns);
    ImGui::Separator();

    // Any letter case, also outside A-Z (Editor::Text::FoldCase).
    const std::string searchFolded = Editor::Text::FoldCase(m_szItemSearchBuffer);
    if (m_pTable)
    {
        m_pTable->Render(searchFolded, m_columnVisibility, m_selectedRow, m_bFreezeColumns);
    }
}

void CMuItemEditorUI::RenderSearchBar()
{
    // Search bar
    ImGui::Text(I18N::Editor::Search);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    ImGui::InputText("##ItemSearch", m_szItemSearchBuffer, sizeof(m_szItemSearchBuffer));
}

void CMuItemEditorUI::RenderColumnVisibilityMenu()
{
    if (ImGui::Button(I18N::Editor::Columns))
    {
        ImGui::OpenPopup("ColumnVisibility");
    }

    // Track if popup was open in previous frame
    static bool wasPopupOpen = false;
    bool isPopupOpen = ImGui::IsPopupOpen("ColumnVisibility");

    if (ImGui::BeginPopup("ColumnVisibility"))
    {
        ImGui::Text(I18N::Editor::ToggleColumnVisibility);
        ImGui::Separator();

        // Select All / Unselect All buttons
        if (ImGui::Button(I18N::Editor::SelectAll))
        {
            for (auto& col : m_columnVisibility)
            {
                col.second = true;
            }
            SaveColumnPreferences();
        }
        ImGui::SameLine();
        if (ImGui::Button(I18N::Editor::UnselectAll))
        {
            for (auto& col : m_columnVisibility)
            {
                col.second = false;
            }
            SaveColumnPreferences();
        }
        ImGui::Separator();

        // METADATA-DRIVEN: Render all columns using metadata with translations
        bool changed = false;

        // Index column (special case, not in metadata)
        changed |= ImGui::Checkbox(I18N::Editor::Index, &m_columnVisibility["Index"]);

        // Get all fields from metadata and render checkboxes
        const ItemFieldDescriptor* fields = GetFieldDescriptors(); const int fieldCount = GetFieldCount();
        for (int i = 0; i < fieldCount; ++i)
        {
            const char* displayName = GetFieldDisplayName(fields[i]);
            changed |= ImGui::Checkbox(displayName, &m_columnVisibility[fields[i].name]);
        }

        // Save immediately when any checkbox changes
        if (changed)
        {
            SaveColumnPreferences();
        }

        ImGui::EndPopup();
        wasPopupOpen = true;
    }
    else if (wasPopupOpen)
    {
        // Popup just closed - save one final time
        SaveColumnPreferences();
        wasPopupOpen = false;
    }
}

void CMuItemEditorUI::SaveColumnPreferences()
{
    // Save to unified config system
    g_MuEditorConfig.SetAllColumnVisibility(m_columnVisibility);
    g_MuEditorConfig.Save();
}

void CMuItemEditorUI::LoadColumnPreferences()
{
    // Load from unified config system
    const auto& savedVisibility = g_MuEditorConfig.GetAllColumnVisibility();

    // Only update columns that exist in saved config
    for (const auto& col : savedVisibility)
    {
        // Only update if the column exists in our default map
        if (m_columnVisibility.find(col.first) != m_columnVisibility.end())
        {
            m_columnVisibility[col.first] = col.second;
        }
    }
}

#endif // _EDITOR
