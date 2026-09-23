#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorUI.h"
#include "MapEditorSave.h"
#include "MapAttributeSave.h"
#include "MapHeightSave.h"
#include "MapEditorFilePicker.h"
#include "MapEditorFileUtil.h"
#include "MapTextureBrowser.h"
#include "MapObjectPlace.h"
#include "MapObjectBrowser.h"
#include "MapEditorStatusLine.h"
#include "MapEditorShortcuts.h"
#include "MapEditHistory.h"
#include "MapObjectEditor.h"
#include "MapTerrainLayers.h"
#include "MapAssetReview.h"
#include "MapOutliner.h"
#include "RegenRequestDialog.h"
#include "ObjectThumbnail.h"

#include "Render/Textures/ZzzOpenglUtil.h"  // CollisionPosition (world hit point)
#include "Engine/Object/ZzzObject.h"         // ObjectListGeneration (objects freed since)
#include "Engine/Object/w_ObjectInfo.h"      // class OBJECT (transform fields)
#include "Camera/CameraManager.h"            // top-down minimap view
#include "Camera/CameraMode.h"
#include "Camera/CameraState.h"
#include "Camera/FreeFlyCamera.h"
#include "MapMinimapCapture.h"

#include "imgui.h"
#include "Core/Globals/_define.h"          // EDIT_MAPPING / EDIT_NONE
#include "Core/Globals/_TextureIndex.h"    // BITMAP_MAPTILE
#include "Render/Sprites/GlobalBitmap.h"   // Bitmaps[]
#include "Render/Renderer/MuRenderer.h"    // mu::GetRenderer().GetTexturePointer
#include "Render/Terrain/ZzzLodTerrain.h"  // CurrentLayer
#include "World/MapInfra/MapManager.h"     // gMapManager.WorldActive
#include "Core/MuEditorCore.h"             // hover flag for input blocking
#include "Core/LiveMap.h"                  // the saved state of the loaded map
#include "Core/OfflineWorld.h"             // map opened without a server
#include "UI/Console/MuEditorConsoleUI.h"
#include "Core/Utilities/StringUtils.h"    // WideToNarrow (UTF-8 safe, unlike raw truncation)
#include "Editing/ObjectEditCommand.h"
#include "Editing/SurfaceBrush.h"
#include "MapInspect/TilePalette.h"             // tile slot names
#include "Render/Renderer/RenderUtils.h"        // mu::PackABGR
#include "Render/Terrain/TerrainBrushOutline.h" // the round brushes' outline
#include "Render/Terrain/TerrainGroundRects.h"  // the Gates tab's areas
#include "Assets/EditorText.h"                  // EqualIgnoringCase (tab names)

#include <cstring>
#include <filesystem>
#include <iterator>

// Brush/selection globals owned by ZzzInterface.cpp. Declared locally in the
// same style as EditObjects.cpp rather than pulling in a wide interface header.
extern int EditFlag;
extern int SelectMapping;
extern int BrushSize;

// Terrain arrays + picking state. SelectFlag/SelectXF/SelectYF are updated by the
// terrain render pass each frame while EditFlag != EDIT_NONE (the mouse-over-tile
// under the cursor).
extern unsigned char TerrainMappingLayer1[];
extern unsigned char TerrainMappingLayer2[];
extern float         TerrainMappingAlpha[];
extern float         BackTerrainHeight[];
extern bool SelectFlag;

// Tints tiles by their TerrainWall attribute while the Attribute tab is showing.
extern bool g_bMapEditorAttrOverlay;

// Highlights the texture brush's tile footprint on the ground before/while
// painting (ZzzLodTerrain.cpp). Bounds are inclusive tile coordinates.
extern bool g_bMapEditorBrushHighlight;
extern int  g_MapEditorBrushMinX, g_MapEditorBrushMinY;
extern int  g_MapEditorBrushMaxX, g_MapEditorBrushMaxY;

// Object under the cursor in Select & edit mode, before any click (ZzzObject.cpp).
extern OBJECT* g_MapEditorHoveredObject;
// Place-new-mode placement preview (ZzzObject.cpp) - kept in sync every frame
// by PlaceObjects() below, drawn by RenderPlacementPreview().
extern bool  g_MapEditorPlacementPreviewActive;
extern int   g_MapEditorPlacementPreviewType;
extern vec3_t g_MapEditorPlacementPreviewPos;
extern float g_MapEditorPlacementPreviewYaw;
extern float g_MapEditorPlacementPreviewScale;

// Live mouse-button states. We capture and clear these before the game consumes
// them (CaptureInputForPainting) so painting doesn't also move/attack the hero.
extern bool MouseLButton, MouseLButtonPop, MouseLButtonPush, MouseLButtonDBClick;
extern bool MouseRButton, MouseRButtonPop, MouseRButtonPush;

namespace
{
    // Number of terrain tile slots the loader fills (BITMAP_MAPTILE + 0..29):
    // 14 base Tile* slots followed by 16 ExtTile slots. Index N in a mapping
    // layer selects BITMAP_MAPTILE + N.
    constexpr int TILE_SLOT_COUNT = Editor::MapInspect::TILE_SLOT_COUNT;

    // Palette thumbnail size (pixels) and grid width (columns).
    constexpr float TILE_THUMB_SIZE = 56.0f;
    constexpr int   TILE_COLUMNS    = 5;

    // The panel is at least this wide (at 100% UI scale), so every tab label, up to
    // "Assets", fits without the tab bar scrolling.
    constexpr float MIN_PANEL_WIDTH = 665.0f;

    // The Objects tab's modes (m_objMode).
    constexpr int OBJECT_MODE_PLACE = 0;
    constexpr int OBJECT_MODE_SELECT = 1;

    // The tabs, in the order the panel shows them (ShowTab takes these names).
    constexpr const char* TAB_TEXTURE = "Texture";
    constexpr const char* TAB_OBJECTS = "Objects";
    constexpr const char* TAB_HEIGHT = "Height";
    constexpr const char* TAB_ATTRIBUTE = "Attribute";
    constexpr const char* TAB_LIGHT = "Light";
    constexpr const char* TAB_GATES = "Gates";
    constexpr const char* TAB_TEXTURE_BROWSER = "T. Browse";
    constexpr const char* TAB_MINIMAP = "Minimap";
    constexpr const char* TAB_OBJECT_BROWSER = "O. Browse";
    constexpr const char* TAB_ASSETS = "Assets";
    constexpr const char* TAB_NAMES[] = {TAB_TEXTURE,         TAB_OBJECTS, TAB_HEIGHT,         TAB_ATTRIBUTE, TAB_LIGHT,
                                         TAB_GATES, TAB_TEXTURE_BROWSER, TAB_MINIMAP, TAB_OBJECT_BROWSER, TAB_ASSETS};

    // Brush half-size clamp. The painted square is (BrushSize*2 + 1) tiles wide.
    constexpr int MAX_BRUSH_HALF = 10;

    // Layer-2 sentinel meaning "no overlay tile here" (matches InitTerrainMappingLayer).
    constexpr unsigned char NO_OVERLAY_TILE = Editor::Editing::NO_OVERLAY_TILE;

