# In-Game Map Editor (MuEditor)

An editor inside the game client: shape and paint a map's ground (height, textures, walkability,
light), place, move, turn and scale its objects, save into the game's own files, and review the
map's art models and ask Codex to rework them. It exists only in editor builds (`ENABLE_EDITOR`),
so the normal game is unaffected. Windows: presets `windows-x64-mueditor` and
`windows-x64-mueditor-release` ([console build](../../../../docs/build/windows/console.md)), then
`Main.exe --editor --world 1`; in a game session **F12** opens the editor. The quick start is for
the Mac; everything after it is reference.

### Quick start (macOS)

1. **Build** (repository root; the first build takes several minutes):
   ```sh
   PATH=/opt/homebrew/bin:$PATH cmake --preset macos-arm64-mueditor -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
   PATH=/opt/homebrew/bin:$PATH cmake --build --preset macos-arm64-mueditor-release --target Main
   ```
2. **Open Lorencia without a server** (`--world N` opens the folder `Data/WorldN`):
   ```sh
   cd out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS && ./Main --editor --world 1
   ```
   Over the editor's panels (and their gaps, previews and lists) you see the normal Mac pointer;
   over the 3D world the game's own cursor.
3. **Fly:** arrow keys, **PgUp/PgDn** up and down (**fn+Up/fn+Down** on a laptop), hold **Shift** to go
   faster, **right-drag** to look around (only while no tool is ticked). **Reset view** goes back.
4. **Objects:** Objects tab, tick **Enable object editing**, choose **Select & edit**, click an object
   (**Shift+click** or **Cmd+click** adds more). Drag it along the ground or use the gizmo: **W** move,
   **E** rotate, **R** scale (cursor over the 3D view; **Snap** for even steps). **Cmd+D** duplicates,
   **Backspace** deletes, **Drop to ground** puts it on the ground, **Esc** clears or cancels a drag.
   The **Outliner** checkbox opens a list of every object with a name filter; clicking a row selects it,
   flies there and switches the Objects tab to Select & edit.
5. **Undo/redo:** **Cmd+Z** / **Cmd+Shift+Z**, or the **Undo** / **Redo** buttons at the top, which name
   the step. Loading another map clears the history.
6. **Ground:** tick the tab's **Enable ...** box, then click or drag in the 3D view. **Height** (raise or
   lower, flatten, smooth, set height; objects ride along), **Texture** (layer 1 square, layer 2 round),
   **Attribute** (walkable, safezone, blocked, void, water), **Light** (add, subtract, tint, smooth).
   **[** / **]** change the brush size, **Shift+[** / **Shift+]** its strength.
7. **Save:** every tab has its own button (**Save objects**, **Save height**, **Save terrain textures**,
   **Save client .att**, **Save light**). A save writes the running game's `Data` and your checkout's
   `src/bin/Data/World1/...`, and first copies the file it replaces to `out/editor-backups/<time>/`. The
   line under the button lists the paths. Then commit like code: `git status`,
   `git add src/bin/Data/World1/<file>`, `git commit`; `git checkout -- src/bin/Data/<file>` throws an
   edit away. Brand-new files under `src/bin` need `git add -f`. Walkability also needs a server file
   for OpenMU (Attribute tab, step 2).
8. **Review art:** the **Assets** tab lists every Lorencia model with its status (`accepted`,
   `in-progress`, `unchanged`, `blocked`). **Highlight all** outlines its copies in magenta; the arrow
   buttons select one copy after the other and point the camera at it. **Looks good in client** /
   **Needs work** store your verdict in `assets-work/World1/client-review.json`; commit that file.
9. **Before and after:** once, in the repository, run
   `python3 tools/world_editor/materialize_variant.py original --world 1`. Then switch a model (or
   **All models**) between **Current** and **Original**. **Reload from disk** loads files Codex
   delivered. A model that says `as built` shows the last build's files: press **Current** first.
10. **Ask Codex for new art:** select the model (and the copy that matters), fly to a telling view,
   click **Flag for regeneration...**, fill in the kind, a one-sentence summary and the details, then
   **Create**. It writes `assets-work/World1/requests/<id>/`. Commit only that folder
   (`git add assets-work/World1/requests/<id>`, `git commit -m "docs(assets): file request <id>"`)
   and push it to `main` on your fork (`origin`). Codex only sees requests on `main`, and the commit
   your checkout was on must be pushed too (the dialog warns when it is not).

**Known limits:** offline there are no monsters, NPCs or game HUD. With the camera close to the ground
(under about 1000 units) objects near the bottom of the view can vanish: fly back a little. Brushes act
once per frame, so holding longer does more. The Light tab always saves `TerrainLight.OZJ` (Battle
Castle and Crywolf also load other light files). Leaves hide the highlight outline on trees. Only
Lorencia has an asset catalog. These tools were tested on the Mac only.

### Running on macOS

Apple Silicon, from the repository root. Keep `/opt/homebrew/bin` first on `PATH`: an
Intel Homebrew under `/usr/local` can otherwise leak x86_64 libraries into the build.

```sh
PATH=/opt/homebrew/bin:$PATH cmake --preset macos-arm64-mueditor -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
PATH=/opt/homebrew/bin:$PATH cmake --build --preset macos-arm64-mueditor-release --target Main
```

`macos-arm64-mueditor-debug` builds a much slower Debug client into `src/Debug/Main.app`
instead. To have Metal check every GPU call, start either client with
`MTL_DEBUG_LAYER=1 ./Main --editor`: the first invalid call then stops the client with an
error message in the terminal, which helps when something draws wrongly. The first build
takes several minutes. Run the client from the bundle's executable folder; it is also the
working directory the game reads `Data/` from:

```sh
cd out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS
./Main --editor
```

- **Server:** the first build seeds `config.ini` in that folder. Set `ServerIP` /
  `ServerPort` under `[CONNECTION SETTINGS]` to your server (local OpenMU: `127.0.0.1` /
  `44406`). The map editor works on the map you are standing on, so log in and enter a map,
  or open a map offline with `--world N` (next section).
- **Logs:** `MuError.log` (game) and `MuEditor/MuEditor_YYYYMMDD.log` (editor console), both
  in that folder.
- **Keys:** **F12** toggles the editor. If your keyboard's top row sends media keys (the
  macOS default on laptops), press **fn+F12**. **Cmd+Z** undoes the last stroke or edit on the
  Texture, Objects, Height, Attribute and Light tabs, **Cmd+Shift+Z** redoes it (Windows: Ctrl+Z, and
  Ctrl+Shift+Z or Ctrl+Y). In the Objects tab's Select & edit mode: **Backspace** deletes the
  selected objects (Delete works too; on Windows and Linux only Delete does), **Cmd+D** (Ctrl+D)
  duplicates them, **Esc** clears the selection (or cancels a drag), and with the cursor over the
  3D view **W**, **E** and **R** switch the gizmo between move, rotate and scale. On the brush
  tabs **[** and **]** make the brush smaller and larger, **Shift+[** and **Shift+]** weaker and
  stronger (the keys right of P; on a German keyboard **Ü** and **+**). None of these
  keys do anything while you are typing in a field, such as the Pos/Angle boxes, the console or
  the game chat. In a game session the editor keeps W, E, R and Esc to itself while it uses them,
  so they do not also use a potion or open the game menu.
- **Menus, lists and the colour picker:** while one is open, a click outside it only closes it
  (it never also selects, places or paints what lies behind), and **Esc** closes it without
  clearing the selection.
- **File dialogs** (Upload image, Convert a .tga, Load server base .att) slide down as a
  sheet on the game window. The game keeps running while it is open, and the button stays
  greyed out until you choose a file or cancel. The result applies to the map you were on
  when you clicked; if you changed map meanwhile, the texture upload and the server base are
  refused and say so.
- **Where saves go:** into the bundle's `Data` and into the repository's `src/bin/Data`,
  with a backup of the replaced repo file; see [Where saves go](#where-saves-go).

### Where saves go

Every save writes the file twice: the texture mapping, client `.att`, objects, height, light
and minimap saves, and the files the T. Browse and O. Browse imports create.

1. **The game's own `Data` folder** next to the executable (macOS:
   `Main.app/Contents/MacOS/Data/World{N}/...`), so the running client uses the change at once.
2. **The repository**: the same file under `<repo>/src/bin/Data/...`, which git tracks. The
   build copies `src/bin` over the game's `Data` whenever anything in `src/bin` changes, so a
   save that only lived in the game's `Data` would be lost at the next build (gotcha 9).

Before a repo file is replaced, its old bytes are copied to
`<repo>/out/editor-backups/<YYYYMMDD-HHMMSS>/Data/...`, named after the time of the save
(`out/` is not in git). If a save produces exactly the bytes the repo already has, nothing
is copied and no backup is made. To throw an edit away, copy the backup back, or run
`git checkout -- src/bin/Data/<file>` for a file git tracks.

- **Which repository:** the editor walks up from the game folder to the first folder that has
  both `src/bin/Data` and `.git` (a clone or a git worktree); the build output lies inside the
  checkout, so this finds it. To use another checkout, start the client with
  `MU_EDITOR_REPO_ROOT=/path/to/MuMain ./Main --editor`. The line under each Save button
  names the repository in use.
