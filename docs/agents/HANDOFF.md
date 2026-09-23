# Handoff for AI agents and new contributors

Start here if you are an AI assistant (Claude, Codex, Gemini, Cursor, ...) or a person picking
up this fork. It records what this fork is for, what already works, where things are, and what
is open. General coding rules are in [`docs/CODING_RULES.md`](../CODING_RULES.md); this file is
about *this fork's* goal and state. **Append a dated entry to
[`WORKLOG.md`](WORKLOG.md) at the end of every session** and update this page when the
state changes.

## 1. Goal of the fork

Build the owner's own MU Online client on top of MuMain (sven-n's Season 6 Episode 3 client
fork) with **new graphics**: first regenerate the existing items, monsters, characters, world
objects, terrain textures and UI at higher quality, then extend maps a little and add items.
Assets are produced by AI tooling that drives Blender; the server is
[OpenMU](https://github.com/MUnique/OpenMU). The regeneration plan and the rules the game
imposes are in [`ASSET_REGENERATION_PLAN.md`](ASSET_REGENERATION_PLAN.md); formats and tools
in [`../asset-pipeline.md`](../asset-pipeline.md). The artist agent's own brief is
[`../../ASTRA.md`](../../ASTRA.md).

## 2. What exists and works (as of 2026-09-22)

| Area | State |
|------|-------|
| Player client build on macOS (Apple Silicon) | Works: `cmake --preset macos-arm64 ...`, see [`../build/macos/console.md`](../build/macos/console.md). Tests 100 % green. |
| Client vs OpenMU | Verified: connects to the local Docker OpenMU on port 44406, receives the server list, reaches the login scene. |
| Client stability on macOS | Fixed 2026-09-22: crashes in Metal/CoreFoundation after seconds to minutes were heap corruption from miniaudio's failure paths, triggered by the missing `Data/Music` files (see WORKLOG). miniaudio is patched at configure time (`cmake/patches/`). |
| Editor build (`ENABLE_EDITOR=ON`) | Windows and macOS. On the Mac: preset `macos-arm64-mueditor`, run `./Main --editor [--world N]` (N opens `Data/World{N}` offline, no server). Usage in [`MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md). |
| `bmdconv` (model converter BMD <-> SMD, compare, validate) | Works, tested (`tests/tools`). |
| `tools/mu_texture.py` | Works, byte-identical round trips on shipped textures. |
| Blender scripts (`tools/blender/`) | Import and export through Blender Source Tools; verified round trip on `Monster01.bmd` (geometry, bone order, 7 actions equivalent). |
| Lorencia asset pilot | 17 ground textures, Beer01 and three more static props (Candle01, TreasureChest01, Tomb03) exported/validated offline on `art/world1-pilot`, now merged into `main`; client acceptance pending. See [`assets-work/World1/notes.md`](../../assets-work/World1/notes.md). |
| Lorencia static rebuild | All 106 in-scope static models accepted offline and on `main`: the rebuild batches arrived with PR #11 (merge commit `7b808473`, 2026-09-22); the four pilot props and four tavern files were already there. Client acceptance pending. Per-model facts: [`assets-work/World1/catalog.json`](../../assets-work/World1/catalog.json). Follow-up art: [regeneration requests](../../assets-work/World1/requests/README.md). |
| Game data in `src/bin/Data` | Complete except: no `Sound/`, no `Music/`, most of `Object74/` missing, a few effect/skill models missing. |
| UI art pilot revision (`art/ui-modern-pilot`) | Five right-HUD textures revised with clean dark metal, bold symbols and clearer selected states. User selected this restrained direction. Offline validation and source Data installation are isolated to the revision worktree. Native/1080p comparisons cover anchored and classic layouts: [`assets-work/UI/notes.md`](../../assets-work/UI/notes.md). Client verification pending; shared runtime untouched. |
| Remaining bottom HUD (`codex/ui-hud-completion`) | Extends merged PR #9's style across 14 connected frame, gauge, item/skill-slot, XP and cash-shop textures. Exact dimensions/UVs preserved; all exports, 10% resource visibility, shared backing alignment and reproducible assembly pass offline checks. Merged into `main` with PR #10 (merge commit `17a932a2`, 2026-09-22); the 14 textures are in `src/bin/Data/Interface/`. [Inventory, sources, previews and client checklist](../../assets-work/UI/HudCompletion/notes.md). Shared skill slots also affect MU Helper/pet information; client acceptance pending. |

## 3. Development machine (owner's Mac)

Facts an agent needs when working on that machine; adjust if the environment moves.

- Apple Silicon, macOS 15, Xcode Command Line Tools, Homebrew in `/opt/homebrew`. An **old Intel
  Homebrew also exists in `/usr/local`**; keep `/opt/homebrew/bin` first on `PATH` and pass
  `-DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"`.
- The Command Line Tools used to carry a **stale libc++ header folder** that broke every C++
  build with `'cassert' file not found`. The owner moved it away on 2026-09-22, so a plain
  `cmake --preset macos-arm64` works; if the error returns after a tools update, see the
  troubleshooting section of the macOS guide.
- .NET 10 SDK in `~/.dotnet` (exported in `~/.zshrc`; GUI-launched IDEs need it set separately).
- Build directory: `out/build/macos-arm64` (Ninja Multi-Config; Release built). Runtime:
  `out/build/macos-arm64/src/Release/Main.app/Contents/MacOS/` (run `./Main` from there;
  `config.ini` there points at `127.0.0.1:44406`). Client log: `MuError.log` in that folder.
- Tools: `out/build/macos-arm64/tools/bmdconv/Release/bmdconv`, Blender 5.2 at
  `/Applications/Blender.app` with Blender Source Tools 3.4.3 enabled.
- OpenMU server: shallow clone in `../OpenMU`; start/stop from `OpenMU/deploy/all-in-one` with
  `docker compose -f docker-compose.mumain-local.yml up -d` / `down`. Admin panel
  `http://127.0.0.1:8090/` (create the first admin user; until then it has no login). Ports 80,
  8081 and 3307 belong to other tools on this machine; do not use them. Server log:
  `docker logs openmu-startup`. Test accounts: `test0` .. `test9`, `testgm` (password = user name).
- Formatting/lint used by CI: `clang-format` 21 (`pip install clang-format==21.1.8`, binary in
  `~/Library/Python/3.12/bin`), `cppcheck` (Homebrew). CI checks only the changed line ranges.

## 4. Command cheat sheet

```bash
# build + test (player client)
cmake --build --preset macos-arm64-release && ctest --test-dir out/build/macos-arm64 --build-config Release --output-on-failure
# run the client
(cd out/build/macos-arm64/src/Release/Main.app/Contents/MacOS && ./Main)
# server
(cd ../OpenMU/deploy/all-in-one && docker compose -f docker-compose.mumain-local.yml up -d)
# model round trip
out/build/macos-arm64/tools/bmdconv/Release/bmdconv bmd2smd src/bin/Data/Item/Sword01.bmd work/Sword01
out/build/macos-arm64/tools/bmdconv/Release/bmdconv smd2bmd work/Sword01/Sword01.smd work/Sword01.bmd --manifest work/Sword01/Sword01.actions.txt
out/build/macos-arm64/tools/bmdconv/Release/bmdconv compare src/bin/Data/Item/Sword01.bmd work/Sword01.bmd
```

## 5. Decisions taken

- Reuse the engine's own SMD parser and BMD writer for conversion instead of a separate
  Python writer, so converted files are what the game reads by construction.
- Valve SMD is the exchange format with Blender (Blender Source Tools), because MU's BMD is a
  descendant of the Half-Life SMD pipeline: same rotation conventions, one bone per vertex.
- Server runs in Docker from the official `munique/openmu` image, bound to localhost only.
- Docs for assets are usage-level (`docs/asset-pipeline.md`); agent state lives in
  `docs/agents/`.

## 6. Open work, in priority order

1. **Lorencia static art**: All 106 in-scope static models are accepted offline and on `main` (the rebuild merged with PR #11, merge commit `7b808473`, 2026-09-22; the four preserved pilot props were already there); the 17 terrain paintings remain unchanged. Production is complete for this inventory. Client acceptance and genuine 1920×1080 before/after captures remain pending under explicit offline authorization; investigate stability separately. Follow-up art goes through [regeneration requests](../../assets-work/World1/requests/README.md) and the generated per-model [`catalog.json`](../../assets-work/World1/catalog.json); read the [consolidated handoff](../../assets-work/World1/coordination/notes.md) and [asset board](../../assets-work/World1/coordination/asset-board.md) first. The rebuild's branches and worktrees are historical, so start from `main`. `validate_integration.py` still compares against the pre-merge baseline `ac0f6dd8`; adapt it before relying on it on `main`.
2. **World editor on macOS** (branch `feat/world-editor`, plan in
   [`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md), usage in
   [`MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md), which starts with a quick
   start for the owner): M1 to M7 are done and reviewed, all uncommitted in the working tree.
   - Build with the `macos-arm64-mueditor` preset; `./Main --editor` starts the editor and
     `./Main --editor --world N` opens `Data/World{N}` offline (no server, no login) with the
     FreeFly camera and the Map Editor open. Editor clients run under Metal API validation
     (`MTL_DEBUG_LAYER=1`); use it when checking rendering changes. A Debug build alone does not
     switch it on.
   - Saves land in the game's `Data` and in the repo's `src/bin/Data` (the replaced repo file is
     kept in `out/editor-backups/`). Object delete/undo is safe, lighting no longer drifts after
     sculpting, object drag follows the ground. A map unload (teleport, character screen, or the
     same map loading again, also while the panel is closed) drops the selection and every undo
     step. Objects keep their record order, so a save without edits writes `EncTerrain{N}.obj`
     unchanged and git shows only the records you changed.
   - Editing comfort (M6): multi-level undo/redo shared by all editing tabs (Cmd+Z,
     Cmd+Shift+Z), multi-select, a move/rotate/scale gizmo (W/E/R, snap), Duplicate (Cmd+D) and
     the Outliner window (a pick there switches the Objects tab to Select & edit). A click that
     closes a menu, list or colour picker never reaches the world; Esc closes menus and keeps the
     selection.
   - Terrain comfort (M7): round brushes with a soft edge and an outline on the ground (Height:
     raise/lower, flatten, smooth, set height; Texture layer 2; Attribute; [ ] keys), objects
     follow sculpted ground, and a **Light** tab that paints and saves `TerrainLight.OZJ`. Editor
     lines use `RenderScreenLines` (pixel width, visible from above); one `RenderQuad3D` call draws
     at most 4096 quads, so large overlays go through `Render::Topology::ForEachQuadBatch` (the tile
     grid and the Attribute overlay do); see gotcha 16 in MAP_EDITOR.md.
   - The **Assets** tab lists every catalog model with its live placement count and details,
     outlines and visits its instances, records client verdicts in
     `assets-work/World1/client-review.json` (folded into the catalog by
     `build_editor_catalog.py`) and files regeneration requests that pass `validate_request.py`.
     A request reaches Codex only once its folder is committed and pushed to `main` on `origin`;
     the dialog warns when `HEAD` is on no `origin` branch or a target's BMD differs from the
     catalog, and leaves out `obj_index` while `EncTerrain1.obj` has uncommitted changes.
   - A/B compare: after `python3 tools/world_editor/materialize_variant.py original --world 1`
     (writes `out/ab/original`, about 4 MB) the tab switches one model or all between
     `src/bin/Data` and the pre-rebuild originals without a restart. Models the map load read
     show `as built` (the last build's `Data` copy); press **Current** before judging files you
     just pulled or Codex just delivered.
   - Not yet tried by hand: choosing a file in the SDL file dialog, the Item/Skill editors, the
     request dialog's **Open folder** and preview buttons, **Copy command**; the M6/M7 buttons were
     driven by injected mouse and key events only. Windows and Linux were not built. For scripted
     UI checks, a driver library loaded with `DYLD_INSERT_LIBRARIES` must switch off the ImGui SDL3
     backend's global-mouse fallback, or a focused client window replaces the scripted cursor with
     the real one.
3. **Audio data**: add `Data/Sound/*.wav` and `Data/Music/*.mp3` (formats the code expects).
4. **CI hygiene**: `cppcheck` from Homebrew reports pre-existing findings in
   `src/source/Render/Models/ZzzBMD.cpp` (old-style casts, a `%ld` format); CI's cppcheck version
   may differ, so check the CI run of the first PR that touches that file.
5. Later phases: static objects, items, characters, terrain, new content (see the plan).

## 7. Logs and where to look when something fails

| Symptom | Look at |
|---------|---------|
| Client window opens but nothing loads | `MuError.log` beside `Main` (asset paths, GPU driver, fonts) |
| Client dies in Metal, CoreFoundation or XPC code with an address like `0x1` | Heap corruption in the client, not a driver bug. Build with `-fsanitize=address` (see WORKLOG 2026-09-22) and run from a normal Terminal, not a sandboxed tool shell. |
| Client stops after the `<UI font>` log lines, idle, no window content | After a crash macOS asks whether to reopen the app's windows, and that dialog blocks the start. Answer it, or start scripted runs with `./Main ... -ApplePersistenceIgnoreState YES` (affects only that run). |
| Client cannot connect | `docker ps`, `docker logs openmu-startup`, `config.ini` ServerIP/Port (44406) |
| Build fails on a standard header | stale libc++ folder, see section 3 |
| Model does not load in game | `bmdconv info` on the file; `bmdconv validate` on its SMD; texture names vs files |
| Texture looks padded or flipped | `mu_texture.py check` (power of two, TGA origin) |

## Lorencia rebuild integration — 2026-09-22

The completed offline static environment pass was integrated on `art/lorencia-rebuild` in the
sibling worktree `MuMain-lorencia-rebuild`, and was merged into `main` with PR #11 on 2026-09-22
(merge commit `7b808473`, PR head `5b792e9f`). That worktree and the per-batch worktrees no
longer exist; branch and worktree names in the coordination files are historical. Use the
[asset board](../../assets-work/World1/coordination/asset-board.md) and
[consolidated handoff](../../assets-work/World1/coordination/notes.md) before claiming any
World1/Object1 asset. All 106 static models are covered, including four preserved pilot props;
nine fauna/hidden-marker models remain excluded. The final production integration is
`973ab58d`; subsequent review/evidence/handoff commits are recorded in the ledger.
The [exact game-file manifest](../../assets-work/World1/coordination/changed-game-files.md)
lists 98 BMDs and 85 texture containers changed relative to reviewed pilot `ac0f6dd8`.
All 141 other World1/Object1 files remain byte-identical. Completed terrain, TerrainLight,
alpha strips, placement/height/walk data, Beer01 and earlier props remain protected.
The [combined gallery](../../assets-work/World1/coordination/final-review.md) uses actual
integrated models/materials and retains per-model provenance hashes. All validation and
preview evidence is offline: this task has not installed runtime assets or verified them
in the client. No engine/CMake/UI edits were authored for this pass. Following explicit publication
authorization, the branch was pushed and [PR #11](https://github.com/vaskodagamo/MuMain/pull/11)
was opened against main; it was merged on GitHub on 2026-09-22 (merge commit `7b808473`). The
merge brought exactly the PR's 179 Object1 game files to main (four tavern files were already
there); the reviewed World1/Object1 bytes are unchanged. Client verification is still pending.
New work on these models starts from `main` as a
[regeneration request](../../assets-work/World1/requests/README.md); the editor and agents read
per-model facts from [`catalog.json`](../../assets-work/World1/catalog.json).