    // The overlay brush's strength: the share of the way to the opacity per frame.
    constexpr Editor::BrushControls::Range OVERLAY_STRENGTH_RANGE = {0.05f, 1.0f, 0.05f};
    // Outline colours of the round brushes on the ground.
    const std::uint32_t OVERLAY_OUTLINE_COLOR = mu::PackABGR(1.0f, 0.95f, 0.15f, 0.9f);
    const std::uint32_t ATTRIBUTE_OUTLINE_COLOR = mu::PackABGR(1.0f, 1.0f, 1.0f, 0.9f);

    // Total terrain cells (256x256).
    constexpr int CELLS = TERRAIN_SIZE * TERRAIN_SIZE;

    // The terrain arrays each brush paints, for its undo step.
    const std::vector<int> TEXTURE_LAYERS = {MAP_LAYER_TILE_BASE, MAP_LAYER_TILE_OVERLAY, MAP_LAYER_TILE_ALPHA};
    const std::vector<int> WALL_LAYERS = {MAP_LAYER_WALL};

    // Hands a finished brush stroke to the undo history (nothing when none was held
    // or it changed no cell).
    void FinishStroke(Editor::Editing::TerrainStroke& stroke)
    {
        g_MapEditHistory.Push(stroke.Finish());
    }
}

CMapEditorUI& CMapEditorUI::GetInstance()
{
    static CMapEditorUI instance;
    return instance;
}

int CMapEditorUI::ResolveWorldNumber() const
{
    if (m_TargetWorldOverride >= 0)
        return m_TargetWorldOverride;
    // Off-by-one: the world enum value maps to folder World{value + 1}. Special
    // per-map remaps (Blood Castle, Chaos Castle, ...) exist in MapManager, but
    // for the common case WorldActive + 1 is correct; the user can override the
    // number in the UI when a special map needs it.
    return gMapManager.WorldActive + 1;
}

void CMapEditorUI::EnterEditMode(int editFlag)
{
    EditFlag = editFlag;
    m_bEditActive = true;
}

void CMapEditorUI::RestoreGameMode()
{
    if (!m_bEditActive)
        return;
    EditFlag = EDIT_NONE;
    m_bEditActive = false;
}

void CMapEditorUI::Render(bool* p_open)
{
    const bool show = (p_open != nullptr && *p_open);

    // While this panel is up (or the map was opened offline), a FreeFly camera
    // culls the world itself, so the map stays drawn wherever it flies.
    CameraManager::Instance().SetFreeFlyCullsWorld(show || Editor::OfflineWorld::IsActive());
    UpdateTopDownRenderFlags(show);

    if (!show)
    {
        g_bMapEditorAttrOverlay = false;     // no panel -> no overlay
        g_bMapEditorBrushHighlight = false;  // no panel -> no brush cursor
        Render::Terrain::BrushOutline::Hide();
        Render::Terrain::GroundRects::Hide();    // no panel -> no gate areas
        g_MapObjectEditor.PublishOutline(false); // no panel -> no selection outline
        g_MapEditorHoveredObject = nullptr;  // no panel -> no hover outline
        g_MapEditorPlacementPreviewActive = false; // no panel -> no placement preview
        g_MapAssetReview.ClearHighlight();         // no panel -> no "Highlight all" outlines
        FinishStrokes();
        g_MapObjectEditor.EndFrame();
        RestoreGameMode();
        return;
    }

    ForgetUnloadedMap();
    CaptureGroundUnderCursor();
    // Before the panel, so the panel's tools know this frame when the cursor is over it.
    // Picking there readies the Objects tab to move what was picked (not to place more).
    if (g_MapOutliner.Render(&m_showOutliner, ResolveWorldNumber()))
        m_objMode = OBJECT_MODE_SELECT;
    m_newMapWindow.Render(&m_showNewMap);
    ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(MIN_PANEL_WIDTH * g_MuEditorCore.GetUIScale(), 0.0f),
                                        ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::Begin("Map Editor", p_open))
    {
        ImGui::End();
        g_MapObjectEditor.EndFrame();
        // Collapsed but still open: keep edit mode as-is.
        return;
    }

    // Reset the object-thumbnail render budget for this frame.
    g_ObjectThumbnail.BeginFrame();

    if (Editor::OfflineWorld::IsActive())
        RenderOfflineWorldBar();
    RenderHistoryBar();

    // Count hovering the scrollable child regions (e.g. the texture browser grid)
    // as hovering the panel, so the game cursor stays hidden and clicks don't leak
    // to the world while the mouse is over editor content.
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                               ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        g_MuEditorCore.SetHoveringUI(true);

    // Each tab decides what edit mode / overlay it needs this frame; default to none.
    // The Attribute tab turns the overlay back on below when it's the active tab, and
    // the Texture tab turns the brush highlight back on the same way.
    m_desiredEditFlag = EDIT_NONE;
    g_bMapEditorAttrOverlay = false;
    g_bMapEditorBrushHighlight = false;
    Render::Terrain::BrushOutline::Hide(); // the active brush tab shows it again
    Render::Terrain::GroundRects::Hide();  // the Gates tab shows them again
    // Unlike the selection outline (which follows the selection across tabs on
    // purpose), hover only makes sense while the Objects tab's Select mode is
    // actively driving it this frame - reset so switching tabs doesn't leave a
    // stale hover outline on screen. Same reasoning for the placement preview:
    // only PlaceObjects() (Objects tab, Place new mode) should keep it alive.
    g_MapEditorHoveredObject = nullptr;
    g_MapEditorPlacementPreviewActive = false;

    RenderTabs();
    // Opened from the Assets or the Objects tab; drawn here so it stays up whichever tab is active.
    g_RegenRequestDialog.Render();

    // The selection outline follows the selection regardless of which tab is active
    // (so switching to, say, the Attribute tab doesn't hide which objects you had
    // selected), and clears itself whenever the selection does (map change,
    // delete, undo).
    g_MapObjectEditor.PublishOutline(true);
    g_MapObjectEditor.EndFrame();

    ImGui::End();

    // The window's close (X) button flips *p_open. Restore the game the moment
    // that happens so the client doesn't stay stuck in edit mode.
    if (!*p_open)
    {
        RestoreGameMode();
        return;
    }

    // Apply the active tab's requested edit mode (or hand the game back if the
    // current tab isn't an editing tool, e.g. Browse).
    if (m_desiredEditFlag != EDIT_NONE)
        EnterEditMode(m_desiredEditFlag);
    else
        RestoreGameMode();
}