- **What was written:** after a save, the tab's status line lists the absolute path of each
  file: `game:` (the game's `Data`), `repo:` (in `src/bin/Data`) and `backup:`. The Editor
  Console and `MuError.log` log the same paths.
- **New files:** a file the repo did not have (an imported `ExtTileNN.OZJ`, an imported model
  and its textures, the first `mini_map.tga`) is created in `src/bin/Data`, but git ignores
  new files under `src/bin` (the `**/bin/**` rule in `.gitignore`), so `git status` does not
  list it. Add it with `git add -f <path>`; the status line points this out. Changed files
  that git already tracks show up in `git status` as usual.
- **Server `.att`:** still written next to the executable (`terrain_map{E}_server.att` and its
  `_HOWTO.txt`), and copied to `<repo>/out/editor-exports/`. It is uploaded in the Admin
  Panel, not shipped with the client.
- **No repository found** (a client outside a git checkout, or `MU_EDITOR_REPO_ROOT` naming a
  folder without `src/bin/Data`): saves go to the game's `Data` only, without a backup, plus a
  copy in a folder next to the executable (`World{N}/...`, `Object{N}/...`) that the next build
  does not overwrite. The line under each Save button says so, with the reason.

### Opening a map without the server

```sh
cd out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS
./Main --editor --world 1
```

On Windows: `Main.exe --editor --world 1`. `--world=1` works too; without `--editor` the
option is ignored.

- **The number is the map's folder in the client's `Data` directory:** `--world 1` opens
  `Data/World1` (Lorencia), `--world 3` Devias, `--world 4` Noria, and so on (1 to 82).
  The game itself counts maps from 0 (Lorencia is map 0, which is also the number OpenMU
  uses, see gotcha 6), but here you always give the folder number. Event maps whose levels
  share one folder open with that folder's number: Blood Castle `12`, Chaos Castle `19`,
  Kalima `25`, Illusion Temple `47`.
- **What happens:** the client skips the login screen and never connects to a server. It
  loads the map, shows it in the normal game view with the **FreeFly camera** looking down
  at 45 degrees on the middle of the map's objects (on Lorencia, the town; on event maps
  such as Blood Castle, the castle, which lies far from the map centre), and opens the Map
  Editor. A yellow line at the top of the Map Editor says the map is open offline.
- **Camera:** arrow keys fly, **PgUp/PgDn** go up and down (**fn+Up/fn+Down** on a Mac
  laptop keyboard, which has no Page Up/Down keys), **Shift** is faster, and
  **right-drag** looks around (only while no editing tool is switched on, because the
  tools use the right mouse button). **Reset view** in the Map Editor goes back to the
  start view. **F9** switches to the game camera, which looks at the empty start point;
  Reset view brings the FreeFly camera back. To find something on a large map, the
  Minimap tab's **Top-down view** shows the whole map from above.
- **Culling follows the FreeFly camera:** terrain and objects are drawn wherever you fly.
  This also applies in a normal game session while the Map Editor window is open. With
  only the Dev Editor's FreeFly spectator, the world is still culled as the game camera
  sees it, and its view is drawn as a wireframe. Known limit: with the camera close to the
  ground (less than about 1000 units from what it looks at), objects near the bottom edge of
  the view can disappear; fly back a little (or use the Outliner, which looks from 1800 units).
- **What works offline:** every Map Editor tab: texture painting, height, walkability
  (Attribute tab and its overlay), light, placing, moving and deleting objects, T. Browse,
  O. Browse and Minimap. **Saving works** and writes the same files as in a game session
  (see "Where saves go" above). The map looks as in the game: animated objects, water,
  fire and smoke. Falling leaves and birds, which normally follow your character, stay
  around the start point.
- **What does not:** there are no monsters, NPCs, other players or your own character
  (the server sends those), and no game HUD, chat or inventory. Lorencia's roofs stay on:
  they fade only when a character walks into a house.
- **Missing or incomplete map:** a map needs `EncTerrain{N}.map`, `.att` and `.obj`,
  `TerrainHeight.OZB` and `TerrainLight.OZJ` in `Data/World{N}`. If one of them is
  missing, or the number is out of range, the client writes one line to `MuError.log`
  and the Editor Console, for example
  `[Editor] --world 6: Data/World6/EncTerrain6.obj is missing - starting the normal login instead.`,
  and continues with the normal login screen.
- **Broken object records:** an object whose model number is impossible is left out when
  the map loads, online and offline, with one `MuError.log` line. Arena's shipped
  `EncTerrain7.obj` has one (`Data\World7\EncTerrain7.obj: skipped object 18 with invalid
  model type -515`). Saving the objects of such a map writes the file without that record.
- **Leaving:** close the window.

---

## 1. The core idea

The client already exposes, as **global arrays**, everything the editor needs,
and already ray-casts the mouse onto the terrain each frame **whenever the global
`EditFlag != EDIT_NONE`**. So the Map Editor is mostly:

1. Set `EditFlag` to the mode the active tab wants (`EDIT_MAPPING`, `EDIT_OBJECT`,
   `EDIT_HEIGHT`). The engine's terrain render pass then fills the picking globals
   `SelectXF`, `SelectYF`, `SelectFlag` (mouse-over tile) and `CollisionPosition`
   (world-space hit point) every frame.
2. On a captured mouse click over terrain, mutate the live global arrays.
3. On **Save**, write the arrays back to the map's real files (re-encrypting where
   the engine's own savers don't).

