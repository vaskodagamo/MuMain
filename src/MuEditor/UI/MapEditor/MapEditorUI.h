#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>
#include <vector>

#include "MapObjectPlace.h"  // Editor::ObjectPlace::ModelEntry
#include "MapBrushControls.h"
#include "MapGatesTab.h"
#include "MapHeightTool.h"
#include "MapLightTool.h"
#include "MapNewMapWindow.h"
#include "Editing/TerrainStroke.h"

class OBJECT;

// In-game Map Editor panel.
//
// Rather than reimplement terrain picking and painting, this panel drives the
// client's existing edit pipeline: setting the global EditFlag makes the terrain
// render loop ray-cast the mouse onto the ground each frame (SelectXF/SelectYF/
// CollisionPosition), and Editor::EditObjects() (called every frame in
// MainScene) performs the actual paint using the brush/selection globals this
// panel exposes. The panel adds a texture palette and an encrypting Save that
// writes the edit back to the map's real, reloadable file.
//
// Phase 1 covers the terrain texture (mapping) painter. Later phases (attribute,
// height, objects) add tabs to the same window.
class CMapEditorUI
{
public:
    static CMapEditorUI& GetInstance();

    // Called once per frame while the editor is open. Pass the show flag; pass
    // nullptr (or a false flag) to signal "not shown" so the panel can hand the
    // game back to normal (EDIT_NONE) mode.
    void Render(bool* p_open);

    // Called from the editor Update (before the game consumes input) to capture
    // the mouse-button state for the active tool and suppress the click from
    // reaching the game, so the character doesn't walk/attack while editing.
    void CaptureInputForPainting();

    // Drops the selection and the undo history once the map's objects were freed
    // (a map change or reload), also one that happened while the panel was closed,
    // and takes the new map as the saved state; see ObjectListGeneration(). Runs at
    // the start of every panel frame, and right after a scripted map switch.
    void ForgetUnloadedMap();
    // Drops the selection, the strokes still held and every undo step now: a scripted
    // revert reloaded parts of the map from its files.
    void ForgetEdits();

    // A stroke or drag is still held: undo, redo and scripted edits wait for it.
    bool IsEditInProgress() const;

    // Around a scripted edit (the control socket's map-apply): before it, the Attribute
    // tab takes its baseline for the server export if it has none for this map yet;
    // after it, the tiles in `walls` count as edited, so "Save server .att" writes the
    // ones that changed.
    void BeforeScriptedEdit();
    void NoteScriptedWallEdits(const Editor::Editing::CellRect& walls);

    // Opens the Map Editor on its Gates tab with gate `number` selected (the control
    // socket's gate-show), so the owner sees the gate an agent talks about.
    void ShowGate(int number);

    // Opens the Map Editor on the tab called `name` (any case, e.g. "Texture" or "Gates";
    // the control socket's map-tab). False, and nothing changes, for a name no tab has.
    bool ShowTab(const std::string& name);
    // The tab names, in the order the panel shows them.
    static std::vector<std::string> TabNames();

private:
    CMapEditorUI() = default;
    ~CMapEditorUI() = default;