void CMapEditorUI::RenderTabs()
{
    if (!ImGui::BeginTabBar("MapEditorTabs"))
        return;
    if (ImGui::BeginTabItem(TAB_TEXTURE, nullptr, TabFlags(TAB_TEXTURE)))
    {
        RenderTextureTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_OBJECTS, nullptr, TabFlags(TAB_OBJECTS)))
    {
        RenderObjectsTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_HEIGHT, nullptr, TabFlags(TAB_HEIGHT)))
    {
        RenderHeightTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_ATTRIBUTE, nullptr, TabFlags(TAB_ATTRIBUTE)))
    {
        RenderAttributeTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_LIGHT, nullptr, TabFlags(TAB_LIGHT)))
    {
        RenderLightTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_GATES, nullptr, TabFlags(TAB_GATES)))
    {
        RenderGatesTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_TEXTURE_BROWSER, nullptr, TabFlags(TAB_TEXTURE_BROWSER)))
    {
        g_MapTextureBrowser.Render(ResolveWorldNumber());
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_MINIMAP, nullptr, TabFlags(TAB_MINIMAP)))
    {
        RenderMinimapTab();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(TAB_OBJECT_BROWSER, nullptr, TabFlags(TAB_OBJECT_BROWSER)))
    {
        RenderObjectBrowserTab();
        ImGui::EndTabItem();
    }
    // "Show in Assets tab" (Objects tab) switches here for one frame.
    const ImGuiTabItemFlags assetsTabFlags =
        m_selectAssetsTab ? ImGuiTabItemFlags_SetSelected : static_cast<ImGuiTabItemFlags>(TabFlags(TAB_ASSETS));
    m_selectAssetsTab = false;
    m_selectTab.clear();
    if (ImGui::BeginTabItem(TAB_ASSETS, nullptr, assetsTabFlags))
    {
        RenderAssetsTab();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

int CMapEditorUI::TabFlags(const char* name) const
{
    return m_selectTab == name ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
}

bool CMapEditorUI::ShowTab(const std::string& name)
{
    for (const char* tab : TAB_NAMES)
    {
        if (!Editor::Text::EqualIgnoringCase(name, tab))
            continue;
        g_MuEditorCore.SetEnabled(true);
        g_MuEditorCore.ShowMapEditor();
        m_selectTab = tab;
        return true;
    }
    return false;
}

std::vector<std::string> CMapEditorUI::TabNames()
{
    return std::vector<std::string>(std::begin(TAB_NAMES), std::end(TAB_NAMES));
}

void CMapEditorUI::RenderObjectBrowserTab()
{
    // Importing a model selects it for placing: jump to the Objects tab's
    // Place mode with the new type ready.
    int imported = -1;
    g_MapObjectBrowser.Render(ResolveWorldNumber(), &imported);
    if (imported < 0)
        return;
    m_selectedModelType = imported;
    m_objMode = OBJECT_MODE_PLACE;
    m_bObjEditEnabled = true;
    m_modelsWorld = -1; // force the Objects model list to rebuild
}

void CMapEditorUI::RenderAssetsTab()
{
    // Stepping to an instance there selects it here, ready to be moved.
    OBJECT* const primary = g_MapObjectEditor.Primary();
    OBJECT* stepped = primary;
    g_MapAssetReview.Render(ResolveWorldNumber(), stepped);
    if (stepped == primary)
        return;
    g_MapObjectEditor.Select({stepped});
    m_objMode = OBJECT_MODE_SELECT;
}

// While the minimap top-down view is active (and still on FreeFly), render the
// whole map: full-terrain bounds + g_Camera.TopViewEnable, which the engine
// itself uses for minimap capture to bypass per-tile terrain AND object
// culling.
//
// IMPORTANT: only WRITE these flags while we own the mode, and release them
// exactly once on exit. The game's own minimap uses TopViewEnable too, so
// writing it every frame (even false) would fight the game minimap and crash it.
void CMapEditorUI::UpdateTopDownRenderFlags(bool show)
{
    extern bool g_bMapEditorFullTerrain;
    extern CameraState g_Camera;

    // The window closing (X button, editor mode toggled off, or the panel no
    // longer being drawn) must end minimap capture even if the user never
    // clicked "Back to game camera" -- otherwise TopViewEnable/full-terrain
    // stay stuck true and MainScene's TopViewEnable early-return permanently
    // freezes normal gameplay input/UI processing.
    if (!show && m_bMinimapMode)
    {
        m_bMinimapMode = false;
        if (CameraManager::Instance().GetCurrentMode() == CameraMode::FreeFly)
            CameraManager::Instance().SetCameraMode(CameraMode::Default);
    }

    const bool topdown = m_bMinimapMode && (CameraManager::Instance().GetCurrentMode() == CameraMode::FreeFly);
    if (topdown)
    {
        g_bMapEditorFullTerrain = true;
        g_Camera.TopViewEnable = true;

        // Zoom with the mouse wheel. We read ImGui's wheel (valid for the whole
        // frame after NewFrame) rather than the game's MouseWheel global, which
        // the HUD consumes earlier in the frame than the camera update runs.
        // Skip it when the pointer is over an editor window so scrolling the
        // panel doesn't also zoom.
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && !ImGui::GetIO().WantCaptureMouse)
        {
            ICamera* cam = CameraManager::Instance().GetActiveCamera();
            if (cam && strcmp(cam->GetName(), "FreeFly") == 0)
                static_cast<FreeFlyCamera*>(cam)->ZoomTopDown(wheel);
        }
    }
    else if (m_topdownWasActive)
    {
        g_bMapEditorFullTerrain = false;
        g_Camera.TopViewEnable = false;
    }
    m_topdownWasActive = topdown;
}

void CMapEditorUI::RenderTextureTab()
{
    // Painting toggle. When on, take the game into EDIT_MAPPING (mouse picking +
    // paint); when off, hand the game back so you can walk without painting.
    ImGui::Checkbox("Enable painting", &m_bPaintingEnabled);
    if (m_bPaintingEnabled)
    {
        m_desiredEditFlag = EDIT_MAPPING;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "PAINTING");
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "off (walk freely)");
    }

    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Left-click terrain to paint. Right-click fades layer 2 out.");
    ImGui::Separator();

    // Target world / file the Save button writes to.
    int autoWorld = gMapManager.WorldActive + 1;
    bool useAuto = (m_TargetWorldOverride < 0);
    if (ImGui::Checkbox("Auto world number", &useAuto))
        m_TargetWorldOverride = useAuto ? -1 : autoWorld;
    if (!useAuto)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##world", &m_TargetWorldOverride);
        if (m_TargetWorldOverride < 0) m_TargetWorldOverride = 0;
    }
    ImGui::Text("Saves to: Data\\World%d\\EncTerrain%d.map", ResolveWorldNumber(), ResolveWorldNumber());

    ImGui::Separator();

    // Layer + brush controls. Both layers honor the brush size; the base layer
    // (0) replaces the tile outright, while the overlay (1) blends over the base
    // using the opacity below.
    ImGui::RadioButton("Layer 1 (base)", &CurrentLayer, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Layer 2 (overlay)", &CurrentLayer, 1);

    RenderTextureBrushControls();

    // Dropper: click picks the tile under the cursor (from the current layer)
    // into the selection instead of painting. Holding Alt does the same.
    ImGui::Checkbox("Dropper - pick tile under cursor (or hold Alt)", &m_bDropperMode);

    ImGui::Separator();
    ImGui::Text("Tile palette (selected: %s)", Editor::MapInspect::TileSlotName(SelectMapping).c_str());

    // Palette of the current world's loaded tile textures. Each cell is the
    // actual GPU texture the terrain samples, so it always matches this map.
    for (int slot = 0; slot < TILE_SLOT_COUNT; ++slot)
    {
        if (slot % TILE_COLUMNS != 0)
            ImGui::SameLine();

        ImGui::PushID(slot);
        SDL_GPUTexture* const tex = Bitmaps[BITMAP_MAPTILE + slot].sdlTexture;
        const bool selected = (SelectMapping == slot);

        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));

        bool clicked = false;
        if (tex != nullptr)
        {
            clicked = ImGui::ImageButton("t", (ImTextureID)(intptr_t)tex,
                                         ImVec2(TILE_THUMB_SIZE, TILE_THUMB_SIZE));
        }
        else
        {
            // Slot has no texture on this map (e.g. missing ExtTile) - show a
            // labeled placeholder so the index is still selectable.
            clicked = ImGui::Button("--", ImVec2(TILE_THUMB_SIZE, TILE_THUMB_SIZE));
        }

        if (selected)
            ImGui::PopStyleColor();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%d: %s", slot, Editor::MapInspect::TileSlotName(slot).c_str());
        if (clicked)
            SelectMapping = slot;

        ImGui::PopID();
    }

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Save terrain textures", ImVec2(-1.0f, 0.0f)))
    {
        const int world = ResolveWorldNumber();
        Editor::MapSave::SaveMappingEncrypted(world, world, m_textureStatus);
    }
    ImGui::PopStyleColor(2);
    RenderSaveTargetNote();
    Editor::StatusLine::Render(m_textureStatus);

    // Apply the brush this frame (after the UI, so hover state is known).
    PaintMapping();
}

