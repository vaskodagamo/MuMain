# World editor plan

Goal (owner, 2026-09-22): open a map on the Mac, see it rendered exactly as the game renders it
with every object, move/rotate/scale objects, change the terrain, see what each asset is and
whether it was regenerated, and hand assets that need more work to the art builder (Codex agents
following [`ASTRA.md`](../../ASTRA.md)) with one click instead of a chat message.

Branch: `feat/world-editor`. Usage docs for the finished tool go into
[`src/MuEditor/UI/MapEditor/MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md).

## 1. Decision: extend the in-client Map Editor

The client already ships an ImGui Map Editor (`ENABLE_EDITOR=ON`): texture painting, height
sculpting, walkability painting, object placement/move/delete, an object browser with
thumbnails and a minimap generator, all saving the game's encrypted formats. It is the only
option that shows the real look (per-type blend meshes, fire/water effects, terrain lightmap,
roof alpha). A standalone web viewer would reach roughly 80-90 % fidelity on objects and would
duplicate every cipher and loader; Blender has no natural representation for terrain layers or
walkability. The web viewer stays a later option for headless Codex screenshots (section 5).

## 2. What blocked it on macOS (investigation 2026-09-22)

- Compile: only three files, all for the Win32 open-file dialog (`MapTextureImport.cpp`,
  `MapMinimapCapture.cpp`, `MapAttributeSave.cpp`). Everything else compiles and links.
- Runtime: backslash paths handed to `std::filesystem` (object browser empty, texture import
  always overwrites `ExtTile01`, junk `Data\...` folders); `--editor` ignored because
  `GetCommandLineW()` is a stub off Windows; offscreen thumbnail target format differs from the
  pipeline format on Metal.
- Workflow: the world only loads after a server login and map join; FreeFly culling follows the
  hero, so flying away hides objects; saves land in the build's `Main.app` and are overwritten
  by the next asset copy.
- Correctness: undo/delete/move of Lorencia objects that own an `Operates[]` entry leaves
  dangling pointers (110 in Lorencia, limit 200); `TerrainNormal` accumulates on every recompute,
  so lighting drifts after sculpting; `SaveObjects` can write a wrong count and crash on
  `fopen` failure.
- Out-of-date docs: thumbnails, the selection outline and the minimap are already portable.

## 3. Milestones

Each milestone ends with a build of `out/build/macos-arm64-mueditor` (preset `macos-arm64-mueditor`) and a
visual check on Metal.

| # | Milestone | Acceptance |
|---|-----------|------------|
| M1 | **Runs on the Mac**: SDL3 async file picker shared by the three pickers; `std::filesystem` paths built with `operator/`; `--editor` read from the portable command line; `macos-arm64-mueditor` preset; offscreen capture uses the swapchain format; stale MAP_EDITOR.md claims fixed. | Editor build links; client starts with `--editor`; thumbnails render. |
| M2 | **Offline world**: `Main --editor --world N` opens map N in the main scene with the FreeFly camera, no server, no login; culling follows the FreeFly camera while the editor owns it. | `MU_CAPTURE_FRAME` screenshot of Lorencia with terrain and objects, no server running. |
| M3 | **Safe, correct editing**: saves go to the runtime `Data` **and** the repo's `src/bin/Data` (auto-detected, overridable) with a timestamped backup; `Operates[]` cleanup on delete/undo; `TerrainNormal` reset; `SaveObjects` hardening; hotkeys ignored while typing (Backspace deletes on Mac); object drag follows the ground. | Save/undo/reload round trip in Lorencia; saved files appear in `git status`. |
| M4 | **Asset Review + regeneration requests**: `assets-work/World1/catalog.json` generated from the coordination files; an *Assets* tab listing every model with identity, status, batch, textures, placement count; click an object to see the same; highlight/step through all instances; **Flag for regeneration** writes a request folder with a screenshot. | Request folder written for a Lorencia model; Codex contract documented. |
| M5 | **A/B compare**: reload a model and its textures from disk at runtime; switch a model (or all) between *current* and *original* (materialized from git). | Toggle Tree01 original/current in the running client. |
| M6 | **Editing comfort**: object outliner with search; multi-select; transform gizmo; duplicate; snap to ground; multi-level undo/redo. | Owner moves/rotates a group of objects and undoes it. |
| M7 | **Terrain comfort**: smooth brush, circular falloff, partial normal/light updates, a Light tab that paints and saves `TerrainLight.OZJ`. | Sculpt + light paint + reload looks identical. |
| M8 | **Eyes for an AI agent**: control socket on the Mac editor build; the loop keeps answering while the window is hidden; `map-open`, `map-info`, `map-camera`, `map-export`, `map-query`, clean/region PNG screenshots; terrain loader bounds fixes. | Driven from Python: clean shot, top-down shot and layer PNGs of Lorencia read correctly. |
| M9 | **Hands**: high-level, undoable map edits over the socket (edit scripts `mu-map-edit/1`: terrain, textures, walkability, light, objects incl. seeded scatter), dry runs, undo/redo/history, saved like the panel's, revert from disk. | A script sculpts a hill, paints a road and scatters 30 trees around it on Lorencia; dry run first; before/after shots; undo matches before; redo; revert. |
| M10 | **New maps and the agent kit**: (A) maps 82+ (`Data/World83`...) created from a template or flat and joined by gates (`Gate.bmd`), with an OpenMU export; (B) `tools/world_editor/mapctl.py`, a command line and module over the socket, and `AI_MAP_EDITING.md` as the single entry point an agent follows on the owner's prompt (template, rules, loop, MU design rules, hand-back, server side). | Part A done: a flat map 82 made over the socket opens offline; a Lorencia <-> 82 gate pair appears in `gate-list` and the Gates tab; `Gate.bmd` round-trips byte for byte. Part B done: every `mapctl` command run against a launched editor client; unit tests without a client in ctest. |

Status:

- **M1 done (2026-09-22).** The editor builds with the `macos-arm64-mueditor` preset and
  starts with `./Main --editor`; one SDL3 file dialog replaces the three Win32 pickers,
  editor paths are built with `std::filesystem`, and thumbnails render in the swapchain
  format. Metal API validation (`MTL_DEBUG_LAYER=1`) no longer aborts the editor at its
  first ImGui draw: ImGui still draws inside the engine's main pass, now with a copy of its
  pipeline that the renderer builds for that pass's depth buffer, with depth test and write
  off. Checked with the Release client on `--world 1` and the login scene and with the Debug
  client on `--world 1`. A Debug build alone does not switch Metal's validation on; set the
  variable.
- **M2 done (2026-09-22).** `./Main --editor --world N` opens `Data/World{N}` offline with the
  FreeFly camera over the middle of the map's objects and the Map Editor open; a missing or
  incomplete folder logs one line and falls back to the login. FreeFly culls with its own
  frustum while the Map Editor is open or a map is open offline. Checked on Lorencia (frames
  300 and 1200, 70 s run, a 95 s scripted fly-around), Devias, World31, a sweep of every
  world folder and the fallback cases; usage in `MAP_EDITOR.md`. Review fixes: ImGui no
  longer inherits the last world draw's depth state (it drew as empty panels on Devil Square
  and other maps); the start view targets the objects' median, so event maps open on their
  content. One player-build change: `OpenObjectsEnc` skips object records with an impossible
  model type (Arena's `EncTerrain7.obj` has one, type -515, which crashed the client when its
  block was drawn). Not exercised offline yet: painting, object edits and saves (M3).
- **M3 done (2026-09-23).** Every Map Editor save (texture mapping, client `.att`, objects,
  height, minimap, T. Browse and O. Browse imports) writes the game's `Data` and copies the
  file into `<repo>/src/bin/Data`; the repo file it replaces goes to
  `out/editor-backups/<time>/` first. The repo is `MU_EDITOR_REPO_ROOT` or the first folder
  above the game with `src/bin/Data` and `.git`; without one the old copy next to the
  executable is kept and the panel says so. The server `.att` export is also copied to
  `out/editor-exports/`. Status lines list the absolute paths. Objects: drag follows the
  ground point under the cursor and keeps the height above ground, **Drop to ground**,
  Backspace deletes on a Mac, Cmd/Ctrl+Z on every editing tab, and no hotkey fires while
  typing. Checked offline on Lorencia with a temporary hook (since removed): 110 operates and
  no dangling owner after moves across blocks, three undos and delete/undo of an operate
  owner, 70 s with the game's operate selection running every frame; injected Backspace and
  Cmd+Z; an injected drag; sculpt + undo restores the normal, light and height arrays
  bit for bit. Three player-build changes, all bug fixes:
  - Deleting world objects (map change, editor edits) first drops their `Operates[]`
    entries and the effects, joints and particles attached to them (no more dangling
    pointers; the editor's undo no longer overflows `Operates[]`).
  - `CreateTerrainNormal` rebuilds each normal from zero. The first map load is bit for bit
    unchanged (checked against the old code on Lorencia), but every later load used to add
    to the previous maps' normals; game maps entered after the login and character screens
    therefore get slightly stronger slope shading than before, the same as a first load.
  - `SaveObjects` checks `fopen`/`fwrite`, writes the header count of the records it wrote,
    and leaves out objects a map spawns at run time. `OpenObjectsEnc` shares the new
    `WorldObjectFile` decoder and loads only complete records of a cut-short file (before,
    it read past the buffer).
- **M4 done (2026-09-23).** The Map Editor has an **Assets** tab over
  `assets-work/World{N}/catalog.json` (World1 only so far; other maps say there is no catalog):
  a sortable, filterable table with the live placement count per model, details (identity,
  BMD, textures and the models sharing them, engine rules, batches newest first, offline
  previews, open requests), **Highlight all** (magenta outline on every instance), **<**/**>**
  (select an instance and point the free-fly camera at it), and **Looks good in client** /
  **Needs work**, written to the owner's `assets-work/World1/client-review.json`, which
  `build_editor_catalog.py` now folds into `client_verified` and `client_review`. The Objects tab
  shows the selected object's model, status and verdict. **Flag for regeneration...** writes
  `requests/<date>-<model>-<slug>/` exactly per `requests/README.md` (base commit read from
  `.git`, BMD SHA-256 of the checkout, shared-texture partners as optional extra targets, the
  picked instance with its record index in `EncTerrain1.obj` (left out while that file has
  uncommitted changes), a 1920 px JPEG of the view taken
  in a frame without the editor overlay, cursor or camera text) and runs no other tool; usage in
  `MAP_EDITOR.md`. Checked offline on Lorencia with a temporary hook (since removed): Tree01
  selected, highlighted, stepped to an instance, a request with Tree02 as partner and a capture
  written, `validate_request.py` OK, the verdict folded into the catalog; test data removed. The
  data code is unit-tested (`editor_asset_request_tests`). No player-build change: the new lines
  outside `src/MuEditor` are editor-only. Known limit: the outline is drawn on the model's
  surface, so leaves hide it on trees.
- **M5 done (2026-09-23).** `tools/world_editor/materialize_variant.py original` builds
  `out/ab/original/Data/Object1/` from git (each catalog model at its `original.revision`,
  SHA-256 checked, plus every texture that original model names, cross-checked against the
  texture baseline; deterministic, with a `manifest.json`). The Assets tab switches one model or
  all models between current (`src/bin/Data`) and original while the client runs, shows what each
  model shows (`as built` until it is switched, `current`, `original`, `mixed` when a shared
  texture follows another model), offers
  **Reload from disk**, and prints the command when the original files are missing or older than
  the catalog. The reload (`Editor::Assets::HotReload`) runs between frames, checks the file first
  (BMD version, mesh/vertex/bone limits, indices, texture files and sizes; refusals are messages,
  never the loaders' fatal exit), re-opens the model with `BMD::Open2`, and reads each texture into
  the bitmap index the model already used (`CGlobalBitmap::ReloadImage`, editor-only), so shared
  textures keep their references; object thumbnails of the type are dropped. Checked offline on
  Lorencia with a temporary hook (since removed): all 112 typed models to original and back, the
  town and a tree view visibly differ (21 % of pixels vs 3-5 % between two unchanged frames),
  texture count and memory return exactly to the start values after the round trip and after a
  map reload with every model original, broken and texture-less files refused, 60+ s alive, also
  under Metal API validation. No player-build behaviour change (one shared GPU-release helper in
  `GlobalBitmap.cpp`, same code path).
- **Review of M1-M5 done (2026-09-23).** Fixed: a map unload (another map, the same map again, or
  while the panel was closed, also with the Target world override set) now drops the Map
  Editor's selection and every undo step, keyed on the editor-only `ObjectListGeneration()` that
  `DeleteAllObjects` bumps (before, a stale `OBJECT*` could be read, moved or freed twice, and an
  undo could write the old map's data into the new one). Request ids keep their `-2`/`-3` within
  80 characters; the dialog warns for the BMD of every target, trims text as the validator does
  (no-break spaces), checks that `HEAD` is on `origin` (not upstream) and gives `obj_index` only
  when `EncTerrain1.obj` equals git's index. The Shows column says `as built` for what the map
  load read from the build's `Data` copy. The materialize command runs from any folder
  (`py -3` on Windows). Paths in messages and `MU_EDITOR_REPO_ROOT` are UTF-8 on Windows; shared
  text helpers in `Editor::Text`. `__pycache__/` is ignored. Checked offline on Lorencia with a
  temporary hook (since removed) under Metal API validation. Player build: no behaviour change.
- **M6 done (2026-09-23).** Editing comfort, usage in `MAP_EDITOR.md` (Objects, Transform gizmo,
  Outliner, Undo and redo):
  - **Undo/redo:** one multi-level history for the Texture, Objects, Height and Attribute tabs
    (Cmd+Z, Cmd+Shift+Z, Ctrl+Y on Windows and Linux; buttons named after the next step), replacing
    the per-tab one-level snapshots. A terrain stroke keeps only the rectangle it changed (before and
    after bytes of each layer it painted); an object step keeps each object's state before and after,
    named by its record index. Undo never rebuilds all objects (the old `RestoreAll` path is gone):
    it moves, deletes or re-creates only what the step changed, through the owner-safe delete.
    64 MB limit, oldest steps dropped; cleared on every map unload; unavailable (buttons greyed out,
    keys ignored) while a stroke or drag is held. The logic (`MuEditor/Editing/`) has no ImGui or engine code and is unit-tested.
  - **Stable save order:** each loaded object keeps its record index (`OBJECT::SaveOrder`, editor
    build only; the pointers are stable, and an object re-created in another block or by an undo
    gets its index back), and `SaveObjects` writes the records in that order, added objects after
    them. A save without edits writes `EncTerrain1.obj` byte for byte (unit test on the shipped
    file and every other shipped `.obj`, and at run time: no backup made). Player build: same
    grid-order save as before (all orders unset).
  - **Selection set:** click selects one, Shift/Cmd+click adds or removes, Esc clears; the last one
    picked is the primary (yellow outline, the others orange); kept across tabs, dropped on a map
    unload. Delete, Duplicate (Cmd+D, copies one tile east, selected), Drop to ground, the ground
    drag and the Pos/Angle/Scale fields (the primary's values, applied as a change to all) work on
    the whole selection.
  - **Outliner** window: every live object with catalog name, type, tile position, height, angle
    and scale; name and type filters; list clipper; click selects and points the free-fly camera at
    it; Shift/Cmd+click; "Select all of type".
  - **Transform gizmo** drawn in ImGui's background draw list with the camera the world was drawn
    with (projection checked against the engine's `CameraProjection::WorldToScreen` to within a
    pixel): move (three arrows and a ground-plane square), rotate (three rings; a turn around X or
    Y is composed with `AngleMatrix` and turned back into `Angle[]`, unit-tested against
    `AngleMatrix`), uniform scale; W/E/R with the cursor over the 3D view (in a game session they,
    and Esc, are kept from the game's potion hotkeys and menu); snap 25 units, 15 degrees, 0.1; a
    group turns and scales around its centre; one undo step per drag; the handles take the click.
  - Checked offline on Lorencia under `MTL_DEBUG_LAYER=1` with a temporary hook (since removed):
    three cannons selected and turned 45 degrees through the gizmo API and by an injected drag on the
    Z ring (48 degrees, snapped to 45), undo/redo bit for bit, Cmd+D, Backspace, Cmd+Z, W/E/R, Esc,
    a pose box's operate entry dropped and restored by delete/undo/redo, three terrain strokes
    undone and redone bit for bit (mapping, height, normals, light, walls), two objects saves
    byte-identical to git `HEAD`, a map reload clearing history and selection; a cannon moved across
    an object-grid block was re-created with its record index, the save changed only its record,
    and after the undo the save equalled `HEAD` again. Screenshots of each gizmo mode. Found, not changed: with the free-fly camera closer than about 1000 units, objects
    near the bottom of the view are culled (documented).
- **M7 done (2026-09-23).** Terrain comfort, usage in `MAP_EDITOR.md` (Round brushes, Texture,
  Height, Attribute, Light, gotcha 16):
  - **Lines that editor overlays can use:** `IMuRenderer::RenderScreenLines(vertices, widthPixels)`
    draws camera-facing ribbons of a width in pixels, untextured and never culled, built in view
    space (`Render::Lines`, unit-tested: the width in pixels at both ends of a line running away
    from the camera, a ground line seen from straight above, cuts at the camera, degenerate
    input). The GL shim breaks `GL_LINE_STRIP`/`GL_LINE_LOOP` into segments (they were dropped)
    and honours `glLineWidth`. The attribute overlay and the Texture tab's highlight set their
    state through the `ZzzOpenglUtil` wrappers (`TerrainOverlayState`, restores the caller's) and
    draw with the renderer instead of the no-op `glPushAttrib` and raw `glBegin`; the tile grid,
    debug spheres/boxes and the FreeFly frustum use the new lines. Player build: no visible change.
    `RenderLines` is unchanged, and its callers outside the editor (the `_DEBUG` terrain wire in
    `RenderTerrainTile`, `CSK_DEBUG_RENDER_BOUNDINGBOX`, and the uncalled `RenderWayPoint` and
    `BMD::RenderBone`) keep it; the switched callers are reached only from `_EDITOR` code; no code
    outside the editor uses `glBegin` line modes or `glLineWidth`.
  - **Round brushes:** soft (full to half the radius, then a smoothstep to the rim) for height,
    the overlay texture's opacity and light, hard for walkability; all clip at the map's edges
    instead of wrapping; outline on the ground (rim plus the fade's start); Radius/Strength sliders
    with [ ] and Shift+[ ]. Height tools: raise/lower, flatten (to the height where the stroke
    began), smooth (the legacy 5-point kernel on the values before the frame), set height
    (Alt-click samples). The layer-1 texture brush stays a square, now clipped too.
  - **Partial relighting:** a stroke frame rebuilds normals and light only for its rectangle plus
    one cell (new `CreateTerrainNormal_Rect`/`CreateTerrainLight_Rect`, wrapping as the normals
    do); unit tests compare them with a whole-map rebuild bit for bit (interior, cliff, plateau,
    both map corners) and `CreateTerrainLight` with its old loop. Undo/redo use them too.
  - **Objects follow terrain** (default on): objects on ground a height stroke moves keep their
    height above it; their moves join the stroke's undo step (`EditCommandGroup`), and an undo
    does not change the selection.
  - **Light tab (`EDIT_LIGHT`):** add, subtract, tint and smooth the light map with colour and
    strength, one undo step per stroke; **Save light** writes `TerrainLight.OZJ` (24-byte prefix,
    256 x 256 JPEG, bottom row first, quality 100, 4:4:4) with the repository copy and backup, and
    lights the ground from the file it wrote (`Editor::LightMap`, unit-tested including the
    shipped Lorencia file).
  - Checked offline on Lorencia under `MTL_DEBUG_LAYER=1` with a temporary hook (since removed)
    and injected mouse and key events: a raise stroke next to Cannon02 (one step "Raise ground",
    12 objects moved and all 2870 kept their height above the ground, partial lighting equal to a
    whole-map rebuild, undo/redo bit for bit, selection kept), smooth (roughness 4697 to 1424),
    [ and Shift+], Alt-click sampling and set height, flatten, add/tint/smooth light, save, map
    reload (light map, lit colours, normals and heights bit for bit; screenshots differ only in
    the water, less than two frames before the reload differ from each other). Screenshots of the
    outlines from 45 degrees and straight down. Data restored, `out/editor-backups` removed.
- **Review of M6-M7 done (2026-09-23).** Fixed:
  - A click that closed a popup (the Outliner's row menu or type list, the Light tab's colour
    picker, any combo) also reached the world under the window it landed on: it placed a tree,
    replaced the selection with a wall behind the Outliner, or painted light under the panel.
    While a popup is open, and until the button that closed it is released, the mouse now counts
    as over the editor (`Editor::Editing::PopupMouseGuard`, unit-tested). Esc closes open menus
    and lists (ImGui's keyboard navigation, which would, is off) and no longer also clears the
    selection.
  - `RenderQuad3D` draws at most 4096 quads per call, so the Dev Editor's tile grid (92524 quads in
    the offline Lorencia view) showed a corner of the view and the Attribute overlay tinted 18 % of
    it, with a warning in `MuError.log` every frame. Both now submit in runs of 4096
    (`Render::Topology::ForEachQuadBatch`, unit-tested); the renderer's clamp itself is unchanged.
  - A selection made in the Outliner or with the Assets tab's arrows switches the Objects tab to
    Select & edit (it stayed in Place new, so the next click on the object placed a tree).
  - `OBJECT::SaveOrder` exists in every build, so `OBJECT` has one layout (a test target built with
    `_EDITOR` links the player's `MuClient`); only the editor sets it, player behaviour unchanged.
  - Cleanups: one shared live-object walk (`Editor::ObjectPlace::ForEachLiveObject`) instead of four
    copies of the object-grid loop; the gizmo's drag label moved into the tested
    `Editor::Editing::Transform::Describe`; `RenderAttributeTab` and `CMuEditorCore::Render` split;
    docs corrected (Strength is not on the Attribute tab, Backspace deletes only on a Mac, undo keys
    are ignored rather than queued while a stroke is held).
  - `MAP_EDITOR.md` starts with a quick start for the owner.
  - Checked offline on Lorencia under `MTL_DEBUG_LAYER=1`, driven by scripted SDL input from a
    driver library outside the repository (no hook in the tree): dismissing the row menu over the
    Outliner (Place new and Select & edit), the colour picker over the panel and with a long press
    on the 3D view changed nothing, Esc closed the menu and kept three selected cannons, the
    Outliner switched the Objects tab to Select & edit, a later click still painted; the tile grid
    and the Attribute overlay cover the whole view, also top-down over the whole map, with no
    `clamping draw` warning. `ctest` 335/335.

- **M8 done (2026-09-23).** An AI agent can see a map. Usage in `docs/control-socket.md`
  ("Editor commands") and `MAP_EDITOR.md` ("Letting a script or an AI agent look at the map").
  - The `macos-arm64-mueditor` preset turns the control socket on; it builds and works on macOS
    (accepted sockets get `SO_NOSIGPIPE` and a 256 KiB send buffer instead of macOS's 8 KiB; the
    socket unit tests write chunks into that 8 KiB buffer from the reading thread and needed a
    wider client buffer on macOS too). Paths up to 103 bytes.
  - Hidden window: on this Mac (macOS 15.6, SDL 3.4.8, Metal) the loop never stalled, minimized,
    hidden (Cmd+H) or covered for minutes (`ping` 1-20 ms, screenshots correct); `nextDrawable`
    kept vending drawables. Still, while the socket serves, a frame whose window SDL reports as
    minimized, hidden or occluded is now drawn into an offscreen target of the window's size
    (`Render::HiddenWindow`, socket builds only), so no swapchain wait can hold the loop on other
    platforms and captures keep working; verified on macOS through the log (offscreen on/off at each
    change) and clean, region and top-down captures taken while minimized, hidden and covered.
  - Commands (thin handlers in `App/Control/ControlCommandsMap*.cpp` over `Editor::LiveMap`,
    `Editor::Camera` and the unit-tested `MuEditor/MapInspect/`): `map-open` (runtime switch via
    `Editor::OfflineWorld::Open`, selection and undo dropped through `ForgetUnloadedMap`),
    `map-info` (identity, objects, catalog, gates from the loaded `Gate.bmd`, unsaved flags per file
    from content digests taken at load and after each save), `map-camera` (tile framing or top-down
    rectangle), `map-export` (PNG per layer, `legend.json`, `objects.json` in save order),
    `map-query`, and `screenshot` with `clean`/`region`/PNG through `Editor::ViewCapture` (the clean
    frame also leaves the game HUD out). `Request::GetStructured` hands array/object arguments over.
  - Player-build fixes (memory safety): `OpenTerrainMapping` and `OpenTerrainHeightNew` refuse a
    file cut short (logged) instead of reading past it; `OpenJpegBuffer` refuses a light map that is
    not 256 x 256 instead of writing past `TerrainLight`; the tiles of row 255 read their top
    corners from row 255 instead of past the terrain arrays (other corners, including column 255's
    wrap to the next row, unchanged). Stock maps load byte-identical (all 49 light maps are
    256 x 256, every `.map` 196610 bytes, every extended `.OZB` 196666).
  - Checked with the Release editor client on World1 and World3 driven from Python: every command
    and its argument errors, clean and overlay shots, a top-down shot of all of Lorencia cropped to
    the map, the seven layer PNGs (the attribute image shows the town's safe zone and walls, the
    height image the moat and the river, north up as in the top-down shot), map switches
    World1 -> World3 -> World1. `ctest` player 358/358 (19 new), editor build 386/386. No game data
    changed.

- **M9 done (2026-09-23).** An AI agent can edit a map. Usage in
  [`AI_MAP_EDITING.md`](AI_MAP_EDITING.md) (the agent's reference: the loop, the script format,
  limits), `docs/control-socket.md` ("Editor commands") and `MAP_EDITOR.md`.
  - `map-apply` runs an edit script (`"schema": "mu-map-edit/1"`, up to 256 ops): `terrain.raise`,
    `.lower`, `.flatten`, `.set`, `.ramp`, `.smooth`, `.noise`; `texture.paint` (layer 1 or 2),
    `.erase`; `attribute.set`; `light.add`, `.subtract`, `.tint`, `.set`, `.smooth`; `object.place`,
    `.scatter`, `.move`, `.rotate`, `.scale`, `.delete`, `.drop_to_ground`; over circles,
    rectangles, polygons and paths with a soft or hard edge (soft ops default to the round brushes'
    edge). Every field is checked (unknown keys too) and model/texture names resolved on the loaded
    map before anything changes; errors name the field. The script runs on a copy of the map and its
    changes become one undo step (a `TerrainStroke` over the changed layers plus an
    `ObjectEditCommand`); `dry_run` reports cells and bounding rectangles per layer, objects added,
    removed and changed, and warnings. Scatter is seeded Poisson-disk dart throwing with weighted
    models, scale/yaw ranges and avoid rules (walkability, textures, slope, other objects, shapes
    such as a road), and can mark the new objects' tiles (e.g. blocked trunks). Walkability takes
    clean values only and refuses the anti-tamper tiles.
  - `map-undo`, `map-redo`, `map-history` over the Map Editor's shared history (the panel shows
    "Undo: <label>"; the selection is kept by key), `map-save` (default: the files with unsaved
    edits; the tabs' own saves with repository copy and backup; answers the paths), `map-revert`
    (reads files back as the loader does, checked first; clears the history). All answer `busy`
    while a stroke or drag is held.
  - Pure units in `src/MuEditor/MapScript/` (25 test cases, `editor_map_script_tests`), the brushes
    of `Editing/` generalised to masks of any shape (`WeightMask`; the round brushes unchanged),
    adapters `Editor::LiveMapEdit` and `Editor::LiveMapFiles`, handlers in
    `App/Control/ControlCommandsMapEdit.cpp`. The height save moved out of the panel into
    `Editor::HeightSave` and now refuses 24-bit height maps instead of writing a file they cannot
    load.
  - Checked with the Release editor client on World1 driven from Python: the grove script (hill,
    smooth, gravel road on layer 2, 30 seeded trees avoiding the road and blocking their trunk
    tiles, warm light on the hill) dry-run and applied in about 30 ms; before/after clean shots from
    one camera pose; undo returns every unsaved flag to false and the shot to the before shot within
    frame-to-frame noise (1.59 % vs 1.57 % of pixels between two unchanged frames); redo matches the
    after shot; `map-revert all` restores 2870 objects and the area's exact statistics; `map-save`
    wrote all five files with backups and a reload showed the edit (then restored from git); every
    error path (sentinel tile, typo, schema, unknown model and texture, ids, empty selector, heights
    above 382.5). Player build: no scripting code in the binary (`strings`), no behaviour change.

- **M10 part A done (2026-09-23).** The world can grow: new maps and the gates between them.
  Usage in `MAP_EDITOR.md` ("New maps", "Gates", "OpenMU export") and, for agents,
  [`AI_MAP_EDITING.md`](AI_MAP_EDITING.md) ("Growing the world").
  - **New maps** take numbers 82 to 254 (folder = number + 1: map 82 is `Data/World83` and
    `Data/Object83`; the `EncTerrain` headers store the folder number in one byte, so folder 255
    and map 254 are the last). The **New map...** window and `map-new` copy a template (its three
    `EncTerrain` files renumbered, height, light, textures, optional minimap, models) or make flat
    ground (height, tile slot, clean walkability value, light, the tile set of another map), write
    the game's `Data` and `src/bin/Data`, and refuse existing folders. Lorencia's named models
    (`Tree01.bmd` ...) are copied as `Object{type + 1}.bmd`: its types are below 160, so the copied
    `.obj` keeps its types unchanged. Pure units `MuEditor/NewMap/`, adapter `Editor::NewMapFiles`.
  - **Client, editor builds (player build: owner's decision pending):** `GetMapName` reads the
    name of a map numbered 82 or higher from `Data/World{N}/MapName.txt` (`World::MapNames`, cached
    per map) before its old fallback. The fixer put the call behind `_EDITOR` because the player
    build may change only for the named memory-safety fixes; to name new maps in the player's
    client too, the owner removes that `#ifdef` in `CMapManager::GetMapName`. Map numbers 0 to 81
    are unchanged either way.
    **Editor:** `--world` and `map-open` accept any existing folder 1 to 255.
  - **Gates:** `MuEditor/Gates/` reads and writes `Gate.bmd` byte for byte (512 records of 14
    bytes, each `BuxConvert`ed; unit test on the shipped file), adds one-way pairs in the lowest free
    numbers from **345** (344 is a Karutan 2 spawn record in the client and OpenMU's seed, so the
    investigation's "344 to 511 are free" was one off), removes and moves only those, and warns
    about blocked tiles, tiles OpenMU blocks and overlapping gates. The **Gates** tab lists the
    map's gates and ways in, draws them on the ground over the walkability overlay
    (`Render::Terrain::GroundRects`), draws new areas with the mouse and adds, moves and removes
    gates; socket `gate-list`, `gate-add`, `gate-remove`, `gate-show`. Every change saves
    `Gate.bmd` at once with a repository copy and backup (not part of the undo history).
  - **OpenMU export** (Gates tab button, `map-server-export`): `out/openmu-export/map{N}/` with
    the new map's walk map in OpenMU's layout, `map.json`, `gates.json` (OpenMU's map-export shape,
    grouped by map), `HOWTO.md` (Admin Panel steps, from its code) and `openmu.sql` (one
    transaction, marked not applied, never run). The game's own maps get gates only.
  - Checked with the Release editor client over the socket: flat map 82 created (35 files) and
    opened (flat grass at 150, all walkable, named in `map-info`); booted straight into it with
    `--world 83`; a Lorencia template copy as map 83 (305 files, 2870 objects on the renamed models;
    Lorencia-only effects missing and some blended meshes black, as documented); gates 345/346
    (Lorencia [245,92,246,97] -> 82) and 347/348 (82 -> Lorencia [240,93,242,96]) added, `Gate.bmd`
    on disk changed in exactly those four records; `gate-list`, `map-info` and the Gates tab (via
    `gate-show`) show them; export files read back. Test maps removed and `Gate.bmd` restored
    afterwards. Walking through a gate needs the server and was not tried.

- **M10 part B done (2026-09-23).** Agents have a kit and one page to follow.
  - [`AI_MAP_EDITING.md`](AI_MAP_EDITING.md) is the entry point (pointed to from `AGENTS.md` and
    `HANDOFF.md`): the owner's prompt template and the defaults an agent takes, ground rules (what
    needs the owner's OK: saves, new maps and gates, commits, pushes and PRs on the fork only; never
    the server), setup, the loop (orient, observe with export and before shots from fixed poses,
    plan one script per layer and sketch it, dry run, apply terrain -> textures -> walkability ->
    objects -> light, look from the same poses and check with numbers, fix, hand back), design rules
    for MU maps (walkability under objects, walkable roads, safe zones, gates kept free, client and
    server walk maps, anti-tamper tiles, densities measured on Lorencia, cliffs, only existing
    models with the art-request route, borders), growing the world (why maps cannot grow, template
    or flat, gate placement), hand-back (open for review, or branch + PR with a record folder and
    before/after pictures, after the owner's OK) and the server side (export and HOWTO; the owner
    applies it). The script and command reference and the gotchas stay in the same page.
  - `tools/world_editor/mapctl.py` (standard library only): `launch --world N` (finds the editor
    build, starts it with `MU_CONTROL_SOCKET`, waits for the world scene, prints the PID; refuses a
    socket that is served, a path over the system's limit), `ping`, `info`, `open` (refused while
    the loaded map has unsaved edits, which `map-open` would drop silently; `--discard`), `camera`,
    `shot` (clean PNG; `--topdown` frames and crops a rectangle), `export`, `query`, `apply`,
    `dry-run`, `undo`, `redo`, `history`, `save`, `revert`, `new-map`, `gates`, `gate-add`,
    `gate-remove`, `gate-show`, `server-export`, `quit` (waits until the client has gone), `send`,
    and `sketch` (`map_sketch.py`: a labelled tile grid and a script's shapes drawn over an export
    layer or a top-down shot, with its own small PNG reader and writer). JSON out, exit codes 0/1/2/3,
    timeouts, request ids, scripts inline or by path above the 256 KiB line limit.
  - Tests: `tools/world_editor/tests/test_mapctl.py` (47 cases: framing, argument parsing for every
    command, script loading, launch helpers, a fake socket server for the client, `open`'s guard,
    `quit`, the PNG codec with every filter, tile-to-pixel mapping, sketches), registered as the
    ctest `world_editor_mapctl` in every build.
  - Checked against the Release editor client: every subcommand on Lorencia and on a new map 82
    (launch in 1.7 s, clean/overlay/top-down shots, export, query, a sketch of the plan over the
    top-down shot and over the export, dry run, apply of a knoll/path/grove script, undo, redo,
    save of one file, revert, new map, a gate pair, gate-show, server export, gate removal, open,
    send, quit; launch errors). A sketch of the gate rectangles lies exactly on the gate areas the
    engine draws. Test data removed afterwards; `Gate.bmd` byte-identical to before.
- **M10 acceptance (2026-09-23).** An agent built map 82 "Lorencia Outskirts" end to end with
  mapctl and the guide only (blank map, terrain, textures, walkability, 3,664 objects, a relief light
  bake, gates 345-348 to and from Lorencia's east edge, OpenMU export). Saved in `src/bin/Data`
  (`World83`, `Object83`, `gate.bmd`), not committed; the export is in `out/openmu-export/map82`.
- **M8-M10 review fixes (2026-09-23).** From three reviews and the acceptance run:
  - Crashes and safety: Korean (CP949) model names made the JSON encoder throw and ended the client
    in `map-query`, `map-export` and `map-apply` on most game maps; names are now escaped as `%XX`
    (`Editor::Text::ValidUtf8`), handler exceptions answer `failed`, and response lines replace bad
    UTF-8. A hidden window's frames are paced like a visible one's (two frames in flight, 60 per
    second; `Render::HiddenWindow::SubmitPaced`): before, the unpaced loop grew the GPU driver's
    buffers to tens of GB. `openmu.sql` writes the map name as hex bytes and stops at the first
    error; `map.json` and `gates.json` carry a format OpenMU's spawn import refuses; exports hold
    only the gates the editor added (stock records differ from OpenMU's seed). Gate edits read
    `Gate.bmd` from disk first (a second client no longer erases the first one's gates) and use the
    file's on-disk spelling (`gate.bmd`, also for the repository copy and the tests on Linux).
    `gate-add` refuses arrivals nobody could leave and endless bounces unless `allow_trap`. Scripts
    that would hold the main loop for more than about 1.5 s are refused (`MapScript/ScriptCost`).
    MapScript units are built without FP contraction, so a seed gives the same map everywhere.
  - Agent comfort: `light.bake` (relief shading), `attribute.set` with `under` (the tiles of
    selected objects), `map-tab`, `map-info` with `models`, `ping`/`quit` report the `pid`, the
    client logs why it quits, captures retry skipped frames for 3 s, PNGs are deflated (a frame
    about 70 % of its raw size). mapctl: a per-user default socket folder and an owner check,
    `launch` refuses a busy client and a foreign pid, `quit` waits for the process, `shot` retries,
    JSON on argument errors, `info --models`, `gate-add --allow-trap`, `tab`. Guide: Lorencia's
    measured walkability convention per model, water and bridge recipe, light bake for flat maps,
    names on new maps, gate facing, `gate.bmd` in git.
  - Player build: only the named memory-safety fixes (M8). `CMapManager::GetMapName`'s
    `MapName.txt` lookup now sits behind `_EDITOR` until the owner decides the player client
    should show new maps' names (remove that `#ifdef`).
  - Not done: the source map's catalog names on maps made with `models_from` (types work), a
    `tiles` shape and a higher op limit, an in-client JPEG option, fog and sound for new maps, an art
    pass on map 82 (weak points: an empty central meadow, a road that ends in the rim, hard river
    edges). Windows and Linux were not built.

## 4. Regeneration request contract (M4)

One folder per request, so parallel requests never conflict:

```
assets-work/World1/requests/<YYYY-MM-DD>-<model>-<slug>/
  request.json   machine contract, schema "mu-regen-request/1"
  brief.md       generated human/Codex brief
  captures/*.jpg in-client screenshots (small JPEGs)
```

`request.json` carries: id, created, status (`open` -> `claimed` -> `delivered` -> `accepted` /
`rejected` / `withdrawn`), priority, kind (`repaint`, `remodel`, `repaint+remodel`,
`new-variant`), base commit, the target model(s) (type id, BMD path, textures, placement count,
picked instances, previous batch, original revision), the owner's notes (what to change, what to
keep), constraints copied from the catalog (shared textures and their consumers, engine
controls), captures with camera position, and a `handoff`/`result` block the worker fills.
Placement and terrain edits are **not** art requests: the owner makes them in the editor on
their own branch. The coordinator alone updates the ledger and board, as today.

## 5. Later options

- Web viewer (three.js) fed by a `bmdconv bmd2gltf` export, for headless Playwright captures
  by Codex.
- Client `--screenshot`/camera CLI on top of the offline world mode for true-client captures by
  agents.
- Data-driven Lorencia model table so new object types persist without code edits.