    // The tab bar and the active tab.
    void RenderTabs();
    // ImGuiTabItemFlags for the tab called `name`: selected this frame when ShowTab asked for it.
    int TabFlags(const char* name) const;
    void RenderObjectBrowserTab();
    // The Assets tab; stepping to an instance there selects it.
    void RenderAssetsTab();
    // Owns the whole-map render flags while the Minimap tab's top-down view is on,
    // and releases them once when it ends (also when the panel closes).
    void UpdateTopDownRenderFlags(bool show);
    void RenderTextureTab();
    void RenderObjectsTab();
    void RenderHeightTab();
    void RenderMinimapTab();
    // Status line and "Reset view" for a map opened with --world (no server).
    void RenderOfflineWorldBar();
    // One line under a Save button saying where saves go (game Data + repository).
    void RenderSaveTargetNote();
    // Keeps this frame's terrain point under the cursor (see m_groundHit).
    void CaptureGroundUnderCursor();
    // The Undo/Redo buttons shared by every editing tab (and their shortcuts).
    void RenderHistoryBar();
    // Ends the brush strokes that are still open, each as one undo step.
    void FinishStrokes();
    // "Convert a .tga to mini_map.OZT...": opens the file dialog, then converts the
    // picked file in a later frame (the dialog does not block the game).
    void RenderMinimapTgaConvert();
    void RenderAttributeTab();
    // The Attribute tab's brush values (the TW_* bits the .att stores).
    void RenderAttributeBrushValues();
    // Tiles painted this session that now differ from the baseline; rescans only after
    // a stroke, an undo or a baseline reset.
    int CountEditedAttributeTiles();
    // "Step 2: Save server .att": merges the edited tiles onto the loaded server base.
    void RenderServerAttSave(int serverMap);
    // "Load server base .att...": same two-step pattern as RenderMinimapTgaConvert.
    void RenderServerBaseLoad(int world);
    void LoadServerBaseFile(const std::filesystem::path& file, int world);
    // Paints TerrainWall[] with the selected attribute under the cursor.
    void PaintAttribute();
    // Sets the walkability of the tiles inside `circle` to `value`.
    void PaintWalls(const Editor::Editing::BrushCircle& circle, unsigned short value);
    // Hands this frame's cursor to the Height tab's brush while the tab is active.
    void SculptHeight();
    // The Light tab: paints and saves the map's light map.
    void RenderLightTab();
    // The Gates tab: the map's gates over the walkability overlay (MapGatesTab).
    void RenderGatesTab();
    // The cursor on the ground as the round brushes see it this frame.
    TerrainBrushInput BrushInput() const;

    // Paints the terrain mapping arrays from the current brush/selection when the
    // mouse is over terrain and pressed. Runs each frame the Texture tab is
    // active; the legacy Editor::EditObjects() paint path is compiled out in this
    // build (it's gated on ENABLE_EDIT, not _EDITOR), so the editor owns it.
    void PaintMapping();
    // Where the next click paints: layer 1's square or layer 2's circle, on the ground.
    void ShowTextureBrush(const TerrainBrushInput& input, int x, int y);
    // The dropper: selects the tile of the current layer at (x, y).
    void PickTile(int x, int y);
    // Layer 1: the square of tiles around the cursor gets the selected tile.
    void PaintBaseTiles(int x, int y, bool leftPaint);
    // Layer 2: the round, soft brush paints or fades the overlay.
    void PaintOverlay(const TerrainBrushInput& input, bool leftPaint);
    // The brush sliders of the selected layer and their [ ] keys.
    void RenderTextureBrushControls();

    // Place new mode: the placement transform and the model palette.
    void RenderPlacePanel();
    // A grid of live 3D thumbnails of the map's models; a click picks the one to place.
    void RenderModelPalette();
    // Places an object on left-click (Objects tab, Place mode).
    void PlaceObjects(int world);
    // Places the selected model at the cursor as one undo step.
    void PlaceObjectAtCursor();
    // Select & edit mode: hands this frame's world input to the object editor.
    void HandleObjectSelect();

    // Puts the game into the given edit mode (sets EditFlag). Idempotent.
    void EnterEditMode(int editFlag);
    // Returns the game to normal play (EDIT_NONE) if this panel had taken it.
    void RestoreGameMode();

    // Resolves which World{N}/EncTerrain{N} the live map maps to. -1 = auto from
    // the active world; a user override wins when >= 0.
    int ResolveWorldNumber() const;

    bool m_bEditActive = false;      // this panel currently owns EditFlag
    int  m_desiredEditFlag = 0;      // EDIT_* the active tab wants this frame (0 = EDIT_NONE)
    bool m_bPaintingEnabled = false; // when off, the panel stays open but the
                                     // game plays normally (walk without painting)
    int  m_TargetWorldOverride = -1; // -1 = auto (gMapManager.WorldActive + 1)
    float m_OverlayAlpha = 1.0f;     // blend strength applied to layer-2 overlay paint
    float m_overlayRadius = 3.0f;    // layer-2 brush radius (tiles)
    float m_overlayStrength = 0.5f;  // share of the way to m_OverlayAlpha per frame at the brush's core
    bool m_bDropperMode = false;     // when on (or Alt held), a click picks the tile

    // Mouse state captured before the game consumes it (see CaptureInputForPainting).
    bool m_PaintLDown = false;
    bool m_PaintRDown = false;
    Editor::Editing::TerrainStroke m_textureStroke; // the paint stroke held now (one undo step)