void CMapEditorUI::RenderTextureBrushControls()
{
    if (CurrentLayer == 0)
    {
        // The base layer paints whole tiles: a square around the cursor.
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderInt("Brush", &BrushSize, 0, MAX_BRUSH_HALF, "");
        ImGui::SameLine();
        ImGui::Text("%d x %d tiles  [ ]", BrushSize * 2 + 1, BrushSize * 2 + 1);
        return;
    }
    // The overlay has an opacity per corner: a round brush with a soft edge.
    Editor::BrushControls::RenderRadius(m_overlayRadius);
    Editor::BrushControls::RenderStrength(m_overlayStrength, OVERLAY_STRENGTH_RANGE, "%.2f");
    // How strongly the layer-2 tile shows over the base where the brush is at full strength.
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Overlay opacity", &m_OverlayAlpha, 0.0f, 1.0f, "%.2f");
}

void CMapEditorUI::PaintMapping()
{
    // Uses m_PaintLDown/m_PaintRDown captured in CaptureInputForPainting (the raw
    // MouseLButton was cleared there to stop the hero walking).
    if (!m_bPaintingEnabled)
    {
        FinishStroke(m_textureStroke);
        return;
    }
    if (CurrentLayer == 1)
        Editor::BrushControls::ApplyKeys(m_overlayRadius, &m_overlayStrength, OVERLAY_STRENGTH_RANGE);
    else
        BrushSize = std::clamp(BrushSize + Editor::Shortcuts::BrushRadiusStep(), 0, MAX_BRUSH_HALF);

    const TerrainBrushInput input = BrushInput();
    if (!input.onGround)
    {
        FinishStroke(m_textureStroke);
        return;
    }

    const int x = (int)SelectXF;
    const int y = (int)SelectYF;
    ShowTextureBrush(input, x, y);

    // Dropper: a click samples the tile under the cursor into the selection.
    const bool dropper = m_bDropperMode || input.altDown;
    if (dropper)
    {
        if (m_PaintLDown)
            PickTile(x, y);
        FinishStroke(m_textureStroke);
        return;
    }

    // Alpha/RGBA tiles are overlay-only material; skip them for base painting to
    // avoid writing a translucent tile as an opaque base.
    if (CurrentLayer == 0 && Bitmaps[BITMAP_MAPTILE + SelectMapping].Components == 4)
        return;

    const bool leftPaint  = m_PaintLDown;
    const bool rightErase = m_PaintRDown && CurrentLayer == 1;
    if (!leftPaint && !rightErase)
    {
        FinishStroke(m_textureStroke);
        return;
    }

    // The whole drag is one undo step.
    if (!m_textureStroke.IsActive())
        m_textureStroke.Begin(g_MapEditHistory.Terrain(), TEXTURE_LAYERS,
                              leftPaint ? "Paint texture" : "Erase overlay");

    if (CurrentLayer == 0)
        PaintBaseTiles(x, y, leftPaint);
    else
        PaintOverlay(input, leftPaint);
}

void CMapEditorUI::ShowTextureBrush(const TerrainBrushInput& input, int x, int y)
{
    if (CurrentLayer == 1)
    {
        Editor::BrushControls::ShowOutline(input, m_overlayRadius, true, OVERLAY_OUTLINE_COLOR);
        return;
    }
    // Highlight the square the next click paints, on the ground before the click lands.
    g_bMapEditorBrushHighlight = true;
    g_MapEditorBrushMinX = x - BrushSize;
    g_MapEditorBrushMaxX = x + BrushSize;
    g_MapEditorBrushMinY = y - BrushSize;
    g_MapEditorBrushMaxY = y + BrushSize;
}

void CMapEditorUI::PickTile(int x, int y)
{
    const int idx = TERRAIN_INDEX(x, y);
    const unsigned char picked = (CurrentLayer == 0) ? TerrainMappingLayer1[idx] : TerrainMappingLayer2[idx];
    if (!(CurrentLayer == 1 && picked == NO_OVERLAY_TILE))
        SelectMapping = picked;
}

void CMapEditorUI::PaintBaseTiles(int x, int y, bool leftPaint)
{
    if (!leftPaint)
        return;
    // The square stops at the map's edges instead of wrapping around to the far side.
    const Editor::Editing::CellRect square = Editor::Editing::Grow({x, y, x, y}, BrushSize, TERRAIN_SIZE, TERRAIN_SIZE);
    const auto tile = (unsigned char)SelectMapping;
    for (int i = square.minY; i <= square.maxY; ++i)
        for (int j = square.minX; j <= square.maxX; ++j)
            TerrainMappingLayer1[TERRAIN_INDEX(j, i)] = tile;
}

void CMapEditorUI::PaintOverlay(const TerrainBrushInput& input, bool leftPaint)
{
    const Editor::Editing::OverlayLayer overlay = {TerrainMappingLayer2, TerrainMappingAlpha, TERRAIN_SIZE,
                                                   TERRAIN_SIZE};
    const Editor::Editing::BrushCircle circle = Editor::BrushControls::CircleAt(input, m_overlayRadius);
    if (leftPaint)
        Editor::Editing::PaintOverlay(overlay, circle, (unsigned char)SelectMapping, m_OverlayAlpha, m_overlayStrength);
    else
        Editor::Editing::EraseOverlay(overlay, circle, m_overlayStrength);
}

void CMapEditorUI::CaptureInputForPainting()
{
    m_PaintLDown = false;
    m_PaintRDown = false;
    g_MapObjectEditor.WithholdKeysFromGame();

    // Only intercept when a tool owns edit mode and the cursor is on the world
    // (not the UI). m_bEditActive covers both the texture and object tools.
    if (!m_bEditActive || g_MuEditorCore.IsHoveringUI())
        return;

    // Read the HELD state from ImGui, not the game's MouseLButton: the game's
    // button flags are set on the Windows press/release edges, and we clear them
    // below every frame, so they'd only ever read true on the press frame (which
    // breaks click-drag). ImGui tracks the physical button independently.
    m_PaintLDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    m_PaintRDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

    // Consume the click so the game doesn't also move/attack the hero.
    MouseLButton = MouseLButtonPop = MouseLButtonPush = MouseLButtonDBClick = false;
    MouseRButton = MouseRButtonPop = MouseRButtonPush = false;
}