> **Note:** the legacy `Editor::EditObjects()` (`ZzzInterface`/`EditObjects.cpp`)
> that historically did the painting is gated on `#ifdef ENABLE_EDIT` — **not**
> `_EDITOR` — so it is **compiled out** of the editor build. The Map Editor
> therefore performs the actual painting/placement itself (reusing only the
> engine's picking + save functions).

---

## 2. Files

All under `src/MuEditor/UI/MapEditor/`:

| File | Responsibility |
|---|---|
| `MapEditorUI.h/.cpp` | The panel + all tabs (Texture, Objects, Height, Attribute, Light, T. Browse, Minimap, O. Browse, Assets). Owns UI state, edit-mode selection, input capture and the texture and walkability strokes; draws the Undo/Redo bar. |
| `MapBrushControls.h/.cpp` | `Editor::BrushControls` — what the round brushes share: the cursor on the ground (`TerrainBrushInput`), the Radius and Strength sliders, the **[** **]** keys, the brush circle and its outline on the ground. |
| `MapHeightTool.h/.cpp` | `CMapHeightTool` — the Height tab's brush (raise/lower, flatten, smooth, set height) and its stroke. |
| `MapGroundFollowers.h/.cpp` | `CMapGroundFollowers` — "Objects follow terrain": moves the objects standing on sculpted ground with it and hands their moves to the stroke's undo step (without selecting them). |
| `MapLightTool.h/.cpp` | `CMapLightTool` — the **Light** tab's brush (add, subtract, tint, smooth) and its stroke. |
| `MapLightSave.h/.cpp` | `Editor::LightSave` — saves the light map to `TerrainLight.OZJ` and lights the ground from what the file holds. |
| `MapEditHistory.h/.cpp` | `CMapEditHistory` — the multi-level undo/redo all editing tabs share: the Undo/Redo buttons (named after the next step) and keys, and the targets the steps apply to (`MapObjectWorld`, `MapTerrainLayers`). |
| `MapObjectWorld.h/.cpp` | `CMapObjectWorld` — the live world objects as the undo steps see them: an object is named by its place in the saved file (`OBJECT::SaveOrder`), creating, deleting and moving go through `Editor::ObjectPlace`. |
| `MapTerrainLayers.h/.cpp` | `CMapTerrainLayers` — the terrain arrays the brushes paint (tile layers, alpha, height, walkability, light map) as layers for the undo steps; rebuilds the normals and lit colours of the changed rectangle after a height or light step. |
| `MapObjectEditor.h/.cpp` | `CMapObjectEditor` — the Objects tab's Select & edit mode: the selection, picking and dragging, the gizmo, the fields, Drop to ground, Duplicate and Delete. |
| `TransformGizmo.h/.cpp` | `CTransformGizmo` — the move/rotate/scale handles drawn over the 3D view in ImGui's overlay, and the drag they measure. |
| `MapOutliner.h/.cpp` | `CMapOutliner` — the **Outliner** window listing the map's objects. |
| `MapEditorShortcuts.h/.cpp` | `Editor::Shortcuts` — the editor's keys (undo, redo, delete, duplicate, Esc, the brushes' [ ]), quiet while the user types; Esc is left to an open popup. |
| `MapEditorSave.h/.cpp` | `Editor::MapSave` — **encrypting** save of the terrain mapping (`.map`). |
| `MapAttributeSave.h/.cpp` | `Editor::AttrSave` — **encrypting** save of the walkability `.att` (client) **and** the plain `.att` for OpenMU's Admin Panel + a HOWTO text file (server). |
| `MapTextureBrowser.h/.cpp` | **T. Browse** tab — preview a chosen world's tile textures; "Use"/"Upload" a texture onto the current map. |
| `MapTextureImport.h/.cpp` | `Editor::TextureImport` — put a `.jpg`/`.ozj` into a free `ExtTile` slot and load it live. |
| `MapEditorFilePicker.h/.cpp` | `Editor::Files` — the shared "open file" dialog (SDL3, same on every platform, never blocks the game) used by Upload image, Convert a .tga and Load server base .att. |
| `MapEditorFileUtil.h/.cpp` | `Editor::Files` — portable `Data/World{N}` / `Data/Object{N}` paths and the map files in them. The save helpers every editor shares are in `MuEditor/Core`: `EditorFiles.h/.cpp` (whole-file read, `MirrorSavedFile` — the repository copy of each save, or the copy next to the executable without a repository — and the status-line text listing what was written) and `RepoMirror.h/.cpp` (finds the repository, `MU_EDITOR_REPO_ROOT` or the first folder above the game with `src/bin/Data` and `.git`, and copies a saved file into `src/bin/Data`, backing up the file it replaces in `out/editor-backups/`; file system only, unit-tested in `tests/editor/test_repo_mirror.cpp`). |
| `MapEditorStatusLine.h/.cpp` | `Editor::StatusLine` — the wrapped result line under a tab's buttons. |
| `MapObjectPlace.h/.cpp` | `Editor::ObjectPlace` — enumerate loaded models, placement position, reposition/remove objects, save `.obj`, and the one walk over the live objects (`ForEachLiveObject`) the Outliner, the undo steps and "Objects follow terrain" share. |
| `MapObjectImport.h/.cpp` | `Editor::ObjectImport` — cross-map object import (copy `.bmd` + textures into the current map's `Object` folder, load live), plus preview-load into a scratch slot. |
| `MapObjectBrowser.h/.cpp` | **O. Browse** tab — thumbnail grid of any map's object models; import onto current map. |
| `ObjectThumbnail.h/.cpp` | `CObjectThumbnail` — renders a loaded `BMD` model into a cached texture through the renderer's offscreen capture (`BeginOffscreenCapture`/`EndOffscreenCapture`), for the object thumbnail grids. |
| `MapAssetReview.h/.cpp` | `CMapAssetReview` — the **Assets** tab (catalog table, details, highlight, Prev/Next, client verdicts) and the catalog block in the Objects tab. |
| `MapAssetVariants.h/.cpp` | `CMapAssetVariants` — the Assets tab's current/original switches, **Reload from disk**, the **Shows** column and the hint to build the original files. |
| `RegenRequestDialog.h/.cpp` | `CRegenRequestDialog` — the **Flag for regeneration** dialog: fills a request from the catalog and the owner's text, takes the capture and writes the request folder. |

Outside that folder, `MuEditor/Core/OfflineWorld.h/.cpp` (`Editor::OfflineWorld`) opens a
map for `--world N` without a server: it checks the map's files, loads the map, enters the
main scene with an inactive hidden hero and points the FreeFly camera at the start view.
`MuEditor/Core/EditorCamera.h/.cpp` switches to the FreeFly camera (start view, Prev/Next),
and `MuEditor/Core/ViewCapture.h/.cpp` takes the request screenshot: one frame without the
editor overlay, the game cursor and the on-screen camera text. `MuEditor/Core/ModelHotReload.h/.cpp`
(`Editor::Assets::HotReload`) loads a model and its textures again while the client runs (the A/B
compare), between two frames. `MuEditor/Assets/` holds the
file-only part of the Assets tab, without ImGui or engine code, unit-tested in
`tests/editor/test_asset_catalog.cpp` and `test_regen_request.cpp`: `catalog.json` and
`client-review.json` (`AssetCatalog`, `ClientReview`), `request.json`, `brief.md` and the
request folder (`RegenRequest`, `RequestBrief`, `RequestFolder`, `RequestNaming`), the checkout's
commit and whether a file differs from git's index, read from `.git` (`GitCheckout`), SHA-256 and
git blob ids (`FileDigest`), the capture JPEG (`CaptureImage`, libjpeg-turbo), the light map file
(`TerrainLightFile`: `TerrainLight.OZJ` written and read as the client loads it, unit-tested in
`test_terrain_light_file.cpp`) and the shared text
helpers (`EditorText`: UTF-8 paths, trimming as the request validator does). For the A/B compare, `ModelPreflight` checks a model file and its
textures before they are loaded, and `AssetVariant` finds the original files that
`tools/world_editor/materialize_variant.py` builds; both are unit-tested in
`tests/editor/test_model_preflight.cpp`. `MuEditor/Editing/` holds the logic of undo/redo, the
selection, the gizmo and the terrain brushes, without ImGui or engine code, unit-tested in
`tests/editor/` (`test_edit_history.cpp`, `test_object_selection.cpp`, `test_gizmo_math.cpp`,
`test_terrain_brush.cpp`): the history (`CommandStack`, `EditCommand`, `EditCommandGroup` for a
step made of several parts), object steps (`ObjectEditCommand`, `ObjectWorld`), terrain steps
(`TerrainStroke`, `TerrainPatchCommand`, `TerrainLayers`), `ObjectSelection`, the gizmo's
projection, drag and rotation math (`GizmoMath`, `ObjectTransform`) and the round brushes: the
circle, its falloff and its clipped footprint (`TerrainBrush`), height and light values
(`FieldBrush`: add, move towards a target, smooth, clamp) and the overlay texture and hard-edged
cells (`SurfaceBrush`), and `PopupMouseGuard`, which keeps the mouse from the world while a popup
is open and until the click that closes it is released (`test_popup_mouse_guard.cpp`).

**Engine files touched (kept minimal, all editor-gated where possible):**

| File | Change |
|---|---|
| `MuEditor/Core/MuEditorCore.h/.cpp` | Added `m_bShowMapEditor`; render the panel each frame; call `CaptureInputForPainting()` in `Update()` (before the game consumes input). While an ImGui popup is open, and until the click that closes it is released, the mouse counts as over the editor (`Editor::Editing::PopupMouseGuard`, unit-tested), and Esc closes open menus and lists (not dialogs). Shows the OS pointer, and not the game cursor (drawn under ImGui), while the mouse is over any editor window or editor UI owns it; over the world the game cursor. Leaves the overlay and the game cursor out of a view-capture frame. Runs the Assets tab's model reloads at the start of a frame. Draws ImGui with the renderer's overlay pipeline (`mu::GetEditorOverlayPipeline()`). |
| `MuEditor/UI/Common/MuEditorUI.h/.cpp` | Toolbar **Map Editor** button + param. |
| `Render/Sprites/GlobalBitmap.h/.cpp` | `RefreshCacheEntry(index)` — invalidate one quick-cache slot (see gotcha #4). Editor only: `ReloadImage(index, file)` reads a texture from disk into the same index, keeping the other models' references (A/B compare). |
| `Camera/DefaultCamera.cpp`, `Camera/OrbitalCamera.cpp` | Their editor-only on-screen camera text is left out of a view-capture frame. |
| `src/CMakeLists.txt` | Editor build only: `src/ThirdParty` on the include path for nlohmann's `json.hpp` (catalog and request files). |
| `Camera/FreeFlyCamera.h/.cpp` | Widened `MAX_PITCH` to `-2` (near straight-down); `SnapTopDown()`; `RotateYaw()`; `LookAt()` (start view of an offline map). |
| `Camera/CameraManager.h` | `SetFreeFlyCullsWorld()` / `GetFreeFlyCullingCamera()`: while the Map Editor is open or a map is open offline, FreeFly culls with its own frustum instead of the spectated game camera's. |
| `Render/Terrain/ZzzLodTerrain.cpp/.h` | `g_bMapEditorFullTerrain` flag; `RenderTerrain()` forces `ResetFrustrumBoundsFullTerrain()` when set. Also `g_bMapEditorAttrOverlay` + `RenderAttributeOverlay()` (tints tiles by their `TerrainWall` bits, submitted in runs of 4096 quads so a wide or top-down view is tinted completely) and the Texture tab's square highlight, both drawn with the renderer and `TerrainOverlayState` (no `glPushAttrib`); the edit pass draws the round brushes' outline. FreeFly terrain/object culling uses `GetFreeFlyCullingCamera()`; the tile grid and the debug spheres and boxes (editor toggles) use screen-space lines. Player build too: `CreateTerrainNormal(_Part)` rebuild each normal from zero (see Height); new `CreateTerrainNormal_Rect` / `CreateTerrainLight_Rect` rebuild a rectangle exactly as the whole-map versions do (`CreateTerrainLight` shares their per-cell code and gives the same values as before, unit-tested). |
| `Render/Terrain/TerrainOverlayState.h/.cpp` | Editor only: the render state of the overlays on the ground (vertex colours, alpha blend, no depth writes, both faces), set and put back through the `ZzzOpenglUtil` wrappers so their state caches stay true. |
| `Render/Terrain/TerrainBrushOutline.h/.cpp` | Editor only: `Render::Terrain::BrushOutline`, the round brushes' circle on the ground (outer rim, and a fainter ring where a soft brush starts to fade). |
| `Render/Renderer/MuRenderer.h`, `MuRendererSDLGpu.cpp`, `ScreenLineRibbons.h/.cpp`, `QuadTopology.h` | `RenderScreenLines(vertices, widthPixels)`: lines a given number of pixels wide, facing the camera from any angle (also straight down), untextured, never culled; the ribbons are built in view space (`Render::Lines`, unit-tested in `tests/render/test_screen_lines.cpp`) and submitted in runs of at most one draw's capacity (`Render::Topology::ForEachQuadBatch`, 4096 quads), so a whole tile grid draws. `RenderLines` keeps its old world-space ribbons for the existing debug draws. |
| `Render/Renderer/GLCompatShim.cpp`, `LineTopology.h` | `GL_LINE_STRIP` and `GL_LINE_LOOP` are broken into segments (they were dropped before) and every `glBegin` line mode draws screen-space lines `glLineWidth` pixels wide. No code outside the editor uses `glBegin` lines. |
| `Camera/FrustumRenderer.cpp` | Editor only: the FreeFly frustum wireframe uses screen-space lines. |
| `Engine/Object/ObjectReferences.h/.cpp` | Player build too: `Engine::Object::ReleaseReferencesTo` drops a world object's `Operates[]` entries (and the operate under the cursor) and ends the effects, joints and particles attached to it, before the object is freed. |
| `Engine/Object/w_ObjectInfo.h/.cpp` | `OBJECT::SaveOrder`, the object's place in a saved `EncTerrain{N}.obj` (the record index it was loaded from, or after them for objects the editor added), also the name the undo steps use. Only the editor build sets it; the field is in every build so `OBJECT` has one layout (tests built with `_EDITOR` link the player's `MuClient`). |
| `Engine/Object/ZzzObject.h/.cpp` | Editor only: `g_MapEditorSelectedObjects` (every selected object gets an outline: the primary yellow, the rest orange); the loader sets `SaveOrder` and `SaveObjects` writes the records in that order. `g_MapEditorHighlightedTypes`, the Assets tab's "Highlight all" outline; `ObjectListGeneration()`, which `DeleteAllObjects` bumps, so the Map Editor notices a map unload and drops its selection and undo steps. Player build too: `DeleteObject` releases those references first; `DeleteAllObjects` does it for all objects in one pass (map change). `OpenObjectsEnc` and `SaveObjects` share `WorldObjectFile`; `SaveObjects` writes the encrypted file once, checks `fopen`/`fwrite`, counts only the records written and leaves out objects a map spawns at run time. |
| `Engine/Object/WorldObjectFile.h/.cpp` | The `.obj` record layout (encode/decode) and `InSaveOrder` (records sorted by their save order; without one, as collected), unit-tested in `tests/engine/`. |
| `World/MapInfra/MapManager.cpp` | `DeleteObjects` frees the map's objects with `DeleteAllObjects`. |
| `Scenes/MainScene.cpp` | Suppress the FreeFly frustum wireframe while `g_bMapEditorFullTerrain` is set (clean minimap shot) or while FreeFly culls the world itself. Offline (`--world`): no game HUD or HUD input. |
| `Scenes/WebzenScene.cpp` | After the start-up loading, `--world N` opens the map offline instead of going to the login screen. |
| `Scenes/SceneManager.cpp` | Offline: no "connection lost" check. |
| `World/GameMaps/GMBattleCastle.cpp` | Offline: skip the castle-owner request to the server while World31 loads. |
| `Render/Renderer/MuRendererSDLGpu.cpp` | Editor-only offscreen capture for thumbnails, rendered in the swapchain's colour format. Editor only: builds the ImGui overlay pipeline at start-up and hands it out through `mu::GetEditorOverlayPipeline()`. `k_DepthFormat` names the main pass's depth format (player build too, same value). |
| `Render/Renderer/SdlGpuEditorOverlayPipeline.h/.cpp` | Editor only: ImGui's own SDL GPU pipeline built again for the engine's main render pass, which has a D32 depth attachment; depth test and write are off. Without it Metal API validation stops the client at the first ImGui draw, and on some maps the panels drew empty. A copy of `imgui_impl_sdlgpu3.cpp`'s pipeline (see gotcha 15). |
| `App/Platform/Windows/Winmain.cpp` | `--editor`, `--world N` (with `--editor`) and `--enable-taskpool` are read from the portable command line, so they work on every platform. |

---

## 3. Tabs / features

### Round brushes

The Height, Attribute and Light tabs and the Texture tab's layer 2 paint with a round brush
around the ground point under the cursor. While the tab's tool is switched on, its outline is
drawn on the ground: a bright circle at the rim and, for a soft brush, a fainter circle where it
starts to fade. The outline follows the hills, is hidden behind walls and objects in front of
it, and keeps its width on screen from any angle, also looking straight down.

- **Radius** (in tiles, 0.5 to 20) on every brush tab, and **Strength** on the Height, Light and
  Texture (layer 2) tabs; the Attribute brush has a hard edge and no strength. **[** / **]**
  change the radius by half a tile, **Shift+[** / **Shift+]** the strength by one step, while you
  are not typing.
- **Soft edge:** a brush acts fully out to half its radius, then fades to nothing at the rim
  along a smoothstep curve, `w = 1 - t*t*(3 - 2t)` with `t` going from 0 where the fade starts
  to 1 at the rim, so there is no visible step where the fade begins or ends. The Attribute
  brush has a hard edge: a tile is painted when its centre is inside the circle.
- **Per frame:** while the button is held the brush acts once per frame, so holding it longer
  builds up (raise more, move closer to a target). Moving the mouse paints a trail; one press to
  release is one undo step.
- **Map edges:** the brushes stop at the map's edges. The original S6 editor (and this editor up
  to M6) wrapped around (`TERRAIN_INDEX_REPEAT`), so a stroke near the west edge also changed the
  east edge.