    std::string m_textureStatus;     // last "Save terrain textures" result

    // The ground point under the cursor this frame. The terrain pass leaves it in
    // CollisionPosition, but picking an object overwrites that with the hit on
    // the object's mesh, so the tools read this copy instead.
    bool   m_groundHitValid = false;
    vec3_t m_groundHit = {};

    // --- Objects tab state ---
    bool  m_bObjEditEnabled = false;   // master: take EDIT_OBJECT (else walk freely)
    int   m_objMode = 0;               // 0 = Place new, 1 = Select & edit
    int   m_selectedModelType = -1;    // CreateObject type to place
    float m_objYaw = 0.0f;             // placement yaw (degrees)
    float m_objScale = 1.0f;           // placement scale
    bool  m_bObjSnap = false;          // snap placement to tile centre
    bool  m_objWasDown = false;        // rising-edge guard so one click = one object
    bool  m_selectAssetsTab = false;   // "Show in Assets tab" was clicked: switch tabs next frame
    std::string m_selectTab;           // gate-show, map-tab: the tab to switch to next frame (empty: none)
    bool m_showOutliner = false;       // the Outliner window is open
    unsigned int m_objectListGeneration = 0; // ObjectListGeneration() the selection and undo belong to
    int   m_modelsWorld = -1;          // world the cached model list is for
    std::vector<Editor::ObjectPlace::ModelEntry> m_models;

    // --- Height tab state ---
    bool  m_bHeightEnabled = false;   // master: take EDIT_HEIGHT (else walk freely)
    CMapHeightTool m_heightTool;      // the brush, its settings and the stroke held now
    std::string m_heightStatus;       // last "Save height" result

    // --- Light tab state ---
    bool m_bLightEnabled = false; // master: take EDIT_LIGHT (else walk freely)
    CMapLightTool m_lightTool;

    // --- Gates tab and New map window ---
    CMapGatesTab m_gatesTab;
    CMapNewMapWindow m_newMapWindow;
    bool m_showNewMap = false; // the New map window is open

    // Minimap top-down mode: while active (and camera is FreeFly), force the whole
    // terrain to render for a full-map screenshot.
    bool m_bMinimapMode = false;
    bool m_topdownWasActive = false;  // edge-detect so we only own TopViewEnable while active
    std::string m_minimapStatus;      // result text for the generate button
    int m_minimapTgaWorld = -1;       // map that was current when the .tga dialog was opened

    // --- Attribute (walkability) tab state ---
    bool m_bAttrEnabled = false;      // master: take EDIT_WALL (else walk freely)
    bool m_bAttrOverlay = true;       // tint tiles by their attribute
    int  m_attrBrushValue = 4;        // TW_* value the brush writes (0 = walkable)
    float m_attrRadius = 1.0f;        // brush radius (tiles), hard edge
    Editor::Editing::TerrainStroke m_attrStroke;
    int  m_serverMapOverride = -1;    // -1 = auto (gMapManager.WorldActive)
    std::string m_attrStatus;

    // Server export is a MERGE onto the server's current TerrainData, because the two
    // walk maps legitimately differ (the client blocks object footprints, the server
    // does not - 1084 tiles on Tarkan). We therefore need to know exactly which tiles
    // the user touched, and write only those. See MapAttributeSave.h.
    std::vector<BYTE>  m_attrBaseline;      // client attr bytes at session start (65536)
    std::vector<bool>  m_attrEdited;        // which tiles the user painted (65536)
    int  m_attrBaselineWorld = -1;          // world the baseline was taken on
    std::vector<BYTE>  m_serverBase;        // server's current TerrainData (65539)
    std::string m_serverBaseName;           // shown in the UI ("" = none loaded)
    int m_serverBasePickWorld = -1;         // map that was current when the base dialog was opened
    int  m_attrEditedCountCache = 0;        // cached "tiles edited" count for the status line
    bool m_attrCountDirty = true;           // recompute the cache only after paint/undo/baseline reset

    // Snapshots the baseline / clears the edit set when the map changes.
    void EnsureAttrBaseline(int world);
};

#define g_MapEditorUI CMapEditorUI::GetInstance()

#endif // _EDITOR