void CMapEditorUI::RenderObjectsTab()
{
    const int world = ResolveWorldNumber();

    // (Re)load the model list when the target world changes (the selection is
    // cleared when the map's objects are freed, by ForgetUnloadedMap).
    if (world != m_modelsWorld)
    {
        m_models = Editor::ObjectPlace::EnumerateModels(world);
        m_modelsWorld = world;
        g_ObjectThumbnail.Invalidate();  // model slots differ per map
        if (!m_models.empty())
            m_selectedModelType = m_models.front().type;
        else
            m_selectedModelType = -1;
    }

    ImGui::Checkbox("Enable object editing", &m_bObjEditEnabled);
    if (m_bObjEditEnabled)
    {
        m_desiredEditFlag = EDIT_OBJECT;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "EDITING");
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "off (walk freely)");
    }

    ImGui::RadioButton("Place new", &m_objMode, OBJECT_MODE_PLACE);
    ImGui::SameLine();
    ImGui::RadioButton("Select & edit", &m_objMode, OBJECT_MODE_SELECT);
    ImGui::Separator();

    if (m_objMode == OBJECT_MODE_PLACE)
        RenderPlacePanel();
    else
        g_MapObjectEditor.RenderSelectionPanel(world, m_selectAssetsTab);

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Save objects", ImVec2(-1.0f, 0.0f)))
    {
        std::string report;
        Editor::ObjectPlace::Save(world, report);
        g_MapObjectEditor.SetStatus(report);
    }
    ImGui::PopStyleColor(2);
    RenderSaveTargetNote();
    Editor::StatusLine::Render(g_MapObjectEditor.Status());

    if (m_objMode == OBJECT_MODE_PLACE)
        PlaceObjects(world);
    else
        HandleObjectSelect();
}

void CMapEditorUI::RenderAttributeTab()
{
    const int world = ResolveWorldNumber();
    const int serverMap = (m_serverMapOverride >= 0) ? m_serverMapOverride : gMapManager.WorldActive;

    // Must run before anything reads m_attrBaseline / m_attrEdited below.
    EnsureAttrBaseline(world);

    ImGui::Checkbox("Enable attribute editing", &m_bAttrEnabled);
    if (m_bAttrEnabled)
    {
        m_desiredEditFlag = EDIT_WALL;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "PAINTING");
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "off (walk freely)");
    }

    ImGui::Checkbox("Show attribute overlay", &m_bAttrOverlay);
    if (m_bAttrOverlay)
        g_bMapEditorAttrOverlay = true;

    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                       "Left-click paints the selected attribute. Right-click paints Walkable.");
    ImGui::Separator();

    RenderAttributeBrushValues();
    Editor::BrushControls::RenderRadius(m_attrRadius);

    // --- Saving: the client and the server each need their own file. ---
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.6f, 1.0f), "1) Client file");
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Save client .att", ImVec2(-1.0f, 0.0f)))
        Editor::AttrSave::SaveClientAtt(world, world, m_attrStatus);
    ImGui::PopStyleColor(2);
    RenderSaveTargetNote();

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.6f, 1.0f), "2) Server file (OpenMU)");
    // The server keys maps by the world ENUM (Arena = 6); the client folder is
    // World{enum+1} (World7). Show both so they're never confused.
    bool autoServer = (m_serverMapOverride < 0);
    if (ImGui::Checkbox("Auto server map number", &autoServer))
        m_serverMapOverride = autoServer ? -1 : gMapManager.WorldActive;
    if (!autoServer)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##servermap", &m_serverMapOverride);
        if (m_serverMapOverride < 0) m_serverMapOverride = 0;
    }
    ImGui::Text("Server \"Number\" = %d   (client folder = World%d)", serverMap, world);

    // The server file is a MERGE onto the server's own map, not a dump of the client's:
    // the two legitimately disagree (the client blocks object footprints, the server
    // does not - 1084 tiles on Tarkan). So we need the server's current TerrainData as
    // a base, and we write only the tiles edited this session.
    const int editedCount = CountEditedAttributeTiles();

    ImGui::Text("Step 1: download this map's \"Terrain Data\" from the Admin Panel, then:");
    RenderServerBaseLoad(world);

    const bool haveBase = !m_serverBase.empty();
    if (haveBase)
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Base: %s   |   %d tile(s) edited this session",
                           m_serverBaseName.c_str(), editedCount);
    else
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                           "No server base loaded. It is needed so we only change the tiles you edited\n"
                           "and leave the rest of the server's map exactly as it is.");

    ImGui::BeginDisabled(!haveBase);
    RenderServerAttSave(serverMap);
    ImGui::EndDisabled();
    ImGui::TextDisabled("Upload the result in the Admin Panel's \"Terrain Data\" field.\nNever upload the client .att there - it is encrypted and would corrupt the map.");

    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f),
                       "Client + server must match, or players will desync (walk through walls / get pushed back).");
    Editor::StatusLine::Render(m_attrStatus);

    PaintAttribute();
}

void CMapEditorUI::RenderAttributeBrushValues()
{
    // These are the byte-sized TW_* bits the .att stores.
    ImGui::Text("Brush attribute:");
    ImGui::RadioButton("Walkable (0)", &m_attrBrushValue, 0);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.80f, 1.0f), "[combat area]");
    ImGui::RadioButton("Blocked / no-go (4)", &m_attrBrushValue, TW_NOMOVE);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.85f, 0.20f, 0.20f, 1.0f), "[red]");
    ImGui::RadioButton("Safezone (1)", &m_attrBrushValue, TW_SAFEZONE);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.20f, 0.80f, 0.35f, 1.0f), "[green - no combat]");
    ImGui::RadioButton("No ground / void (8)", &m_attrBrushValue, TW_NOGROUND);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f), "[black]");
    ImGui::RadioButton("Water (16)", &m_attrBrushValue, TW_WATER);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.23f, 0.43f, 0.82f, 1.0f), "[blue]");
}

int CMapEditorUI::CountEditedAttributeTiles()
{
    // The tab is redrawn every frame it's active, but the count only changes on a paint
    // stroke, an undo, or a baseline reset - so only rescan all CELLS tiles when one of
    // those has flagged it dirty, instead of every frame the tab is open.
    if (g_MapEditHistory.Terrain().TakeWallsChanged())
        m_attrCountDirty = true;
    if (!m_attrCountDirty)
        return m_attrEditedCountCache;
    m_attrEditedCountCache = 0;
    for (int i = 0; i < CELLS; ++i)
        if (m_attrEdited[i] && Editor::AttrSave::StaticAttribute(TerrainWall[i]) != m_attrBaseline[i])
            ++m_attrEditedCountCache;
    m_attrCountDirty = false;
    return m_attrEditedCountCache;
}