- On a rising hill the ground point under a still cursor creeps towards the camera, so the brush
  moves a little during a long raise.

### Texture (`EDIT_MAPPING`)
- Palette of the map's 30 tile slots (`Bitmaps[BITMAP_MAPTILE + 0..29]`), Layer 1
  (base) / Layer 2 (overlay) selector, **dropper** (Alt-click or toggle) to pick the tile under
  the cursor.
- **Layer 1** (the base texture, one tile each) paints a square: **Brush** sets its size
  (1 x 1 to 21 x 21 tiles, also with **[** / **]**). While painting is enabled the square is
  highlighted on the ground under the cursor (translucent fill and a bright outline) before you
  click.
- **Layer 2** (the overlay, blended over the base with an opacity per corner) paints with a
  round, soft brush: **Radius**, **Strength** (how much of the way to the opacity one frame goes
  at the brush's core) and **Overlay opacity** (the opacity the core ends at). Painting over a
  different overlay tile replaces it; the corners just outside the rim that have no overlay take
  the tile at opacity 0, so the soft edge fades out on every side. **Right-click** fades the
  overlay out with the same brush; a corner that reaches 0 loses its overlay tile.
- Paints `TerrainMappingLayer1/2[]` and `TerrainMappingAlpha[]`, clipped at the map's edges.
  Each stroke (press to release) is one step of [Undo and redo](#undo-and-redo).
- **Save** → `Editor::MapSave::SaveMappingEncrypted()` → `Data\World{N}\EncTerrain{N}.map`,
  and the repository copy (see [Where saves go](#where-saves-go)).

### T. Browse
- Scan every `Data\World*` folder, preview all `Tile*/ExtTile*/AlphaTile*` textures
  (loaded into a high scratch bitmap range).
- **Use selected / Upload image** → `Editor::TextureImport` copies the texture into
  the current map's next free `ExtTile` slot (`ExtTileNN.OZJ`, also created in the
  repository), loads it live, and selects it for painting.

### Objects (`EDIT_OBJECT`)
- **Place new:** thumbnail grid of the map's loaded models; yaw/scale/snap; left-click
  drops an object at the cursor (`CreateObject`, at `Editor::ObjectPlace::ComputePlacementPosition`).
  Each placed object is one undo step.
- **Select & edit:** left-click ray-picks a visible object (`CollisionDetectObjects`) and
  selects only it. **Shift+click** (or **Cmd+click**, Ctrl+click on Windows) adds an object to
  the selection or takes it out again. **Esc** clears the selection. A click on empty ground
  keeps the selection. The selection stays when you switch tabs and is dropped when the map
  changes. The last object you picked is the **primary**: the panel shows its model, catalog
  facts and fields.
- **Outlines:** the primary gets a yellow outline shaped like its model, every other selected
  object an orange one, and in Select & edit mode the object under the cursor a cyan one (what a
  click would pick). The outlines follow you to other tabs. Works on every platform.
- **Dragging an object moves the whole selection along the ground:** the point on the ground
  under the cursor pulls the objects along, keeping the offset between that point and each
  object from the moment you grabbed it, so a click does not make anything jump (the drag starts
  after a few pixels of mouse movement). Each object keeps its height above the ground, so a
  group walks up and down slopes instead of floating or sinking. When you grab a tall object by
  its top, the ground point is the one behind it along your view. A click that points at the sky
  only selects. A plain click (no drag) on one object of a selection selects only that one.
  **Esc** during a drag puts everything back.
- **Fields:** Pos, Angle and Scale show the primary's values. An edit changes every selected
  object by the same amount (type 100 more in Pos X and every object moves 100 units east; add
  45 to the Angle's last value and each turns 45 degrees on its spot). Scale never goes below
  0.05. Everything you type into a field until you leave it is one undo step.
- **Drop to ground** sets each selected object's height to the ground's under its origin (the
  same height a newly placed object gets). **Duplicate** (Cmd+D, Ctrl+D on Windows) places a copy
  of each selected object one tile (100 units) east, at the same height above the ground, and
  selects the copies. **Delete objects** (or the Delete key; on a Mac also Backspace) deletes the
  selection. Each of these is one undo step.
- Deleting, moving and undoing are safe for objects the game lets you click, such as
  Lorencia's chairs and pose boxes (the engine forgets its clickable-object entry and the
  effects attached to the object before freeing it; an undo registers them again).
- **Save** → engine `SaveObjects` → `Data\World{N}\EncTerrain{N}.obj` (encrypted), and the
  repository copy. The file gets the objects the map's file placed and the ones you added;
  objects a map spawns while it runs (Devias' donkey and warp gate) are left out, since the game
  creates those itself. If the file cannot be written, the status line says so and the old file
  is kept.
- **Record order:** every object keeps the place it had in the map's file, so saving without
  edits writes `EncTerrain{N}.obj` byte for byte as it was (and makes no backup), and after
  edits `git diff` shows only the records you changed. Objects you add or duplicate come after
  the file's records, in the order you made them; deleted ones drop out (the records after them
  move up by one). An undo that brings a deleted object back gives it its old place.
- **Changing map forgets the selection and every undo step** (texture, objects, height,
  attribute), also when the same map loads again (a teleport back, a trip to the character
  screen) or the Map Editor was closed meanwhile: they belonged to the objects and terrain
  that were unloaded.

#### Transform gizmo

With something selected in Select & edit mode (and **Enable object editing** on), handles are
drawn over the 3D view at the centre of the selected objects (for one object: its origin, where
it stands on the ground). They keep their size on screen however far away the objects are, and
editor windows are drawn over them.

- **Move** (**W**, or the Move button): three arrows move along the world's X (red, east), Y
  (green, north) and Z (blue, up); the yellow square moves in the ground plane at the same
  height (use **Drop to ground** afterwards on hilly ground).
- **Rotate** (**E**): three rings turn around the world's X, Y and Z axes; drag along a ring.
  A group turns around its centre and every object turns with it. The Z ring changes only the
  object's third angle (its heading); the X and Y rings tilt the object and set all three angles
  as the game reads them.
- **Scale** (**R**): drag the centre square or any axis handle away from the centre to grow,
  towards it to shrink. The scale is uniform; in a group the objects also move apart or
  together around the centre.
- **Snap:** moves in quarter tiles (25 units, the primary lands on that grid), turns in 15-degree
  steps and scales in steps of 0.1 (the primary's scale).
- The keys W, E and R work while the cursor is over the 3D view and you are not typing. While a
  drag runs, a label next to the cursor shows it ("Rotate Z 45.0 deg"); **Esc** cancels it. A
  whole drag is one undo step. The handles take the click: a click on a handle never picks or
  moves anything else, and the game does not get it.

### O. Browse
- Thumbnail grid of any map's object models (loaded on demand into a scratch slot
  `Models[MAX_MODELS]`, budget-throttled; failed ones show "n/a").
- **Use selected on current map** → `Editor::ObjectImport::UseModelOnCurrentMap`
  copies the `.bmd` + its textures into `Data\Object{current}\` at a free slot,
  loads it live, and jumps to Objects → Place. The copied files are created in the
  repository too (new files: `git add -f`). Persists on reload for **generic**
  maps (special maps like Lorencia/World1 that don't use the `Object{N}.bmd` scheme
  are live-only).

### Outliner

The **Outliner** checkbox at the top of the Map Editor opens a window with every object on the
loaded map: its name (the asset catalog's, else the model's own), type, position in tiles
(100 world units), height, angle and scale. It works on every tab and shares the selection
with the Objects tab.

- **Filter by name** keeps the rows whose name contains the text (any case); the type list
  next to it shows one model only (with how many are placed).
- **Click** a row to select the object and point the free-fly camera at it (in a game session
  this switches to the free-fly camera; **F9** goes back). **Shift+click** or **Cmd+click**
  adds or removes a row without moving the camera. Any selection made here also switches the
  Objects tab to **Select & edit**, so the next click in the world picks or drags instead of
  placing a new object.
- **Select all of type N** selects every instance of the primary's model; **right-click** a
  row for the same with that row's model.
- Long lists scroll smoothly: only the visible rows are drawn.

### Undo and redo

The Texture, Objects, Height, Attribute and Light tabs share one history of steps for the loaded map:
**Undo** takes back the newest step, whatever tab it came from, **Redo** makes it again. The
buttons at the top of the Map Editor name the step they would apply ("Undo: Rotate 3 objects",
"Redo: Paint texture").

- **Keys:** Cmd+Z undo, Cmd+Shift+Z redo on a Mac; Ctrl+Z, and Ctrl+Shift+Z or Ctrl+Y, on
  Windows and Linux. Not while typing in a field.
- **A step** is one brush stroke (press to release) on the Texture, Height, Attribute or Light
  tab, one placed object, or one object edit (a drag, a gizmo drag, a field edit, Drop to ground,
  Duplicate, Delete), also when it changes many objects. A height stroke and the objects that
  followed the ground are one step.
- **While a stroke or a drag is still held**, Undo and Redo are unavailable: the buttons are
  greyed out and the keys do nothing. Press them again after you release the mouse.
- A new step after an undo drops the steps you could have redone.
- Undoing or redoing an object step selects the objects it changed (the primary stays the
  same), so you see what came back. Undoing a height stroke leaves the selection as it is, also
  when objects moved with the ground.
- **How much is kept:** up to 64 MB of steps; beyond that the oldest are forgotten. A stroke
  keeps only the rectangle of cells it changed (a brush stroke a few KB, a stroke over the whole
  map about 1 MB); an object step a few hundred bytes.
- An undo never rebuilds the other objects: it moves, deletes or re-creates only the ones the
  step changed, and a re-created object gets back its place in the saved file. Undoing a height
  or light step rebuilds the lighting exactly as it was.
- **Changing map** (or reloading the same one) clears the history.
- If a step no longer matches the map (should not happen), the history is cleared and a yellow
  line says so.

### Height (`EDIT_HEIGHT`)
A round, soft brush on `BackTerrainHeight[]` (see [Round brushes](#round-brushes)) with four
tools:

- **Raise / lower:** left-click raises, right-click lowers. **Strength** (1 to 40) is the height
  added per frame at the brush's core, in world units (one tile is 100).
- **Flatten:** drags the ground towards its height under the cursor where you pressed, so a
  stroke across a slope levels it at that height.
- **Smooth:** moves each corner towards the average of itself and its four neighbours, the
  five-point average the original S6 editor used to smooth its light map. Every corner is
  averaged from the heights before the frame (the original worked in place, so the result
  depended on the order it visited the corners); at the map's edge a missing neighbour counts
  as the corner itself.
- **Set height:** brings the ground to **Target height**. **Alt-click** takes the target from
  the ground under the cursor (it changes nothing).
- For Flatten, Smooth and Set height, **Strength** is the share of the remaining difference one
  frame closes at the brush's core: 40 lands the core on the target at once, 10 goes a quarter
  of the way each frame.
- **Objects follow terrain** (on by default): objects standing where the ground moves keep their
  height above it (a house on a hill you raise rises with it). An object stands on the ground of
  the tile it is on, so an object on a tile next to the rim moves by the little the rim changed.
  The moves belong to the stroke's undo step.
- After each frame only the brush's rectangle and one cell around it get new normals and light
  (a cell's normal reads its own corner and the ones towards +x and +y, and a light value only
  its own normal). The result is the same as a whole-map rebuild, bit for bit (unit test on a
  test map; checked in the running client after every stroke), without redoing all 65536 cells
  every frame the button is held. Each stroke is one step of [Undo and redo](#undo-and-redo),
  which restores the heights and the lighting exactly.
- **How the ground is lit:** each cell's normal is the sum of the unit normals of its two
  triangles (so it is up to 2 long), and its brightness is the painted light map
  (`TerrainLight.OZJ`) times `clamp(dot(normal, (0.5, -0.5, 0.5)) + 0.5, 0, 1)`: flat ground
  gets the full light-map colour and slopes facing away from the light get darker. The
  normals are rebuilt from the heights every time. The original S6 client added them to the
  previous values instead, so every recompute (each later map load, including the game map
  after the login and character screens, and every sculpt stroke) made them longer and washed
  out the slope shading. Since M3 each load looks like a first load, in the game too.
- **Save** → engine `SaveTerrainHeight` → `Data\World{N}\TerrainHeight.OZB` (4 prefix
  bytes followed by a plain BMP; the save keeps the file's existing prefix).
- **Height is capped at `255 * factor`** (factor 1.5 normally, 3.0 on the login scene):
  the file stores one byte per cell as `height / factor`, so anything higher can't be
  saved and collapses to a flat plateau on reload. The sculpt clamps `BackTerrainHeight`
  to that ceiling each stroke, so the editor never shows a height that won't persist.

### Attribute (`EDIT_WALL`) — walkability
- Paints `TerrainWall[]` (**sets**, not ORs, the value — same as the standalone
  `_upscale/terrain_editor.html` tool). Left-click paints the selected attribute,
  right-click resets to Walkable. A round brush with a hard edge: **Radius** (or **[** / **]**)
  in tiles; a tile is painted when its centre lies inside the circle (radius 0.5 paints the tile
  under the cursor, 1 a small plus). It stops at the map's edges. Each stroke is one step of
  [Undo and redo](#undo-and-redo).
- Brushes are the **byte-sized** `TW_*` bits the `.att` stores: Walkable `0`
  (= normal combat area), Safezone `1`, Blocked/no-go `4` (`TW_NOMOVE`), NoGround
  `8` (`TW_NOGROUND`), Water `16` (`TW_WATER`).
  > `TW_NOATTACKZONE` (`0x100`) and friends **don't fit in a byte** and so can't be
  > stored in the byte-per-cell `.att` these maps use — don't add them as brushes.
- **In-world overlay** (`g_bMapEditorAttrOverlay` → `RenderAttributeOverlay()`)
  tints each tile so you can read the walk map while painting: red = blocked,
  green = safezone, black = void, blue = water, faint grey = walkable.
- **Two saves are needed — the client and the server each keep their own walk map:**
  1. **Save client .att** → `Data\World{N}\EncTerrain{N}.att` (see the format table).
  2. **Save server .att** → `terrain_map{E}_server.att` **+ a `_HOWTO.txt`** next to
     `Main.exe`, copied to `<repo>/out/editor-exports/` as well. Upload it in the Admin
     Panel's **"Terrain Data"** field for that map, then **restart the game server**.
- ⚠️ **The two map numbers are different.** The server keys maps by `"Number"` = the
  **world enum** (Lorencia `0`, Arena `6`); the client folder is `World{enum + 1}`
  (`World1`, `World7`). The tab prints both side by side — edit the Admin Panel map
  whose `Number` matches the one shown, not the client folder number.
- If client and server disagree, players walk through walls and get rubber-banded
  back. Always ship both.

**Applying the server file**: Admin Panel → map list → edit the map with that
`Number` → **"Terrain Data"** → upload `terrain_map{E}_server.att` → Save → restart
the game server.

> ⚠️ **Never upload the client's `EncTerrain{N}.att` to the Admin Panel.** See gotcha 14.

### Light (`EDIT_LIGHT`)
Paints the map's **light map** (`TerrainLight`, one colour per corner, 0 to 1 per channel), the
colour the ground is multiplied with before the slope shading (see Height, "How the ground is
lit"): darker patches under trees, warm light around a fire, a blue tint by the water. Most
objects are lit from the light map under them too.

- **Enable light painting**, then pick a mode: **Add** adds the **Colour** times the **Strength**
  (the intensity) per frame at the brush's core, **Subtract** takes it away, **Tint** moves the light towards the
  colour (Strength is then the share of the way per frame: 1 lands on the colour at once) and
  **Smooth** evens the light out towards each corner's neighbours (the five-point average, as for
  Height). **Right-click** always smooths. The brush is round and soft, with **Radius** and **[**
  / **]** as on the other tabs; values stay within 0 to 1.
- Add brightens every channel of the colour until it reaches 1: adding a red to a light map that
  is already bright turns the core almost white and leaves a red ring where it fades. Use Tint to
  give an area a colour.
- The ground's lit colour is rebuilt for the brush's rectangle after each frame (the same values
  as a whole-map rebuild). Each stroke is one step of [Undo and redo](#undo-and-redo).
- **Save light** writes `Data/World{N}/TerrainLight.OZJ` and the repository copy (with a backup,
  see [Where saves go](#where-saves-go)): a 256 x 256 JPEG (quality 100, no colour subsampling)
  behind 24 prefix bytes that repeat the JPEG's first 24, the layout `tools/mu_texture.py` wraps
  and the loader (`OpenJpegBuffer`) skips. The loader reads the JPEG bottom row first, so the
  file's bottom row is the map's row y = 0.
- **What you see after saving is what the next load shows:** a JPEG loses a little (a few steps of 255
  per channel on detailed light, under 5 in the tests; flat light not at all), so the editor reads the file back as the loader does
  and lights the ground from it. Checked on Lorencia: after a save and a map reload the light
  map, the lit colours and the normals were bit for bit the same, and the screenshots differed
  only where the water moves.
- The engine's own `SaveTerrainLight` (unused) wrote a plain JPEG without the prefix, which the
  loader cannot read, and cut each value down instead of rounding it.
- Some maps load another light map in some states: Battle Castle (World31) `TerrainLight2.OZJ`
  during the siege, Crywolf (World35) `TerrainLight1/2.OZJ` when occupied or at war. The Light
  tab always saves `TerrainLight.OZJ`.

### Minimap
- **Generate minimap from tiles** builds the map's `mini_map.OZT` (drop-in) **and** an
  editable `mini_map.tga`, both 1024×1024, in `Data\World{N}\` and in the repository.
  It is not a screenshot: each pixel is the average colour of the tile textures at that
  terrain cell, using the same cell-to-pixel mapping the game uses for the position
  marker, so the marker lands exactly on the player. It works from anywhere on the map,
  with no camera setup and no GPU read-back.
- **Convert a .tga to mini_map.OZT...** opens a file dialog for an edited `.tga` (for
  example the generated one with a screenshot painted over it) and wraps it into
  `mini_map.OZT`, normalising orientation, bit depth and RLE compression. Relog to see it.
- **Top-down view** is only for looking at the map: it switches to the FreeFly camera,
  snaps it above the map centre looking straight down (`SnapTopDown`), and renders the
  whole terrain and all objects (`g_bMapEditorFullTerrain` + `g_Camera.TopViewEnable`).
  All four **arrows pan**, the **mouse wheel zooms** (PgUp/PgDn too, fn+Up/fn+Down on a Mac
  laptop), Shift pans faster,
  **Rotate 90 CW/CCW** turns the view. **Back to game camera** leaves it.
- **Important:** the top-down render flags are only written while the mode is active and
  released **once** on exit — never every frame — because the game's own minimap uses
  `TopViewEnable` too (see gotcha #5).

### Assets tab

Every model of the map's asset catalog, what state its rework is in, and a way to ask the art
builder (a Codex agent following [`ASTRA.md`](../../../../ASTRA.md)) to rework it. The tab reads
`assets-work/World{N}/catalog.json` from the repository the saves go to
([Where saves go](#where-saves-go)). So far only Lorencia (`World1`) has a catalog; other maps
show "No catalog for this world". The catalog is generated by
`assets-work/World1/coordination/build_editor_catalog.py`; **Reload** reads it again.

- **Table:** Name, Type (the object type number), Status (as on the asset board: `accepted`,
  `in-progress`, `unchanged`, or `blocked` for models kept out of the art scope), Placed (how many
  are on the loaded map now; hover for the number in the shipped map file), Client (your
  verdict), Shows (which files the client shows, see
  [Current and original files](#current-and-original-files-ab-compare)) and Identity. Type in the filter box to search names and identities, pick a status in
  the list next to it (it also offers "needs work" and "open request"), and click a column header
  to sort.
- **Details** of the selected row: identity, BMD file (rebuilt or original), each texture and the
  other models that share it (a shared texture can only change when all of its models are reworked
  together), the engine's rules for the model in yellow, the batches that produced it (newest first,
  with their notes files), **Open baseline preview** / **Open final preview** (the offline renders
  before and after the rebuild) and the model's open requests.
- **Highlight all** outlines every instance of the model in magenta, on every tab, until you untick
  it or close the Map Editor. The outline follows the model's surface, so leaves hide most of it on
  trees; walls, fences and props show it clearly.
- **<** and **>** step through the placed instances, select the instance (yellow outline, as a click
  in the Objects tab, which switches to **Select & edit**) and point the free-fly camera at it from
  50 degrees above, so it is in the middle of the view. In a game session this switches to the
  free-fly camera; **F9** goes back.
- **Looks good in client** / **Needs work** (with an optional note) record your verdict in
  `assets-work/World{N}/client-review.json`, a file only you edit. Commit it; running
  `python3 assets-work/World1/coordination/build_editor_catalog.py` copies the verdicts into
  `catalog.json`. A verdict does not ask for rework; flag the model for that.
- **Objects tab:** the selected object shows its model name, status and your verdict, with
  **Flag for regeneration...** and **Show in Assets tab**.

#### Current and original files (A/B compare)

The running client can show each catalog model with its **current** files (the checkout's
`src/bin/Data`, what the game ships) or its **original** files from before the art rebuild, and
switch between them without a restart. It only changes what this client shows: nothing is saved
and no file is changed.

1. **Build the original files** once, and again whenever `catalog.json` changes. In the
   repository:

   ```sh
   python3 tools/world_editor/materialize_variant.py original --world 1
   ```

   It writes `out/ab/original/Data/Object1/` from git (Lorencia: 115 models and 105 textures,
   about 4 MB; `out/` is not in git): each model at its catalog `original.revision`, checked
   against `original.sha256`, and every texture that original model names, from the same
   revision. Until the folder exists, or when it was built from an older `catalog.json`, the tab
   shows the command with **Copy command** and **Check again**; while the folder is missing, the
   **Original** buttons stay greyed out. The shown command names the script by its full path, so
   it runs in a terminal from any folder: `python3 ...` on macOS and Linux, `py -3 ...` on
   Windows (the Python launcher python.org installs; with another Python, type `python` instead).
2. **All models: Current / Original** loads every model of the list again from `src/bin/Data` or
   from `out/ab/original`, a few per frame (the line next to the buttons counts them). **Current**
   also picks up files that changed on disk since the map was loaded.
3. **One model:** in the details, **Show: Current / Original** switches the selected model.
   **Reload from disk** loads it again from the files it shows now, for example after Codex
   delivered new files into `src/bin/Data`: no build, no restart.
4. **What a model shows** is in the **Shows** column and next to the switch: `as built`,
   `current`, `original` or `mixed`. `As built` is what the map load read from the game's own
   `Data` folder, the copy of `src/bin/Data` the last build made; after a `git pull` without a
   build it is older than `src/bin/Data`. `Current` means the model was loaded from `src/bin/Data`
   in this session. Mixed means the model file comes from one side and at least one of its
   textures from the other. That is how shared textures behave: a texture belongs to every model
   that uses it, so switching Tree01 also switches the leaves of Tree02 (the details name those
   models). Switch all models, or all models of a texture, for a clean comparison.
5. **Checked before loading:** the model file must be a BMD the client reads, within the
   engine's limits (50 meshes, 15000 vertices per mesh, 200 bones, each mesh on its own texture
   slot) and without indices that point past its data, and every texture it names must be next
   to it as a readable `.OZJ`/`.OZT` of at most 1024 px. Anything else is refused with the reason
   in the status line and the Editor Console, and the model keeps what it showed. A missing
   texture never closes the game here.
6. **Limits:** a map load (a teleport in a game session, or the next start) reads the game's own
   `Data` folder again, so every model shows `as built`: the files of the last build, not
   necessarily `src/bin/Data`. **Before a verdict on files you just pulled or Codex just delivered,
   press Current** (one model or all), or rebuild. Models the code spawns (birds, fish,
   butterflies) have no switch. The engine's per-model rules (the yellow lines) apply to the
   original files as they did before the rebuild.

**Flagging a model for regeneration:**

1. Select the model and, if one placement matters, an instance (**<** / **>**, or a click in the
   Objects tab). The request records where it stands, and its record number in `EncTerrain1.obj`
   unless that file has changes you have not committed (then the numbering differs from the one
   at your commit, and the dialog says the number is left out).
2. Fly the camera to the view the art builder should see: the capture is the 3D view as it is on
   screen, without the editor panels.
3. Click **Flag for regeneration...** and fill in:
   - **Kind:** repaint (new textures), remodel (new mesh), both, or new variant (an extra model
     derived from this one, such as a broken fence; it installs nothing, and you add it to the map's
     model table yourself).
   - **Priority**, a one-sentence **Summary** (its first words name the request folder), and
     **Details**, **Keep** and **Avoid**, one item per line.
   - **Also rework ...** appears when other models share this model's textures. Without it those
     textures are frozen; the dialog lists the files the art builder may replace and warns when a
     repaint could change nothing.
   - **Include the current view** attaches the screenshot. **Codex may push its branch and open a
     PR** allows exactly that on the fork, never a merge.
4. **Create** writes `assets-work/World{N}/requests/<date>-<model>-<slug>/` with `request.json`,
   `brief.md` (the same request as text) and `captures/01-current.jpg`, and runs nothing else.
   **Open folder** shows it in Finder or Explorer. Check it with
   `python3 assets-work/World1/requests/validate_request.py assets-work/World1/requests/<id>`.
5. **Codex only sees the request once it is on `main` on GitHub.** Commit just that folder
   (`git add <folder>`, then `git commit -m "docs(assets): file request <id>"`) and push it to
   `main`; from another branch, cherry-pick the commit onto `main`. The request names your
   checkout's current commit as its base, so that commit must be pushed to `origin` (your fork)
   as well: the dialog warns when no branch on `origin` has it (a branch of another remote, such
   as the upstream project, does not count), and when the BMD of any model the request names
   differs from the catalog in your checkout (commit or restore it first).
6. Then the coordinator assigns the request to one Codex worker, who works on its own branch
   `codex/lorencia-req-<model>-<slug>`, delivers into the request folder's `delivery/`, and the
   coordinator accepts or rejects the result on `main`. To take a request back, withdraw it as
   described in [the request README](../../../../assets-work/World1/requests/README.md), which is
   also the full contract.

---

## 4. On-disk formats & save encryption

Files live in `Data\World{N}\`. **World folder = world enum value + 1**
(`iMapWorld = WorldActive + 1`; a few maps remap — see `MapManager`). Ciphers:
`MapFileDecrypt`/`MapFileEncrypt` (`ZzzLodTerrain.h`), `BuxConvert` (`_crypt.h`).

| Data | File | Format | Editor save |
|---|---|---|---|
| Tile mapping | `EncTerrain{N}.map` | `mapdecrypt` only. `version, mapNum, Layer1[256²], Layer2[256²], Alpha[256²]` | `MapEditorSave` builds the plaintext and encrypts it with `MapFileEncrypt`. (The engine `SaveTerrainMapping` also encrypts: it writes the plaintext, reads it back and rewrites it encrypted.) |
| Walkability (client) | `EncTerrain{N}.att` | Decode = `bux(mapdecrypt(f))` → `[version, mapId, 255, 255] + attr[256²]`, **1 byte/cell**, index `y*256+x` (same order as `TerrainWall[]`). | **We re-encrypt**: `MapFileEncrypt(BuxConvert(plain))` (`MapAttributeSave`). The engine `SaveTerrainAttribute` writes **plaintext AND 2 bytes/cell** — unusable. |
| Walkability (server) | `terrain_map{E}_server.att` | The **exact bytes** of OpenMU's `config."GameMapDefinition"."TerrainData"` (bytea): `[0, 255, 255] + attr[256²]` = **65539 bytes, NOT encrypted** (attr `i` at offset `3+i`). Same layout as OpenMU's own `Resources\Terrain{N}.att` seed files. Row keyed by `"Number"` = world **enum**. | Written verbatim (`SaveServerAtt`) — no `BuxConvert`, no `MapFileEncrypt`, and **no `mapId` byte** in the header. Upload via the Admin Panel's **"Terrain Data"** field. |
| Objects | `EncTerrain{N}.obj` | `mapdecrypt`. `version, mapNum, count, {Type i16, Pos[3] f32, Angle[3] f32, Scale f32}×count`. **Flat list; re-blocks by position on load.** | Engine `SaveObjects` encrypts and writes the file in one go; `count` is the number of records written. Every shipped `.obj` decodes and re-encodes byte for byte (unit test). |
| Height | `TerrainHeight.OZB` | **4 prefix bytes**, then a plain **BMP**: 1080-byte header + 256×256 8-bit grayscale (66620 bytes in all); `height = byte × 1.5` (× 3.0 on the login scene). The loader requires the prefix and remembers it. Loader appends `OZB` to the `.bmp` name. | Engine `SaveTerrainHeight` — call directly. It rewrites the target file's own prefix (or the last loaded one for a new file). Any external writer must keep the prefix. |
| Light map | `TerrainLight.OZJ` | 24 prefix bytes (a copy of the JPEG's first 24), then a 256×256 RGB JPEG, decoded bottom row first: pixel row 0 of the decoded buffer is the map's row y = 0; light = byte / 255. | `Editor::LightSave` (Light tab) encodes quality 100, 4:4:4, and reads the file back. The engine `SaveTerrainLight` writes no prefix — not loadable. |
| Minimap | `mini_map.OZT` | 4-byte header `00 00 02 00` + 18-byte TGA header + BGRA pixels, **bottom-origin**, 32-bit, **no footer**. 1024×1024 for Arena. | Wrapped externally (strip 4 bytes → TGA to edit; prepend them back → OZT). |

**Tile slots:** a mapping cell stores an index 0–29 → `BITMAP_MAPTILE + index`.
Slots 0–13 are the fixed `Tile*` set, 14–29 are `ExtTile01..16` (the spare slots the
importer uses). Textures are stored as `.OZJ`/`.OZT` but referenced by `.jpg`/`.tga`
names — `LoadBitmap` swaps the extension.

**Object models:** generic maps use `Object{i}.bmd` (model `Type = i-1`, zero-padded
for i<10); some maps (Lorencia) use named files via a hand-coded model table. The
model + its textures live in `Data\Object{N}\`.

---

## 5. Gotchas learned the hard way

1. **Legacy painter is compiled out.** `Editor::EditObjects()` is `#ifdef ENABLE_EDIT`,
   not `_EDITOR`. The Map Editor does its own painting/placement.
2. **Mouse *held* state.** The game's `MouseLButton` is set on the Windows press/release
   *edge*, and the editor clears it every frame to suppress walking — so it only reads
   `true` on the press frame. For click-**drag** (paint strokes, object drag), read the
   held state from **`ImGui::IsMouseDown()`** instead (see `CaptureInputForPainting`).
3. **Save encryption differs per format.** The engine's `.att` saver writes **plaintext**
   (and 2 bytes/cell) → the editor encrypts itself (`BuxConvert`, then `MapFileEncrypt`).
   `SaveTerrainMapping` (`.map`), `SaveObjects` (`.obj`) and `SaveTerrainHeight` (`.OZB`)
   produce loadable files as-is. The editor writes `.map` itself (`MapEditorSave`), which
   also checks for write errors.
4. **Quick-cache invalidation.** `Bitmaps[]` uses a quick-cache that caches even *empty*
   slots (the palette polls all 30 every frame). After `LoadBitmap`-ing into a slot that
   was polled while empty, call `Bitmaps.RefreshCacheEntry(index)` or the new texture
   won't appear until a restart.
5. **Don't clobber shared globals.** `g_Camera.TopViewEnable` is used by the game's own
   minimap. Writing it every frame (even `false`) crashes the game minimap. Only write
   editor-owned globals while the editor owns the mode; release once on exit (edge-detect).
6. **World off-by-one.** `Data\World{WorldActive+1}\`. Special maps remap (Blood Castle,
   Chaos Castle, Hellas, Cursed Temple…). The Texture tab exposes a manual world override.
   **And the server disagrees with the client:** OpenMU keys maps by `"Number"` =
   the raw world **enum** (Lorencia `0`, Arena `6`), while the client folder is
   `enum + 1` (`World1`, `World7`). Mixing them up is the #1 way to write the wrong map.
7. **MSVC comment trap.** A `//` comment ending in a backslash (`...Data\`) continues onto
   the next line and silently eats it. Bit us twice — don't end comments with `\`.
8. **Thumbnails use the renderer's offscreen capture, not OpenGL.** There is no GL
   framebuffer object or `wglGetProcAddress` any more: `ObjectThumbnail` wraps its draw in
   `mu::GetRenderer().BeginOffscreenCapture()`/`EndOffscreenCapture()` (editor-only), and the
   SDL GPU renderer replays those draws into a texture in the swapchain's colour format.
9. **Asset-copy mtime gating.** `CopyAssets` is stamp-gated by source mtime: a change
   anywhere under `src\bin` re-copies all of `src\bin` over the build output's `Data`, which
   replaces files the editor saved there. That is why every save is also copied into
   `src\bin\Data` ([Where saves go](#where-saves-go)). Delete the
   `.assets_copied_Release.stamp` to force a re-copy if the output `Data` is deleted.
10. **Thumbnail render** needs the global `BoneScale = 1` and the sequence
    `Animation → Transform → RenderBody`. Display the texture **without** a UV flip: SDL GPU
    render targets are top-down, like ImGui.
11. **`TerrainWall[]` is not pure map data — never persist it raw.** `MoveCharactersClient()`
    clears and re-stamps `TW_CHARACTER` (`0x02`) **every frame** on every tile a live
    character/NPC occupies. Saving verbatim bakes "an NPC stood here when I hit Save" into
    the map (a safezone tile becomes `1|2 = 3`). The server then reads `3` as **blocked**
    (see #12) while the client still reads it as walkable → **rubber-banding**. `MapAttributeSave`
    masks the bit out (`StaticAttribute()`); any new attribute save must do the same.
12. **Client and server interpret the attribute byte DIFFERENTLY.** Byte-identical is *not*
    behaviour-identical:
    - **Server** (`GameMapTerrain.cs`, exact value): `walkable = (v==0 || v==1)`,
      `safezone = (v==1)`. So **water `16` blocks**, and *any* combination (`3`, `5`, …) blocks.
    - **Client** (`_define.h`, bit test): `blocked = v & 4 || v & 8`, `safezone = v & 1`.
      So `16` does **not** block and `3` is walkable.

    Paint clean single values (`0`/`1`/`4`/`8`) and the two agree. `_upscale/server_att.py`
    can dump the server's walk map, **diff** it against a client `.att`, and export a
    behaviour-faithful `.att` (`--normalize`).
13. **The `.att` loader has anti-tamper sentinels.** `OpenTerrainAttribute()` hard-checks one
    magic tile per map and calls `ExitProgram()` ("data error", client closes) on mismatch:
    Lorencia `(135,123)=5`, Dungeon `(227,120)=4`, Devias `(208,55)=5`, Noria `(186,119)=5`,
    Lost Tower `(193,75)=5`. It also rejects any byte `>= 128`. Anything that rewrites a `.att`
    must preserve these (`server_att.py` does, and validates before writing).
14. **The Admin Panel's "Terrain Data" upload stores the file VERBATIM.** It is
    `ByteArrayField.razor`, a *generic* `byte[]` widget — it does **not** decrypt and it does
    **not** fix up the header (`CurrentValue = memoryStream.ToArray()`, that's the whole
    handler). So **never** upload the client's `EncTerrain{N}.att` to it. That file breaks it
    two ways: it is **encrypted** (the server would read ciphertext as walkability flags), and
    its plaintext header is **4 bytes** (`version, mapId, w, h`) against the server's **3**
    (`version, w, h`) — which alone would shift the entire map by one tile, since
    `GameMapTerrain` does `ReadTerrainData(terrainData.AsSpan(3))`. Upload
    `terrain_map{E}_server.att` (from **Save server .att**) instead: it is written in exactly
    the server's layout.
15. **Updating the imgui submodule? Check the overlay pipeline.** ImGui draws inside the engine's
    main render pass with `SdlGpuEditorOverlayPipeline.cpp`, a copy of the pipeline that
    `src/ThirdParty/imgui/backends/imgui_impl_sdlgpu3.cpp` builds (its shaders from
    `imgui_impl_sdlgpu3_shaders.h`, the `ImDrawVert` layout, blend and raster state) plus the
    pass's depth format. No test compares the two. After an update, compare them and carry any
    change over. If the copy no longer matches, ImGui draws wrongly, or the client logs
    `editor overlay pipeline creation failed` and falls back to ImGui's own pipeline, which brings
    back the Metal validation stop and the empty panels on Devil Square.
16. **Drawing lines and overlays on the ground.** The SDL GPU renderer only draws triangles.
    `mu::GetRenderer().RenderLines` turns each segment into a 1-unit ribbon widened along a world
    axis: edge-on (invisible) from straight above and about a pixel wide at the game's angle; it
    stays that way for the existing debug draws. For editor lines use
    `RenderScreenLines(vertices, widthPixels)` (camera-facing, a width in pixels, untextured).
    `glLineWidth`, `GL_LINE_STRIP` and `GL_LINE_LOOP` work through the shim now, but
    `glPushAttrib`/`glPopAttrib` still do nothing, and raw `glEnable`/`glDisable` calls bypass the
    `ZzzOpenglUtil` state caches (a later `EnableAlphaTest()` then skips switching the texture
    back on, and objects draw untextured). Set overlay state with `TerrainOverlayState`, which
    goes through the wrappers and puts the caller's state back, and draw with the renderer
    (`RenderQuad3D(..., 0)` with the texture off draws in the vertex colours). One `RenderQuad3D`
    call draws at most 4096 quads on the SDL GPU renderer and cuts off the rest (with a
    `clamping draw` warning in `MuError.log` every frame); hand a larger batch to
    `Render::Topology::ForEachQuadBatch`, as the attribute overlay does.

---

## 6. Adding a new tool/tab (recipe)

1. Add a `RenderXxxTab()` to `CMapEditorUI` and a `BeginTabItem("Xxx")` call.
2. In that tab, set `m_desiredEditFlag = EDIT_XXX` when your tool is enabled — the panel
   applies it after the tab bar (and releases to `EDIT_NONE` otherwise).
3. Do the actual mutation from the captured mouse state (`m_PaintLDown/RDown`) when
   `m_groundHitValid` is set and the cursor isn't over the UI, using `SelectXF/SelectYF`
   (tile) or `m_groundHit` (the ground point under the cursor, copied at the start of the
   frame). Don't read `CollisionPosition` after an object pick: the pick overwrites it with
   the hit on the object.
4. A round brush takes the cursor from `BrushInput()`, its sliders and keys from
   `Editor::BrushControls` and its maths from `MuEditor/Editing` (`TerrainBrush`, `FieldBrush`,
   `SurfaceBrush`); `BrushControls::ShowOutline` draws its circle on the ground. After changing
   heights or the light map, rebuild the lighting of the changed rectangle
   (`CMapTerrainLayers::RelightHeights` / `RelightCells`).
5. Make each stroke one undo step: a terrain brush calls `TerrainStroke::Begin` with its
   layers (`MapTerrainLayers`) when the stroke starts and hands `Finish()` to
   `g_MapEditHistory.Push` when it ends (the stroke finds the changed rectangle itself); an
   object tool pushes an `ObjectEditCommand` (`TransformChanges`, `CreationChanges`,
   `RemovalChanges`) over `g_MapEditHistory.Objects()`. The shared Undo/Redo bar and keys need
   nothing else. Add a Save that writes the correct (re-encrypted) file, then passes it to
   `Editor::Files::MirrorSavedFile` and shows `DescribeSavedFiles` in the tab's status line
   (`Editor::StatusLine::Render`).

The **Attribute** tab is the cleanest end-to-end example of this recipe (edit mode +
overlay + undo + two save targets).

**Prior art:** `_upscale/terrain_editor.html` and `_upscale/terrain_tool.py` are the
standalone (out-of-game) versions of the attribute editor — same crypto, same byte
layout, same colours. Useful for bulk/scripted edits and for cross-checking the
in-game tool.