void CMapEditorUI::RenderServerAttSave(int serverMap)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
    const bool save = ImGui::Button("Step 2: Save server .att", ImVec2(-1.0f, 0.0f));
    ImGui::PopStyleColor(2);
    if (!save)
        return;

    std::string written;
    int changed = 0;
    if (!Editor::AttrSave::SaveServerAtt(serverMap, m_serverBase, m_attrBaseline, m_attrEdited, written, changed))
    {
        m_attrStatus = "Server .att save failed.";
        return;
    }
    char buf[192];
    snprintf(buf, sizeof(buf),
             "Wrote the server .att (+ HOWTO) - %d tile(s) changed, the rest of the server's map is untouched. "
             "Upload it in the Admin Panel, then restart the server.\n",
             changed);
    m_attrStatus = buf + written;
}

void CMapEditorUI::RenderServerBaseLoad(int world)
{
    using Editor::Files::FilePickRequest;
    using Editor::Files::FilePickState;

    const Editor::Files::FilePickResult pick = Editor::Files::PollOpenFile(FilePickRequest::ServerBaseAtt);
    if (pick.state == FilePickState::Cancelled)
        m_attrStatus = "Loading the server base was cancelled.";
    else if (pick.state == FilePickState::Failed)
        m_attrStatus = "Could not open the file dialog: " + pick.error;
    else if (pick.state == FilePickState::Picked)
        LoadServerBaseFile(pick.path, world);

    ImGui::BeginDisabled(Editor::Files::IsOpenFilePending(FilePickRequest::ServerBaseAtt));
    if (ImGui::Button("Load server base .att...", ImVec2(-1.0f, 0.0f)) &&
        Editor::Files::RequestOpenFile(FilePickRequest::ServerBaseAtt))
    {
        // The dialog answers in a later frame; the base belongs to the map current now.
        m_serverBasePickWorld = world;
        m_attrStatus = "Choose the server's TerrainData in the file dialog...";
    }
    ImGui::EndDisabled();
}

void CMapEditorUI::LoadServerBaseFile(const std::filesystem::path& file, int world)
{
    // EnsureAttrBaseline forgets the server base whenever the map changes, so a base
    // picked for another map must not be attached to this one.
    if (world != m_serverBasePickWorld)
    {
        m_attrStatus = "The map changed while the file dialog was open - load the server base again.";
        return;
    }

    std::string err;
    if (!Editor::AttrSave::LoadServerBaseAtt(file.wstring(), m_serverBase, err))
    {
        m_serverBase.clear();
        m_serverBaseName.clear();
        m_attrStatus = err;
        return;
    }

    m_serverBaseName = StringUtils::WideToNarrow(file.filename().wstring().c_str());
    m_attrStatus = "Server base loaded. Your edits will be merged onto it.";
}

void CMapEditorUI::EnsureAttrBaseline(int world)
{
    if (m_attrBaselineWorld == world && m_attrBaseline.size() == CELLS)
        return;

    // New map: snapshot what the client's walk map looked like before we touched it,
    // and forget both the edit set and any server base loaded for the previous map.
    m_attrBaseline.resize(CELLS);
    for (int i = 0; i < CELLS; ++i)
        m_attrBaseline[i] = Editor::AttrSave::StaticAttribute(TerrainWall[i]);

    m_attrEdited.assign(CELLS, false);
    m_attrBaselineWorld = world;
    m_serverBase.clear();
    m_serverBaseName.clear();
    m_attrCountDirty = true;
}

void CMapEditorUI::PaintAttribute()
{
    if (!m_bAttrEnabled)
    {
        FinishStroke(m_attrStroke);
        return;
    }
    Editor::BrushControls::ApplyKeys(m_attrRadius, nullptr, {});
    const TerrainBrushInput input = BrushInput();
    if (!input.onGround)
    {
        FinishStroke(m_attrStroke);
        return;
    }
    Editor::BrushControls::ShowOutline(input, m_attrRadius, false, ATTRIBUTE_OUTLINE_COLOR);

    const bool paint = m_PaintLDown;
    const bool clear = m_PaintRDown;   // right-click = back to walkable
    if (!paint && !clear)
    {
        FinishStroke(m_attrStroke);
        return;
    }

    if (!m_attrStroke.IsActive())
        m_attrStroke.Begin(g_MapEditHistory.Terrain(), WALL_LAYERS, paint ? "Paint walkability" : "Clear walkability");

    PaintWalls(Editor::BrushControls::CircleAt(input, m_attrRadius),
               static_cast<unsigned short>(paint ? m_attrBrushValue : 0));
}

void CMapEditorUI::PaintWalls(const Editor::Editing::BrushCircle& circle, unsigned short value)
{
    // Set (not OR) the value, matching the standalone terrain editor's behaviour, on
    // every tile whose centre lies inside the circle (a hard edge, clipped to the map).
    Editor::Editing::ForEachCellInside(circle, TERRAIN_SIZE, TERRAIN_SIZE, Editor::Editing::CellAnchor::TileCentre,
                                       [this, value](int x, int y)
                                       {
                                           const int idx = TERRAIN_INDEX(x, y);
                                           TerrainWall[idx] = value;
                                           // The server export merges only the tiles touched here onto
                                           // the server's own map (see m_attrEdited).
                                           if (!m_attrEdited.empty())
                                               m_attrEdited[idx] = true;
                                       });
    m_attrCountDirty = true;
}

void CMapEditorUI::RenderOfflineWorldBar()
{
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "World%d opened offline: no server, monsters or NPCs.",
                       gMapManager.WorldActive + 1);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Fly: arrow keys, PgUp/PgDn (fn+Up/fn+Down on a Mac laptop), Shift = faster. "
                       "Look: right-drag (tools off).");
    ImGui::PopStyleColor();
    if (ImGui::Button("Reset view"))
    {
        m_bMinimapMode = false; // leave the top-down minimap view, if it is on
        Editor::OfflineWorld::ResetCamera();
    }
    ImGui::Separator();
}

void CMapEditorUI::RenderMinimapTab()
{
    ImGui::TextWrapped("Capture a top-down screenshot of the whole map to make a minimap.");
    ImGui::Separator();

    auto& mgr = CameraManager::Instance();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
    if (ImGui::Button("Top-down view (for minimap)", ImVec2(-1.0f, 0.0f)))
    {
        mgr.SetCameraMode(CameraMode::FreeFly);
        ICamera* cam = mgr.GetActiveCamera();
        if (cam && strcmp(cam->GetName(), "FreeFly") == 0)
            static_cast<FreeFlyCamera*>(cam)->SnapTopDown();
        m_bMinimapMode = true;   // render the whole terrain for the shot
    }
    ImGui::PopStyleColor(2);

    // Rotate the top-down view to orient the map for the shot.
    auto rotateTopDown = [&mgr](float deg)
    {
        ICamera* cam = mgr.GetActiveCamera();
        if (cam && strcmp(cam->GetName(), "FreeFly") == 0)
            static_cast<FreeFlyCamera*>(cam)->RotateYaw(deg);
    };
    if (ImGui::Button("Rotate 90 CW"))
        rotateTopDown(90.0f);
    ImGui::SameLine();
    if (ImGui::Button("Rotate 90 CCW"))
        rotateTopDown(-90.0f);

    // Generate the minimap straight from the map's terrain tiles. This is not a
    // screenshot: it composes pixel (x,y) = tile (x,y), the same mapping the game
    // uses for the position marker, so the marker lands on the player exactly. Works
    // from anywhere - no need for the top-down view.
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Generate minimap from tiles", ImVec2(-1.0f, 0.0f)))
        Editor::Minimap::GenerateFromTiles(ResolveWorldNumber(), m_minimapStatus);
    ImGui::PopStyleColor(2);

    // Wrap an externally-edited .tga (e.g. the generated one with a screenshot painted
    // over it) into the game's mini_map.OZT.
    RenderMinimapTgaConvert();
    ImGui::TextDisabled("Use this after editing the .tga in an image editor.");

    RenderSaveTargetNote();
    Editor::StatusLine::Render(m_minimapStatus);

    if (ImGui::Button("Back to game camera", ImVec2(-1.0f, 0.0f)))
    {
        m_bMinimapMode = false;
        ICamera* cam = mgr.GetActiveCamera();
        if (cam && strcmp(cam->GetName(), "FreeFly") == 0)
            static_cast<FreeFlyCamera*>(cam)->ClearTopDown();
        mgr.SetCameraMode(CameraMode::Default);
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Tips:");
    ImGui::BulletText("\"Generate minimap from tiles\" builds mini_map.OZT (+ an");
    ImGui::BulletText("editable .tga) from the terrain - the position marker will");
    ImGui::BulletText("line up exactly. It works from anywhere; no top-down needed.");
    ImGui::BulletText("The top-down view + arrows/wheel are just for looking around.");
}

void CMapEditorUI::RenderMinimapTgaConvert()
{
    using Editor::Files::FilePickRequest;
    using Editor::Files::FilePickState;

    const Editor::Files::FilePickResult pick = Editor::Files::PollOpenFile(FilePickRequest::MinimapTga);
    if (pick.state == FilePickState::Cancelled)
        m_minimapStatus = "Conversion cancelled.";
    else if (pick.state == FilePickState::Failed)
        m_minimapStatus = "Could not open the file dialog: " + pick.error;
    else if (pick.state == FilePickState::Picked)
        Editor::Minimap::WrapTgaFileToOzt(m_minimapTgaWorld, pick.path.wstring(), m_minimapStatus);

    ImGui::BeginDisabled(Editor::Files::IsOpenFilePending(FilePickRequest::MinimapTga));
    if (ImGui::Button("Convert a .tga to mini_map.OZT...", ImVec2(-1.0f, 0.0f)) &&
        Editor::Files::RequestOpenFile(FilePickRequest::MinimapTga))
    {
        // The dialog answers in a later frame; convert for the map current now.
        m_minimapTgaWorld = ResolveWorldNumber();
        m_minimapStatus = "Choose the .tga in the file dialog...";
    }
    ImGui::EndDisabled();
}

void CMapEditorUI::RenderHeightTab()
{
    ImGui::Checkbox("Enable height editing", &m_bHeightEnabled);
    if (m_bHeightEnabled)
    {
        m_desiredEditFlag = EDIT_HEIGHT;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "SCULPTING");
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "off (walk freely)");
    }
    ImGui::Separator();
    m_heightTool.RenderControls();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Save height", ImVec2(-1.0f, 0.0f)))
        Editor::HeightSave::Save(ResolveWorldNumber(), m_heightStatus);
    ImGui::PopStyleColor(2);
    RenderSaveTargetNote();
    Editor::StatusLine::Render(m_heightStatus);

    SculptHeight();
}

void CMapEditorUI::RenderLightTab()
{
    ImGui::Checkbox("Enable light painting", &m_bLightEnabled);
    if (m_bLightEnabled)
    {
        m_desiredEditFlag = EDIT_LIGHT;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "PAINTING");
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "off (walk freely)");
    }
    ImGui::Separator();
    m_lightTool.RenderControls();

    ImGui::Separator();
    const int world = ResolveWorldNumber();
    ImGui::Text("Saves to: Data/World%d/TerrainLight.OZJ", world);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Save light", ImVec2(-1.0f, 0.0f)))
        m_lightTool.Save(world);
    ImGui::PopStyleColor(2);
    RenderSaveTargetNote();
    Editor::StatusLine::Render(m_lightTool.Status());

    if (m_bLightEnabled)
        m_lightTool.Apply(BrushInput());
    else
        m_lightTool.FinishStroke();
}

TerrainBrushInput CMapEditorUI::BrushInput() const
{
    TerrainBrushInput input;
    input.onGround = m_groundHitValid && !g_MuEditorCore.IsHoveringUI();
    input.groundX = m_groundHit[0];
    input.groundY = m_groundHit[1];
    input.leftDown = m_PaintLDown;
    input.rightDown = m_PaintRDown;
    input.altDown = ImGui::GetIO().KeyAlt;
    return input;
}

void CMapEditorUI::SculptHeight()
{
    if (m_bHeightEnabled)
        m_heightTool.Apply(BrushInput());
    else
        m_heightTool.FinishStroke();
}

void CMapEditorUI::RenderPlacePanel()
{
    ImGui::Text("Loaded object models on this map: %d", (int)m_models.size());
    if (m_models.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "No world-object models are loaded on this map.");
        return;
    }

    // Placement transform.
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Yaw", &m_objYaw, 0.0f, 360.0f, "%.0f deg");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("Scale", &m_objScale, 0.05f, 0.25f, "%.2f");
    if (m_objScale < 0.05f)
        m_objScale = 0.05f;
    ImGui::Checkbox("Snap to tile centre", &m_bObjSnap);
    RenderModelPalette();
}

void CMapEditorUI::RenderModelPalette()
{
    // Model palette: a grid of live 3D thumbnails of the loaded models.
    char selName[80] = "(none)";
    for (const auto& m : m_models)
        if (m.type == m_selectedModelType)
        {
            snprintf(selName, sizeof(selName), "%d: %ls", m.type, m.file.c_str());
            break;
        }
    ImGui::Text("Model palette (selected: %s)", selName);
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Left-click the ground to place.");

    const float thumb = (float)g_ObjectThumbnail.Size();
    const int columns = 5;
    // Fill the remaining window height, leaving room for the Save button, its
    // note and the status text below (~110px).
    float paletteH = ImGui::GetContentRegionAvail().y - 110.0f;
    if (paletteH < 140.0f)
        paletteH = 140.0f;
    ImGui::BeginChild("ObjPalette", ImVec2(0, paletteH), true);
    int col = 0;
    for (const auto& m : m_models)
    {
        if (col % columns != 0)
            ImGui::SameLine();
        ++col;

        ImGui::PushID(m.type);
        const bool selected = (m.type == m_selectedModelType);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));

        const unsigned int tex = g_ObjectThumbnail.Get(m.type);
        SDL_GPUTexture* const texPtr =
            (tex != 0) ? static_cast<SDL_GPUTexture*>(mu::GetRenderer().GetTexturePointer(tex)) : nullptr;
        bool clicked = false;
        if (texPtr != nullptr)
            clicked = ImGui::ImageButton("t", (ImTextureID)(intptr_t)texPtr, ImVec2(thumb, thumb));
        else // still rendering this frame - show a placeholder button
            clicked = ImGui::Button("...", ImVec2(thumb, thumb));

        if (selected)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%d: %ls", m.type, m.file.c_str());
        if (clicked)
            m_selectedModelType = m.type;
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void CMapEditorUI::PlaceObjects(int world)
{
    (void)world;
    // Place mode never picks a hover target (HandleObjectSelect owns that, and
    // isn't called while this mode is active) - clear any stale hover left over
    // from Select mode so its outline doesn't linger after switching modes.
    g_MapEditorHoveredObject = nullptr;
    if (!m_bObjEditEnabled || m_selectedModelType < 0)
    {
        m_objWasDown = false;
        g_MapEditorPlacementPreviewActive = false;
        return;
    }

    const bool down = m_PaintLDown;   // captured left button (game click suppressed)

    // Placement preview: show the selected model, translucent, at the exact
    // spot a click would place it - every frame, before any click, so hovering
    // the ground previews the placement instead of guessing blind.
    g_MapEditorPlacementPreviewActive = SelectFlag && !g_MuEditorCore.IsHoveringUI();
    if (g_MapEditorPlacementPreviewActive)
    {
        Editor::ObjectPlace::ComputePlacementPosition(m_groundHit[0], m_groundHit[1], m_bObjSnap,
                                                      g_MapEditorPlacementPreviewPos);
        g_MapEditorPlacementPreviewType = m_selectedModelType;
        g_MapEditorPlacementPreviewYaw = m_objYaw;
        g_MapEditorPlacementPreviewScale = m_objScale;
    }

    // Place one object on the press edge, only when the cursor is over terrain
    // (SelectFlag) and not over the editor UI.
    if (down && !m_objWasDown && SelectFlag && !g_MuEditorCore.IsHoveringUI())
        PlaceObjectAtCursor();
    m_objWasDown = down;
}

void CMapEditorUI::PlaceObjectAtCursor()
{
    Editor::Editing::ObjectState state;
    state.type = m_selectedModelType;
    Editor::ObjectPlace::ComputePlacementPosition(m_groundHit[0], m_groundHit[1], m_bObjSnap, state.position);
    state.angle[2] = m_objYaw;
    state.scale = m_objScale;

    char label[64];
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    OBJECT* placed = world.CreateNew(state);
    if (placed == nullptr)
    {
        snprintf(label, sizeof(label), "Place failed (type %d).", m_selectedModelType);
        g_MapObjectEditor.SetStatus(label);
        return;
    }
    snprintf(label, sizeof(label), "Place object (type %d)", m_selectedModelType);
    const std::vector<Editor::Editing::KeyedObjectState> created = {
        {placed->SaveOrder, CMapObjectWorld::StateOf(placed)}};
    g_MapEditHistory.Push(
        std::make_unique<Editor::Editing::ObjectEditCommand>(label, world, Editor::Editing::CreationChanges(created)));
    g_MapObjectEditor.SetStatus(std::string(label) + ".");
}

void CMapEditorUI::HandleObjectSelect()
{
    if (!m_bObjEditEnabled)
    {
        g_MapEditorHoveredObject = nullptr;
        return;
    }
    CMapObjectEditor::WorldInput input;
    input.groundValid = m_groundHitValid;
    VectorCopy(m_groundHit, input.ground);
    input.leftDown = m_PaintLDown;
    input.overWorld = !g_MuEditorCore.IsHoveringUI();
    g_MapObjectEditor.HandleWorldInput(input);
}

void CMapEditorUI::ForgetUnloadedMap()
{
    // A map unload frees every object, also when the same map loads again or the
    // panel was closed meanwhile. A selection made before (Objects tab, or the Assets
    // tab's Prev/Next) then points at freed memory, and an undo step would write the
    // old map's objects, heights or walls into the new one.
    Editor::LiveMap::SyncWithLoadedMap();
    const unsigned int generation = ObjectListGeneration();
    if (generation == m_objectListGeneration)
        return;
    m_objectListGeneration = generation;
    ForgetEdits();
}

void CMapEditorUI::ForgetEdits()
{
    g_MapObjectEditor.Forget();
    // A stroke still held down starts over, so it records the new map.
    m_textureStroke.Cancel();
    m_heightTool.CancelStroke();
    m_attrStroke.Cancel();
    m_lightTool.CancelStroke();
    g_MapEditHistory.Forget();
}

void CMapEditorUI::BeforeScriptedEdit()
{
    EnsureAttrBaseline(ResolveWorldNumber());
}

void CMapEditorUI::NoteScriptedWallEdits(const Editor::Editing::CellRect& walls)
{
    if (walls.IsEmpty() || m_attrEdited.size() != CELLS)
        return;
    for (int y = walls.minY; y <= walls.maxY; ++y)
    {
        for (int x = walls.minX; x <= walls.maxX; ++x)
            m_attrEdited[TERRAIN_INDEX(x, y)] = true;
    }
    m_attrCountDirty = true;
}

void CMapEditorUI::ShowGate(int number)
{
    g_MuEditorCore.SetEnabled(true);
    g_MuEditorCore.ShowMapEditor();
    m_selectTab = TAB_GATES;
    m_gatesTab.Select(number);
}

void CMapEditorUI::RenderGatesTab()
{
    // Gates need walkable ground: the walkability overlay is always on here.
    g_bMapEditorAttrOverlay = true;
    m_desiredEditFlag = m_gatesTab.Render(BrushInput());
}

void CMapEditorUI::RenderHistoryBar()
{
    ImGui::Checkbox("Outliner", &m_showOutliner);
    ImGui::SameLine();
    if (ImGui::Button("New map..."))
        m_showNewMap = true;
    ImGui::SameLine();
    const HistoryStep step = g_MapEditHistory.Render(IsEditInProgress());
    if (step != HistoryStep::None)
        g_MapObjectEditor.OnHistoryStep(step == HistoryStep::Undone);
    ImGui::Separator();
}

bool CMapEditorUI::IsEditInProgress() const
{
    return m_textureStroke.IsActive() || m_heightTool.IsStrokeActive() || m_attrStroke.IsActive() ||
           m_lightTool.IsStrokeActive() || g_MapObjectEditor.IsEditInProgress();
}

void CMapEditorUI::FinishStrokes()
{
    FinishStroke(m_textureStroke);
    m_heightTool.FinishStroke();
    FinishStroke(m_attrStroke);
    m_lightTool.FinishStroke();
}

void CMapEditorUI::CaptureGroundUnderCursor()
{
    // Read before any object pick of this frame: CollisionDetectObjects writes
    // its mesh hit into CollisionPosition.
    m_groundHitValid = SelectFlag;
    VectorCopy(CollisionPosition, m_groundHit);
}

void CMapEditorUI::RenderSaveTargetNote()
{
    const Editor::Files::RepoRootLookup& repo = Editor::Files::RepoRoot();
    if (repo.root.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
        ImGui::TextWrapped("No repository found (%s): saves overwrite the game's Data only (no backup), plus a copy "
                           "next to the game. Set MU_EDITOR_REPO_ROOT to your checkout.",
                           repo.description.c_str());
        ImGui::PopStyleColor();
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Saves go to the game's Data and to %s/src/bin/Data; the repo file they replace is kept "
                       "in out/editor-backups.",
                       Editor::Files::PathToUtf8(repo.root).c_str());
    ImGui::PopStyleColor();
}

#endif // _EDITOR
