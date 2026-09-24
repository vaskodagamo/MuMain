# Work log

Chronological record of what was done on this fork, by whom (human or AI agent), with the
outcome and the next step. **Every session appends an entry at the end.** Keep entries factual
and short; link to docs instead of repeating them. Dates are ISO (YYYY-MM-DD).

Entry template:

```
## YYYY-MM-DD - <short title> (<agent or person>)
**Goal:** ...
**Done:** ...
**Verified:** ... (commands run, results)
**Open / next:** ...
```

---

## 2026-09-22 - macOS environment, first build, local OpenMU server (Claude Fable 5.1)
**Goal:** Prepare the user's Apple Silicon Mac to build and run this client against OpenMU, as
the base for a custom client with new assets.

**Done:**
- Installed cmake, ninja, pkgconf, glslang, spirv-cross (Homebrew), .NET 10 SDK (`~/.dotnet`,
  PATH block in `~/.zshrc`), Blender 5.2.2 plus the Blender Source Tools add-on.
- Initialized the SDL and imgui submodules; added `upstream` (sven-n/MuMain).
- Configured and built the player client in `out/build/macos-arm64` (preset `macos-arm64`,
  editor OFF, Release) with `OPENSSL_ROOT_DIR` from Homebrew and a libc++ include workaround;
  wrote [`docs/build/macos/console.md`](../build/macos/console.md) and linked it.
- Cloned OpenMU next to this repo (`../OpenMU`) and started it with a localhost-only compose
  file (`deploy/all-in-one/docker-compose.mumain-local.yml`): admin panel `127.0.0.1:8090`,
  connect port 44406 for this client.

**Verified:** 209/209 unit tests pass; the client starts under Metal, loads the Native AOT
network library, connects to the local server and receives the server list (login scene).
Later the same day the owner played a full session on the Mac: server selection, login as
`test0`, character selection, two minutes in the main scene, clean shutdown (no crash report).

**Open / next:** editor build (`ENABLE_EDITOR=ON`) fails on macOS: three Map Editor files use
the Win32 file dialog. The repository data has no `Data/Sound` or `Data/Music` and misses
most of `Data/Object74`. The machine's Command Line Tools carry a stale libc++ header folder
(see the macOS guide); the permanent fix needs `sudo`.

## 2026-09-22 - Asset tooling groundwork: bmdconv, texture tool, Blender scripts (Claude Fable 5.1)
**Goal:** Lay the groundwork for regenerating all graphics with Blender-driven AI.

**Done:**
- `tools/bmdconv/`: command-line converter between BMD and Valve SMD using the engine's own
  parser and writer (`info`, `validate`, `bmd2smd`, `bmd2smd-dir`, `smd2bmd`, `compare`).
  Built by default (`MU_BUILD_ASSET_TOOLS`), tests in `tests/tools/`.
- Engine fixes found on the way: `FixupSMD` grouped triangles wrongly when they shared the
  first texture (sentinel bug); `BMD::Save2` wrote through a fixed 1 MB buffer and a 64-char
  path buffer (overflowed on `Player.bmd`); root-motion arrays were allocated for unlocked
  actions; `TIME_MAX` raised from 100 to 256 frames because a shipped NPC has 105.
- `tools/mu_texture.py`: wrap/unwrap/check for `.OZJ`/`.OZT`/`.OZB`.
- `tools/blender/mu_bmd_import.py` and `mu_bmd_export.py`: BMD to `.blend` and back through
  Blender Source Tools, preserving action order and lock flags.
- Docs: [`docs/asset-pipeline.md`](../asset-pipeline.md),
  [`ASSET_REGENERATION_PLAN.md`](ASSET_REGENERATION_PLAN.md), [`HANDOFF.md`](HANDOFF.md), this log.

**Verified:** `Sword01`, `Monster01` and `Player.bmd` (284 actions) round-trip BMD -> SMD ->
BMD as `EQUIVALENT` in `bmdconv compare`; texture wrap/unwrap is byte-identical on shipped
files; tool tests 5/5 (211 assertions); full suite 214/214. Blender round trip
(`mu_bmd_import.py` -> Blender 5.2.2 + Source Tools 3.4.3 -> `mu_bmd_export.py`) on
`Monster01.bmd`: `EQUIVALENT`, 0 unmatched triangles, max corner deviation 0.0007 units, max
bone deviation 0.0002 units over all 7 actions, bone order and lock flags restored.

**Open / next:** first real asset pass per the plan (Phase 1 textures, World1). Port the three
Map Editor file dialogs to SDL so the editor builds on macOS. Add sound/music data.

## 2026-09-22 - World1 pilot, offline art and validation (ASTRA / Codex)
**Goal:** Establish the Lorencia terrain benchmark and exercise one static prop through the
Blender/BMD pipeline on `art/world1-pilot`, branched from main at `9a8b2027`.

**Done:** Inventoried and unwrapped all 32 World1 texture containers. Repainted all 17 base
filenames explicitly listed in the brief (14 loaded terrain slots plus three legacy variants)
at 512×512. Rebuilt Beer01 as the tavern still life it actually contains, after user
confirmation: bottle, mug, bowls, grapes and vine. Exported 784 triangles with an exclusive
512×512 plate2 atlas, retaining all five bones and the original one-frame action. Preserved
original geometry, packed Blender sources, higher-resolution source geometry, raw paintings,
scripts, comparison renders and validation reports under
[`assets-work/World1/`](../../assets-work/World1/notes.md). Installed the 19 exported files in
source and runtime Data folders. No engine/CMake files or filenames changed.

**Verified:** All 18 replacement texture containers pass mu_texture checks; decoded terrain
JPEG edge mismatch averages at most 0.424/255 (maximum single-channel delta 5/255). Reviewed
3×3 repeats offline. Reference and action SMDs pass bmdconv validation. Full-model compare
reports DIFFERENT for the intended geometry change (216 → 784 triangles; 5 → 1 meshes),
with zero bone-motion deviation. Isolated original/replacement skeletons and actual actions
compare EQUIVALENT. Bind sizes: 91.20×58.57×61.80 → 91.19×58.48×61.80 units. All 35 protected
World1 files, including TerrainLight and alpha strips, match their baseline hashes. All 19
installed file hashes match the exports in both source and runtime.

**Open / next:** **Not verified in client.** The unchanged client repeatedly crashed in
server selection/Lorencia, including Metal command-buffer assertions, before the three
1920×1080 baseline captures were completed. The user explicitly authorized offline
continuation and deferred client verification. Resolve stability separately, obtain baseline
town/grass/rock screenshots, compare replacements at matching views and inspect Beer01 near
Lorencia `(127.4,128.4)`. The delivered Blender comparison is labeled offline. The visual
benchmark and end-to-end proof remain pending client acceptance.


## 2026-09-22 - Three Lorencia static props, offline continuation (ASTRA / Codex)
**Goal:** Continue `art/world1-pilot` with three visually identified props, preserving engine
contracts and completed terrain/Beer01. Client stability explicitly outside scope.

**Done:** Inventoried all 115 Object1 BMDs, imported/rendered eight candidates and decoded
World1 placements read-only. Selected Candle01 (three-candle stand, 6 placements),
TreasureChest01 (arched timber chest, 3 placements), and Tomb03 (upright grave marker,
5 placements). Rebuilt them at 1,018 / 1,174 / 200 triangles from 116 / 66 / 30. Repainted
exclusive candle, treasure_chest and tombstone atlases at 512×512, plus candle2 at 128×128.
Retained all original game filenames and mesh slots. Preserved originals, packed Blender
sources with REF_ORIGINAL and excluded higher-resolution sources, paintings/prompts,
exports, raw validation data and labeled offline before/after, wireframe, action and scale
reviews under [`assets-work/World1/`](../../assets-work/World1/notes.md).

**Verified:** Reference/action SMDs and all four textures pass engine/loader validation.
All three isolated skeleton/action comparisons EQUIVALENT; names, order, parents, action
order, lock=0 and 7/7/1 frame counts unchanged. Local translations/rotations checked over
all keys (largest component differences 0.000015 units / 0.0000003072 radians modulo 2π).
Candle's six original flame triangles and UVs separately verified; original BlendMesh=1
material order retained. Intentional full-model comparisons DIFFERENT. Bind sizes remain
38.67×22.47×87.06, 114.57×64.32×84.04 and 89.76×19.64×117.25 units; chest keyhole adds only
0.0036 units of front projection. Packed sources reopened, final BMDs re-imported for review.
Seven replacements installed with matching hashes in the art source and existing runtime;
317 other World1/Object1 files remain unchanged in each, including all World1 terrain,
TerrainLight/alpha strips and Beer01. No engine/CMake edits.

**Workspace coordination:** Another process switched the shared primary checkout to main
during validation. The installation guard refused the changed baseline before writing.
Continued on the existing art/world1-pilot branch in the isolated worktree
`/Users/webproduktion3/.codex/worktrees/world1-static-batch/MuMain`; no commits or replacement
files written to main. The existing runtime is still the primary checkout's macOS app.

**Open / next:** **Not verified in client.** User-authorized offline continuation remains.
Check real client loading/logs, candle additive blending/flicker, lighting, gameplay zoom,
object contacts/orientation and matched screenshots once stability is addressed separately.
Useful review tiles: Candle01 `(126.58,128.25)`, chest `(185.17,140.06)`, Tomb03 `(130.50,215.00)`.
The delivered renders are Blender evidence, not client screenshots or visual acceptance.

## 2026-09-22 - Publish the World1 pilot for review (ASTRA / Codex)
**Goal:** Commit, push and open a PR for the completed art branch.

**Done:** Confirmed the asset work was already committed and the art worktree clean. Pushed
`art/world1-pilot` to origin and opened [PR #5](https://github.com/vaskodagamo/MuMain/pull/5)
against `main`, covering the complete branch: 17 terrain repaints and Beer01, Candle01,
TreasureChest01 and Tomb03, including sources, paintings, previews and validation evidence.
Recorded this publication in a separate documentation commit.

**Verified:** Refreshed origin/main, reviewed the branch scope (26 replaced game files,
World1/Object1 plus asset-work/handoff files), and passed `git diff --check`. The PR records
previously completed offline validation and explicitly labels all preview images as Blender
renders. No asset changes or new engine build were made during publication.

**Open / next:** PR review and the previously deferred client loading, lighting/blending,
placement and screenshot checks. Client verification is still pending; no merge requested.
## 2026-09-22 - Right HUD material benchmark, offline pilot (ASTRA / Codex)
**Goal:** Establish a dark medieval UI art benchmark while preserving the existing
asset layout, filenames, dimensions, state alignment and engine behavior.

**Done:** Created `art/ui-pilot` from `main` (`9a8b2027`) in the separate
`../MuMain-ui-pilot` worktree. Inventoried 760 Interface images, retained all
untouched payloads and produced 26 labeled contact sheets. Traced active HUD
loads, slices, UVs, scaling, state remapping and alpha behavior read-only. Used
the imagegen skill/built-in tool for five paintings; assembled native-size
OpenRaster sources, PNG masters, JPEG payloads and OZJ exports. Repainted
`Interface/partCharge1/newui_menu03.OZJ` (exposed trim and empty green well) and
`newui_menu_Bt01.OZJ` through `newui_menu_Bt04.OZJ` (Character, Inventory, Friends,
Menu). Installed only those validated source Data files in this worktree.
Prompts, scripts, inventory, exact mappings and review notes are under
[`assets-work/UI/`](../../assets-work/UI/notes.md).

**Verified:** `assemble.py`, `preview.py`, `validate.py --install`; every exported
file checked with `tools/mu_texture.py check`. Exit 0, no rejections; the five
non-power-of-two warnings exactly match the shipped originals (256×51 panel,
30×164 buttons). No resize or atlas change. Protected PNG master pixels are
identical; JPEG maximum per-channel error is 4/255. All 760 original payload
hashes match. Reviewed all four control states at native size and in enlarged
crops, light/dark opacity, and 1920×1080 offline before/after mockups using the
actual HUD geometry. Source Data/export bytes match. No engine, CMake, World1,
Object1 or shared runtime changes; no client stability work.

**Open / next:** Client verification remains pending under the owner's offline
authorization: load errors, actual hover/selected/alert behavior, dynamic text
and skill/gauge overlays, HiDPI, resizing and gameplay readability. The mockups
are explicitly labeled offline reconstructions. Cash-shop and remaining HUD
art are unchanged dependencies, outside this five-file pilot.
## 2026-09-22 - macOS client crashes: miniaudio use-after-free on missing audio files (Claude Fable 5.1)
**Goal:** Find and fix the recurring crashes of the macOS client (ten crash reports on this day:
IOGPU assertion and blit-encoder assertion in `EndFrame()`, `objc_release` of `0x1` on the Metal
completion queue, CFPrefs walking `0x1` from `IMKClient`, NSXPC and AudioComponent crashes).

**Done:**
- Read the ten `.ips` reports: every crash site is an Apple framework object holding a pointer
  that is `0x1` or `0x9` (a `0` or `0x8` incremented by one), on different threads and scenes,
  as early as 12 s after launch. That is heap corruption in the client, not a renderer bug. The
  SDL GPU code in `EndFrame()` and the buffer growth helpers are sound: SDL releases buffers
  and textures deferred, by reference count, once the command buffers that use them complete.
- Built the client with `-fsanitize=address` in a second build directory
  (`out/build/macos-arm64-asan`, config `RelWithDebInfo` with `-O1 -g` so asserts stay on;
  add `-fsanitize-recover=address` and run with `ASAN_OPTIONS=halt_on_error=0` to collect
  every finding in one run). Findings, in the order they appeared:
  1. miniaudio 0.11.25 `ma_resource_manager_data_buffer_node_acquire()` reads the node after
     freeing it when a sound file cannot be opened (every `LoadSound` at startup, no `Data/Sound`).
  2. miniaudio's data-stream load job increments `pDataStream->executionPointer` *after*
     signalling the waiting caller. When the file cannot be opened, the caller frees the stream
     on wake-up, so the job thread writes `+1` into freed memory. The login scene and Lorencia's
     safe zone call `PlayMp3()` every frame and the same-track guard never engages on failure,
     so with no `Data/Music` this ran ~75 times per second (38 000 log lines per session). This
     is the mechanism behind the `0x1` / `0x9` pointers.
  3. `BMD::CreateBoundingBox()` indexes the global `BoundingMin/Max` tables with the vertex bone
     index; `Data/Skill/CW_Bow_Skill.bmd` carries `-8888` on an unreferenced vertex and normal,
     so every launch read and wrote far outside those tables.
  4. `ReceiveOption()` reads the 4-byte `QWERLevel` field one byte past the 32-byte option
     packet OpenMU sends (the client struct is 34 bytes). Read only; left as a follow-up.
- Fixes: `cmake/patches/miniaudio-0.11.25-resource-manager-use-after-free.patch` (applied by
  the existing `ApplyGitPatch.cmake` step; that script now stops git's repository discovery at
  the dependency directory, because a tarball dependency inside the build tree was silently
  skipped before), `MiniAudioBackend` remembers a track that failed to open and skips it until
  a different or enforced request (one log line per track instead of one per frame),
  `BMD::Open2()` clamps out-of-range bone indices to bone 0 and reports the model.
- Docs: macOS guide (missing music behaviour), `HANDOFF.md` (state, symptom table, the stale
  libc++ folder is gone), unit test for the failed-track guard.

**Verified:**
- `ctest` 215/215 (Release), including the new audio test.
- Sanitizer build, before the fixes: report within 2 s of launch (finding 1); after the
  miniaudio patch: finding 3; after all fixes, 200 s run in which the owner logged in and
  played in the main scene: only finding 4, clean exit.
- Release build with `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`: 4 min 25 s alive (login,
  character select, main scene), no validation error, no crash report, clean shutdown on
  SIGTERM. Before the fixes the same build died within 16 s to 3 min of the main scene.
- Do not launch the client from a sandboxed tool shell (window server, audio and GPU access);
  the Bash tool needs its sandbox disabled for the run, and `MTL_DEBUG_LAYER_WARNING_MODE=nslog`
  writes gigabytes per minute (sampler descriptor dumps), so keep warnings off.

**Open / next:**
- Follow-up chip: size-check the option packet in `ReceiveOption()` before reading `QWERLevel`.
- miniaudio's other resource-manager jobs (`load_data_buffer_node`, `load_data_buffer`,
  `free_data_buffer_node`) touch their object after signalling as well; the client never
  exercises them (sound effects decode synchronously). Report upstream together with the patch.
- SDL 3.4.8 `METAL_INTERNAL_AcquireSwapchainTexture()` does not check `nextDrawable` for nil;
  upstream main is the same. Revisit only if a render-pass crash appears without heap corruption.
- `[UI] EnableAnimationTaskPool=1` (worker threads for character animation) was not tested.

## 2026-09-22 - Lorencia tavern furniture batch (ASTRA / Codex)
**Goal:** Rebuild three additional Lorencia tavern props in an isolated worktree, respecting
parallel asset ownership and the original engine contract.

**Done:** Created `art/lorencia-tavern-props` from `main` at `9a8b2027` in the sibling
`MuMain-tavern-props` worktree. Inspected active worktrees/handoffs, all 115 Object1 models,
geometry and World1 placements; claimed Furniture03/04/05 and their exclusively shared
`desk_big.OZJ` before production. Rebuilt the four-legged table, half-round pedestal table
and modular counter with coherent carved oak and restrained iron. Delivered immutable
originals, packed Blender sources with `REF_ORIGINAL` and editable high-poly references,
layered texture sources/prompts, BMD/OZJ exports, offline comparisons and validation reports.
Installed exactly those four files into this worktree's source Data. Batch handoff:
[`assets-work/World1/TavernProps/notes.md`](../../assets-work/World1/TavernProps/notes.md).

**Verified:** 680/324/412 triangles (all below 1,500), one original mesh/bone/action per model,
one frame and lock=0. `bmdconv validate` and every `mu_texture.py check` pass; rig-only
`compare` is EQUIVALENT for all three, with zero local bone translation/rotation deviation.
Bounds and modular joining corners match at SMD precision; full-model DIFFERENT results
are intentional remodeled geometry. Blender source audits preserve original geometry,
UVs/skin/transforms and packed images. Re-imported exports were reviewed from matching
cameras, reduced scale, reverse views, wireframes and original repeated placements.
Final exports have no zero-area UV faces or winding/normal disagreements. Installed hashes
match exports; 320 non-claimed Object1/World1 files remain byte-identical. No engine/CMake,
UI, Beer01/plate2, terrain/placement or shared-runtime changes; no client launch.

**Open / next:** User-authorized offline continuation leaves client acceptance pending.
In a stable coordinated client session, check load logs, runtime lighting/filtering/culling,
table silhouettes and the paired-half-table/counter seams at recorded placements; capture
matched 1920×1080 before/after views. All supplied previews are labeled offline Blender.

## 2026-09-22 - Publish the tavern furniture batch (ASTRA / Codex)
**Goal:** Commit, push and create a PR for the completed tavern furniture work.

**Done:** Pushed `art/lorencia-tavern-props` and opened
[PR #6](https://github.com/vaskodagamo/MuMain/pull/6) against `main`. Merged the latest
main (`300911ed`) first, preserving every work-log entry when resolving the sole conflict.
Updated the installer to protect other artists' committed Data against HEAD after a main
merge while retaining original-backup and claimed-export hash checks. The PR includes
an offline preview, exact four-file game scope, validation evidence and pending client checks.

**Verified:** Installer preflight passes with all four exports matching recorded hashes and
320 protected files matching committed HEAD. The PR game diff contains only Furniture03,
Furniture04, Furniture05 and desk_big.OZJ; `git diff origin/main...HEAD --check` passes.
No assets were regenerated, runtime files written, client session launched or engine build
performed during publication. Prior offline validation remains applicable.

**Open / next:** PR review and previously deferred client acceptance. The PR is not merged.

## 2026-09-22 - Modern UI revision after visual feedback (ASTRA / Codex)
**Goal:** Make the five-file right-HUD pilot visibly cleaner and more readable after
the first pass failed the user's visual expectations.

**Done:** Created isolated `MuMain-ui-modern` / `art/ui-modern-pilot` from main
`7a88d829`; the earlier pilot PR #4 was already merged. Generated five new painted
sources with the built-in imagegen tool and repainted complete button faces with
dark metal, bold pale symbols and a gold selected-state underline. The user selected
the clean, restrained dark-fantasy direction. Repainted the emerald skill well and
XP trough while retaining the AG/mana backing required by unchanged opaque gauge
fills. Kept filenames, dimensions, atlas boundaries and all interaction geometry.
Retained first-pass payloads/prompts for comparison, updated editable sources and
reproduction scripts, and produced eight offline previews including actual 1080p
control sizes and both existing HUD layout modes. Installed only the same five
validated OZJs into this worktree's source Data. Details:
[`assets-work/UI/notes.md`](../../assets-work/UI/notes.md).

**Verified:** `mu_texture.py check` exits 0 for every export with only the same five
pre-existing NPOT warnings. Verified 760 original payload hashes, unchanged opaque
RGB dimensions, wrappers, protected panel pixels, state ordering and JPEG error
at most 4/255 per channel. Reassembly reproduced all ten master/export hashes;
all five editable ORA composites match their masters. Inspected native, all-state,
1080p and light/dark previews. No engine, CMake, World1, Object1 or shared-runtime
changes; no client launch or engine build.

**Open / next:** Client loading, GPU filtering, localized text, skill/counter
overlays, input/alert states, gameplay readability and HiDPI checks remain pending.
The rest of the HUD is outside this five-file benchmark. All previews are labeled
offline mockups, not client screenshots.

## 2026-09-22 - Lorencia rebuild coordination and first integrations (ASTRA / Codex)
**Goal:** Rebuild the placed static environment in independent worktrees while preserving
completed terrain/Beer01 and all gameplay-bound data.

**Done:** Created `art/lorencia-rebuild` in the sibling `MuMain-lorencia-rebuild` worktree
from reviewed World1 pilot `ac0f6dd8`. Inspected all task/worktree ownership and existing
handoffs, mapped 115 BMDs/105 texture dependencies with exact World1 placements, and imported
and visually identified all 106 in-scope assets. Added coordinator-only
[`asset-board.md`](../../assets-work/World1/coordination/asset-board.md), immutable baseline
inspection scenes, six review sheets, dependency map and integration ledger. Independent
review accepted prior Furniture03/04/05; integrated their original and production commits.
Parallel subagents completed Grass01/02/05/06 surface paintings and Fence01–04 remodels;
coordinator reviewed actual export images and integrated each validated batch sequentially.
Trees, tall scrub, barrel/crate/lantern and the next shared rock group are assigned separately.

**Verified:** Tavern joins and original rig/actions independently pass; groundcover full-model
comparisons are EQUIVALENT; fences intentionally DIFFERENT for geometry with EQUIVALENT
rig/actions. Coordinator rechecked installed groundcover BMDs. Independent cross-review
accepted fence visuals and actual-placement joining sheets. Combined model scan resolves all
textures, installed hashes match exports and untouched original archives match pilot history.
Fifteen game files differ from the integration baseline; all other 309 World1/Object1 files,
including all terrain/light/placement/alpha data and completed pilot assets, are unchanged.
No runtime installation, client launch, engine/CMake/UI edit, push or main merge.

**Open / next:** Production continues through the board. These are offline acceptances only;
no asset was observed in client by this task. All client loading, shading/filtering, motion,
placement and matched 1920×1080 capture checks remain pending. Consolidated handoff:
[`coordination/notes.md`](../../assets-work/World1/coordination/notes.md).

## 2026-09-22 - Lorencia static inventory completed offline (ASTRA / Codex)
**Goal:** Complete the coherent dark-medieval Lorencia static art pass through the actual
World1/Object1 inventory, with independent worktrees, exclusive texture ownership and
sequential reviewed integration.

**Done:** All 106 in-scope static models are accepted offline on `art/lorencia-rebuild` in
`/Users/webproduktion3/Documents/claude-test-mumain/MuMain-lorencia-rebuild`. This includes
four preserved pilot models and 102 accepted integrations (including prior tavern work),
covering furniture/tableware, buildings and modular walls, gates/fences, vegetation/rocks,
lamps/fire props, notices/banners, cart/hay sets, monuments/graves, boat, wells/pottery,
bridges, cannons and gallows. The three worker agents (groundcover, fences, reviewer) and
coordinator used isolated branches/worktrees with explicit BMD/container ownership; shared
materials were reviewed across all consumers. Final production integration `973ab58d`
completes Cannons01; every source/integration/review commit and exact asset owner is recorded
in the consolidated ledger and board. All production sources, REF_ORIGINAL, originals,
editable paintings, imagegen masters/prompts, reproduction scripts and offline evidence
are retained under assets-work/World1. Six final combined sheets show all 106 actual
integrated model/material combinations with per-model SHA-256 provenance.

**Verified:** Combined validation passes for all 115 inventory BMDs and complete texture
resolution. Exactly 183 source-game files differ from reviewed pilot ac0f6dd8: 98 Object1
BMDs and 85 Object1 texture containers; all 141 other baseline World1/Object1 files are
byte-identical. Original archives and source/export hashes match. Rig names/order/parents,
actions, mesh-material ordering, original anchors and modular openings are preserved.
Remodeled full compares correctly report DIFFERENT; rig/action equivalence is proved
separately. The coordinator's bidirectional named-bone vertex audit passes all 72 remodeled
models; raw normal-node checks pass 101 changed/retained BMDs across bind and all keys.
Grass02/Tree12/Tree13 retain exact original BMD bytes with new paintings after converter
normal-sharing review; pre-existing Tree12/13 behavior is preserved, not declared repaired.
Independent review accepted the batch packages and cross-batch completeness; two final
cannon visual issues (bright atlas gutters and dark muzzle rims) were corrected before
acceptance. Protected completed terrain/Beer01/other pilot files remain unchanged.
No engine/CMake, UI, characters, monsters, equipment, other maps, terrain placement/height/
walk/lighting/alpha data, shared runtime, push or main-merge changes were performed.

**Open / next:** No production asset remains blocked or unfinished within the 106-model
static scope. Nine fauna/hidden-marker models are excluded and unchanged. **Client verified:
none in this task.** Existing client instability and the user's explicit offline authorization
leave loading, shading/filtering/alpha, engine effects, motion, collision/interaction and
1920×1080 before/after captures pending. Do these serially in a stable client session; all
current images are labeled offline Blender evidence. Consolidated handoff, complete owners/
commits, exact changed paths and review gallery:
[coordination/notes.md](../../assets-work/World1/coordination/notes.md).


## 2026-09-22 - Publish complete Lorencia static rebuild (ASTRA / Codex)
**Goal:** Follow the owner's explicit request to commit, push and create a PR for the
completed static environment pass.

**Done:** Merged origin/main at 0f589224 into art/lorencia-rebuild (179fa7b9), preserving
all worklog entries and taking main's newer tavern handoff and guarded install helper.
Pushed the branch to vaskodagamo/MuMain and opened [PR #11](https://github.com/vaskodagamo/MuMain/pull/11)
against main. The PR describes the complete offline validation, retained editable sources,
exact game scope and pending client acceptance. Added a PR-specific game-file manifest and
publication metadata; updated the consolidated handoff and board with publication status.

**Verified:** All 183 reviewed Object1 export hashes still match b850741b; the main merge
changed no World1/Object1 bytes. Relative to current main, the PR changes only 179 Object1
game files (95 BMDs and 84 containers): the four TavernProps files were already merged in
PR #6. No engine/CMake/UI/other-map changes occur in the PR diff. Python syntax and authored
Python/Markdown/JSON whitespace checks pass. Retained converter logs keep their original
output whitespace. Previous art validation and independent acceptance remain applicable;
no assets were regenerated or runtime files written, and no client or engine build ran.

**Open / next:** PR review, CI and the previously deferred real client acceptance/captures.
The PR has not been merged. Publication supersedes the earlier no-push instruction for this
branch; the boundary against merging into main remains.
## 2026-09-22 - Extend the approved style across the bottom HUD (ASTRA / Codex)
**Goal:** Remake the other HUD elements after the user merged PR #9.

**Done:** Created `MuMain-ui-hud` / `codex/ui-hud-completion` from merged main
`0f589224`. Inspected actual loading/drawing and shared uses, then selected the
14 connected panel, gauge, XP, cash-shop and normal/selected skill-slot textures.
Generated four imagegen paintings and assembled the exact-size atlases with
quiet dark metal, thin steel/brass trim and matching red/blue/green/gold/violet
resource fills. Replaced the old empty gauge backings together with their opaque
fills. Retained fixed key legends and all dynamic overlay space. Kept the four
approved right-side buttons unchanged. Delivered originals, inventory, prompts,
14 editable ORAs, lossless masters, wrapped exports, nine labeled offline
previews and reproducible assembly/validation scripts under
[`assets-work/UI/HudCompletion/`](../../assets-work/UI/HudCompletion/notes.md).
Installed the 14 validated exports only into this worktree's source Data.

**Verified:** Every `mu_texture.py check` passes with exit 0 and its original NPOT
warning only. All exact dimensions, RGB/alpha=255, OZJ wrapping and cash states
pass. JPEG maximum channel error is 4/255. Verified 760 original UI payloads and
14 merged-baseline container/payload pairs. All 14 editable composites match;
reassembly reproduces 42 master/payload/export hashes. Four empty gauge backings
and five shared frame masks match exactly in masters. Low-fill review caught
and corrected excess generated margin; 10% fills now have visible colored
energy in all four resources. Inspected all fill levels, poison/master variants,
slot/shop states, native size, light/dark edges and both 1080p layout modes.
The source diff contains exactly the 14 claimed assets, with no engine/CMake,
World1/Object1 or shared-runtime changes; no client launch/build.

**Open / next:** Client loading/filtering, resource cut lines, XP gain flashes,
hotkeys/selection/cooldown/disabled overlays, item models and counts, localized
tooltips, HiDPI and real gameplay readability remain pending. Shared skill-slot
art also requires MU Helper and pet-window review. All previews are offline
mockups, not client screenshots.

## 2026-09-22 — Start Lorencia artistic quality continuation (ASTRA / Codex)

**Goal:** Complete genuinely unfinished Lorencia static art before later maps; historical
106/106 acceptance is inventory/technical coverage, not artistic completion.

**Done:** Verified owner-fork main at merged PR #11 revision `7b808473`; preserved primary
checkout uncommitted documentation. Created isolated integration plus two worker worktrees.
Independent reviewer verified all 106 preview manifests and 204 referenced game hashes.
Assigned seven geometry-only masonry/modular assets covering 152 placements, with all
textures frozen. Set up official checksum-verified Blender 5.2.2 and Source Tools 3.4.3
in workspace-local astra-tools; modeling uses Blender Python API.

**Verified:** Headless Blender initialization and add-on setup succeed outside sandbox;
sandbox Metal initialization crashes before Python. Existing Lorencia client success is
user-reported only; no runtime write or client observation by this task. No new asset accepted.

**Open:** Production and independent export/visual review; full current artistic assessment.
See assets-work/Environment/coordination/handoff.md for ownership and exact restart work.

## 2026-09-22 — Accept first Lorencia quality-pass architecture batch (ASTRA / Codex)

**Done:** Integrated worker594b6db2 as9abdf7e8: HouseWall01/04 structural timber bays,
HouseWall05/06 modeled shingle courses, 23 placements. Retained packed Blender sources,
original/current files, reproduction, matching exported diffuse comparisons and assemblies.
Independent reviewer accepted all final hashes after roof-degenerate cleanup.

**Verified:** Converter/model/action checks, exact rig and bounds, one UV layer and full
source/export triangle material/position/UV correspondence, raw normal ownership, frozen
textures, roof modular perimeters, no degenerate triangles. Integrated hash evidence matches;
all115 Object1 models resolve; only the four BMDs changed. Client review pending for new work.
User-reported baseline working in game remains distinct from this offline evidence.

**Open:** Lorencia remains artistically incomplete. Next scrub4 batch covers530 placements;
masonry studies returned for revision because assembly gains did not justify added geometry.
No later-map, engine, shared runtime, placement/collision or terrain changes. Current restart:
assets-work/Environment/coordination/handoff.md.

### 2026-09-22 — ASTRA scrub quality checkpoint

2026-09-22: Scrub01 accepted offline and integrated4b0ccd02 from worker51679ab1. Tree09/10 continuous growth replaces pinched broom waists; Grass03/04 rounded connected crowns replace angular layered shells. Exactly8 game BMDs now differ from baseline; all115Object1 models resolve, frozen textures/protected paths unchanged. 530 scrub placements unchanged. Houses01 assigned to architecture (House01/03/04 only); Broadleaf01 final review pending. No new client verification or later-map work.

2026-09-22: Broadleaf01 integrated072f77f6 from worker0f78f88d. Grass05/06 long smooth arcs replace folded elbows across399placements. Independent full/reverse/reduced/light-dark and bothsix-placement assemblies accepted; technical/rawnormal/rig/UV/source-export checks passed. Combined10changedBMDs/115modelsresolved. Alltextures unchanged. AssignedWell01–04 to furniture underWells02; Houses01/03/04 active witharchitecture.15productioncandidates remain plus5companions/coherence review. No latermaps/clientclaims.

2026-09-23: Houses01 accepted/integrated7be0eaa3 (worker32a1a24b): House01 stone surrounds,House03 supported canopy/flue rims,House04 stepped dome courses. All13gamechanges passcombinedscope/exporthash/115modelresolution. House04original40frame motiontailbyteidentical; fullposed/source/worldnormalproof passes. AssignedStatues02 (StoneStatue01/03,SteelStatue01) toarchitecture; Wells02 finalreviewpending. No client/runtimechanges.

## 2026-09-23 wells checkpoint
Wells02 accepted and integrated `5a0ad5b5` from worker `c1296be9`: Well01–04 open rounded pottery mouths/throats and physical roof boards. Frozen textures, exact structural roof contacts, shared parts and UVs; independently reviewed all four actual-placement groups. Root confirmed BMD/source/image hashes. Combined validation passes 17 changed BMDs and 115 Object1 texture resolutions. New client checks pending.

Architecture owns StoneStatue01/03 and SteelStatue01 (Statues02). Furniture now owns Bonfire01 only (Bonfire02), opaque wood refinement with exact protected additive mesh and frozen textures. Reviewer sharpens remaining masonry direction. Nine production assets remain: four masonry (HouseEtc01, StoneMuWall02/03/04), three statues, Waterspout01 and Bonfire01. Retained-asset/coherence acceptance remains outstanding. No later maps started.

## 2026-09-23 bonfire and retention checkpoint
Bonfire01 accepted/integrated `38292593` from `79ed83a5`: quarter-split wood with curved bark faces and longitudinal grain;38 effect triangles and exact hot-end interface protected. Offline additive approximation/actual tilted placements accepted; client pending. Total18 new replacements. Independent individualized retention decision for76 baselines in retention-gate.json plus earlier4companions yields80 defensible retained assets; not80new remakes and not client verification. Root rechecked all217retention dependencyreferences/76imagehashes.

Coherence01 `ec35b6f4` contains5actual neighborhood regions,121instances,30matching images with complete source/placement/hash provenance; sampled material coherence accepted by reviewer with no-terrain/effect limits. Remaining8production:3statues atfinalreview,4masonry nowownedbyfurniture(Masonry02 firstHouseEtcfaceprototype),1fountain unclaimed. OverallLorencia incomplete; latermaps notstarted.

## 2026-09-23 statues checkpoint
Statues02 accepted/integrated `cc43c901` from `10d1e765`: StoneStatue01 carved niche around figure(212tris), StoneStatue03 layered curved feather wings and continuous robe/arm forms(1353), SteelStatue01 stepped capital and raised plaque(274). Root independently verified3BMD/source hashes and62reviewed evidence hashes. Full authored triangle/UV/bone/normal and exactcontact proofs pass; all frozen materials match integration. Corrected context omits engine-hidden PoseBox01 and frames entire pillar.

Total21 accepted new replacements +80 individually defensible retained assets; remaining5production: HouseEtc01/StoneMuWall02/03/04 owned furniture(Masonry02), Waterspout01 owned architecture(Fountain02). Fountain only actualdragon geometry; protect additional34 basin/floor triangles inside slot1 as well as entireother3meshes, all21motionframes/particles/waterinterface. Masonry onefaceprototype must earn gain before expansion; exact8unitborder surface and fullcoverage/nonoverlap proofs mandatory. Lorencia incomplete, latermaps notstarted, allnewclientchecks pending.

## 2026-09-23 contextual masonry retention decision
Independent final reassessment retains unchanged HouseEtc01/StoneMuWall02/03/04. In actual stacks, fortification rows and siege context, the hooked nose/eyes, narrow crest and projecting shields provide readable distinct roles and coherent ornament. The earlier isolated boxiness concern over-weighted coarse clay topology for modular masonry; close-up limits remain explicit. No rejected candidate is counted as a remake. Masonry02 deep734tri first study stretched the painting; shallow810tri second study cured that but added negligible visible gain over50tri baseline. Hash-bound individual rationale and evidence: masonry-retention-review.json. Root verified all4 BMD/texture groups; originals unchanged.

Current Lorencia gate:21 accepted new replacements +84 defensible retained assets =105/106 resolved. Waterspout01 is the last unresolved asset, not yet accepted; full21frame/protected-surface/source-normal checks and actual diffuse/effect views remain. Overallgoal incomplete; no latermap production. Furniture is preparing read-only Dungeon queue after committing rejected studies on its own branch.


## 2026-09-23 — Fountain normal correction and focused art revision

Fountain02 official export now passes unchanged position/UV/normal and all21-frame motion/attachment checks after preserving protected mesh connectivity and water-specific custom-normal fans. Independent reviewer verified all256 required protected triangles. First sculpt and subsequent wing-curve study remain unaccepted artistically; fuller membranes still leave a weak dark front torso. Scoped reagon_waterspout.OZJ painting authorized after exact consumer/UV analysis; all other shared textures stay frozen, and internal basin texels plus filtering guard require exact decoded preservation. Built-in imagegen produced an editable atlas master and retained prompt; packaging/model review pending. No fountain game files installed. Lorencia remains21 replacements +84 retained,1 unresolved; later-map readiness is read-only. Published draft PR12 through7929166c.


## 2026-09-23 — Lorencia artistic/offline gate passed

Independent final gate at bb641d04:22 accepted replacements plus84 individually justified retained assets cover all106 in-scope assets. This is not106 new remakes. Final Fountain02 integrates187208c2 as bb641d04: fuller chest, bowed wing membranes and broad charcoal stone painting;921 triangles,11 bones,21 keys. Its dedicated128² dragon atlas changes while all1694 basin/filter-margin texels remain exact through engine-equivalent decoding. All other textures remain unchanged.

Combined scope is22 BMDs plus1 atlas, all115 Object1 models resolve textures. Final reviewer verified accepted-report hashes,254 retained dependency references and five mixed neighborhood comparisons (62 source records,30 images), plus fountain actual placement/effect views. No unresolved known offline production defect remains. Generated JPEG patcher executable is rebuilt from retained source, intentionally untracked. Reports: lorencia-final-gate.json, fountain-integration-validation.json, retained-current-hash-check.json and final-accepted-source-manifest.json.

New client checks remain pending. Prior baseline7b808473 client success is user-reported only. Offline views sample neighborhoods/assemblies and approximate effects; they do not claim a full-map terrain render or live client test. Overall goal remains unfinished: continue Dungeon static assets in a separate focused branch/PR, potteryObject28/29/30 and coffinObject21/22 with shared textures initially frozen. Read-only name-byte round trips pass through official Blender import/export; see dungeon-name-handling.md.


## 2026-09-23 — Start Dungeon static production

Lorencia passed artistic/offline gate in separate draft PR12 (d3ce9acf); new client checks pending. Created focused main-based Dungeon integration and reused the two owned artist worktrees on new branches. PotteryObject28/29/30 and coffinObject21/22 are in production with all shared textures frozen; raw CP949 model names have a verified official-pipeline roundtrip. No Dungeon replacement accepted yet. See assets-work/Environment/coordination/handoff.md for exact ownership and next steps. Primary checkout/runtime untouched.


### 2026-09-23 — Dungeon pottery acceptance and next architecture batches

Integrated worker 15a1a4c4 as 6d0323d7: Object28 and Object29 ceramic forms accepted after independent exported-art and engine-contract review. Exact game scope is those two BMDs; all textures frozen and all 63 model dependencies resolve. Original Object30 and coffin Object21/22 retained on individual visual grounds; coffin studies remain separate at 6cd41d24. Packed sources, comparison assemblies and complete review evidence retained. New client checks pending. Walls01 (Object01/03) and Pier01 (Object04) now have disjoint ownership; Blender Python API production continues with frozen materials. Primary checkout and shared runtime untouched.


### 2026-09-23 — Dungeon wall crown integrated; pier retained

Integrated focused worker commits 81562d69 as c9a34462 for Object01 after independent review accepted the final64-triangle crown chamfer; source/export/integrated hashes match. The dragon plaque, all end faces, textures, original metadata and 1,262 bounds remain protected. Object03’s four-niche baseline and Object04’s carved pier are retained after independent visual review; pier candidates, complete consumer analysis and evidence are committed for future reference. Root combined scope check reports exactly three accepted Dungeon game BMDs and all63 Object2 texture dependencies resolve. New client checks remain pending. Next read-only queue Object45–48 comprises510 bone-remains placements and frozen shared bons.OZJ. Primary checkout/runtime remain untouched.
## 2026-09-22 - World1 editor catalog and regeneration-request contract (Claude Opus 5.5)
**Goal:** Prepare world editor plan milestone M4. Give the in-client editor one per-model file to
read, define how a request from the editor reaches Codex, and remove the stale "PR #11 not merged"
state.

**Done:**
- Added `assets-work/World1/coordination/build_editor_catalog.py`. It generates
  `assets-work/World1/catalog.json` (schema `mu-world-catalog/1`, keyed by World1 object type)
  from the coordination files, the pilot and tavern data hard-coded in `update_board.py` (read
  with `ast`, not copied), the request folders and the history of `HEAD`. Status and identity
  follow the asset board, so the four pilot props are `accepted`. Batches carry the board's
  agent, branch and worktree. `unreachable_commits` and `original.reachable` flag commits that a
  fresh clone cannot `git show`. Requests carry `assigned_to` from their reservation.
- Added `assets-work/World1/requests/`:
  - [`README.md`](../../assets-work/World1/requests/README.md): the lifecycle and where each state
    lives, `start_commit`, new variants, the worker and coordinator rules (including the
    `superseded_game_files` move and the `inspect_final.py` re-run), the fork git rules and an
    example.
  - `request.schema.json` (draft 2020-12).
  - A stdlib-only `validate_request.py`. On the worker branch it allows differences from
    `start_commit` only in the request's `request.json` and `delivery/`, the owned files and this
    log. It compares frozen fields with the filed request, originals and `current_sha256` with
    `base_commit`, and live requests with `catalog.json`.
- Recorded the PR #11 merge (`7b808473`):
  - in `publication.json`;
  - in `write_handoff.py` and `notes.md`;
  - in `update_board.py` and `asset-board.md`, whose header now comes from `publication.json`, so
    a re-run keeps it;
  - in this HANDOFF, together with the PR #10 HUD row.
- Added the fork-only rule to `AGENTS.md` and a request section to `ASTRA.md`.

**Verified:**
- The catalog has 115 models (112 typed, 3 untyped). Each of the 109 placed types resolves to
  exactly one entry. Status is 106 accepted and 9 blocked, and identity, status and the
  pilot/tavern claims equal the board for all 115 rows. 32 recorded commits are not reachable
  from `HEAD`.
- Runs with different hash seeds, and a run in a separate clone, are byte-identical. `--check`
  passes, and no absolute paths appear.
- `update_board.py` regenerates the tracked `asset-board.md` byte for byte.
  `write_handoff.publication_status()` and its Reproduce text match `notes.md`. `write_handoff.py`
  itself was not run, because it would also rewrite the generated revision and worktree lines.
- In a scratch clone at `7b808473`, the README example was filed, reserved, claimed, delivered and
  accepted. Three more requests, including a new variant, were filed and then rejected, withdrawn
  or delivered. 90 checks passed, including:
  - 29 tampering cases on the delivered branch;
  - a stale-request case;
  - the schema subset;
  - `validate_integration.check_exports` with and without the `superseded_game_files` move.
- `validate_integration.py` as a whole was not run.

**Open / next:**
- `validate_integration.py` still compares against the pre-merge baseline `ac0f6dd8`.
- After the first accepted request, re-run Blender `inspect_final.py`. The catalog builder warns
  while provenance is stale.
- Editor side of M4 on `feat/world-editor`: read `catalog.json` (nlohmann), write request folders
  and captures.

## 2026-09-22 - Editor overlay under Metal API validation (Claude Opus 5.5)
**Goal:** Close the open M1 item of the world editor plan: with `MTL_DEBUG_LAYER=1` the editor
client aborted at its first ImGui draw ("render pipeline's pixelFormat (Invalid) does not match
the framebuffer's pixelFormat (Depth32Float)").

**Done:**
- ImGui still draws inside the engine's main render pass (the contract in
  `tests/render/test_imgui_sdlgpu_backend.cmake`). The renderer now builds, in editor builds
  only, a copy of the ImGui SDL_GPU backend's pipeline (same shaders, vertex layout, blend and
  raster state) that declares the main pass's depth format with depth test and write off:
  `src/source/Render/Renderer/SdlGpuEditorOverlayPipeline.*`. `MuEditorCore::RenderDrawData`
  passes it to `ImGui_ImplSDLGPU3_RenderDrawData`. It is released with the other pipelines.
- The pipeline carries its own depth-stencil state, so the earlier depth-state reset before the
  overlay (`ResetDepthStateForEditorOverlay`) is removed. The main pass depth format is one
  named constant, `k_DepthFormat`.
- The contract test now requires the depth-aware pipeline and its depth settings.
- Docs: `MAP_EDITOR.md`, the macOS console guide, HANDOFF item 2 and a HANDOFF symptom row
  (the macOS reopen-windows dialog after a crash blocks the next start), plan status.

**Verified:**
- Release editor, `MTL_DEBUG_LAYER=1` ("Metal API Validation Enabled" in stdout): `--world 1`
  83 s and the login scene 35 s (not logged in), no assertion, full editor UI in the captures.
- Debug editor, `--world 1`: 47 s with the variable unset and 42 s with `MTL_DEBUG_LAYER=1`, no
  assertion. Before the fix the Debug binary with `MTL_DEBUG_LAYER=1` aborted within 2 s with
  the message above. Without the variable a Debug build ran with Metal's validation off (no
  "Metal API Validation Enabled" line), although SDL requests it.
- Devil Square (`--world 10`), where the overlay used to draw as empty panels, shows the full UI.
  A temporary probe that made ImGui use its own pipeline brought the empty panels back
  (reverted, checksum verified).
- Player build and `ctest` 216/216.

**Open / next:**
- Windows (D3D12) was not built here; the new file uses only SDL3 and the ImGui backend's
  shader header, like the backend itself.

## 2026-09-23 - World editor M3: safe, correct editing (Claude Opus 5.5)
**Goal:** Milestone M3 of [`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md): saves that survive the
next build, no dangling pointers on object delete/undo, no lighting drift after sculpting, a
hardened `SaveObjects`, hotkeys that respect typing, and an object drag that follows the ground.

**Done:**
- Saves: `Editor::Files::MirrorSavedFile` (MapEditorFileUtil) + the file-system-only
  `MapEditorRepoMirror.*`. Every Map Editor save and import writes the game's `Data`, then copies
  the file into `<repo>/src/bin/Data`; a repo file with other bytes is first copied to
  `out/editor-backups/<YYYYMMDD-HHMMSS>/Data/...`; identical bytes change nothing. Repo =
  `MU_EDITOR_REPO_ROOT`, else the first folder above the working directory or `SDL_GetBasePath()`
  with `src/bin/Data` and `.git`. Without one: the old copy next to the executable, and the panel
  says why. Server `.att` + HOWTO also go to `out/editor-exports/`. `MirrorNextToExe` is gone.
  Status lines (new `Editor::StatusLine`) list absolute paths; everything is logged.
- Engine (player build too): `Engine::Object::ReleaseReferencesTo` (new `ObjectReferences.*`)
  clears `Operates[]` owners (and `SelectedOperate`) and ends effects, joints and particles
  attached to an object. `DeleteObject` calls it; new `DeleteAllObjects` does it in one pass for
  map change and the editor's undo (`RestoreAll`), so undo no longer re-registers 110 operates on
  top of dangling ones.
- `CreateTerrainNormal(_Part)` rebuild each cell's normal from zero (`ComputeTerrainNormal`).
- `.obj` format in `WorldObjectFile.*` (encode/decode); `OpenObjectsEnc` decodes with it and
  `SaveObjects` writes once through it: `fopen`/`fwrite`/`fclose` checked, header count = records
  written, run-time objects left out (types >= `MAX_WORLD_OBJECTS` are kept only when the loaded
  file had them: World52/58/59/66/69/73/74 ship such records).
- Map Editor: `m_groundHit` copied from the terrain pass before any object pick; drag moves by the
  ground point and keeps the height above ground; **Drop to ground**; Backspace deletes when
  `io.ConfigMacOSXBehaviors`; `UndoShortcutPressed` (Cmd/Ctrl+Z) on Texture, Objects, Height and
  Attribute; no hotkey while `WantTextInput` or a game text box has focus. `SaveHeightMap`
  extracted from the Height tab.
- Tests: `tests/engine/` (object file round trip, every shipped `.obj` re-encodes byte for byte,
  terrain normals equal the old first-load result and stay equal on recompute, reference
  release) and `tests/editor/test_repo_mirror.cpp`. Docs: MAP_EDITOR.md ("Where saves go", keys,
  drag, lighting formula), plan status, HANDOFF item 2.

**Verified:**
- Editor and player builds; `ctest` 236/236 (216 + 20 new). A probe that removed the zeroing made
  the terrain tests fail (reverted, checksum verified).
- `--world 1` with a temporary hook (deleted; grep finds nothing): `TerrainNormal` and
  `BackTerrainLight` after the first load are byte-identical to the old code's dump, and a second
  recompute no longer changes them (old code: every normal doubled, 801 light cells changed).
  110 operates, no dangling owner after moves across blocks, 3 undos (<4 ms each), delete/undo of
  an operate owner; 70 s alive with the game's `SelectOperate` run every frame. Injected
  Backspace deleted, Cmd+Z restored, Backspace in the focused Pos field did not delete. An injected
  drag moved an object by exactly the ground delta, 0.00 above ground before and after. Sculpt +
  undo: normal, light and height hashes equal the pre-sculpt ones; screenshot diff smaller than
  between two unedited runs. Save of objects: `git status` showed `EncTerrain1.obj`, backup equal
  to the tracked bytes; all other saves, imports and the export checked too. Restored afterwards
  (`git checkout`, new files and `out/editor-*` removed, runtime `Data` checksums equal).

**Open / next:**
- Windows/Linux not built here. 3D sounds keep an `OBJECT*` while they play (MiniAudioBackend)
  and are not released on delete. Dragging a tall object by its top moves it at the speed of
  the ground behind it; a drag plane through the grab point (M6 gizmo) would feel more direct.

## 2026-09-23 - World editor M4: Assets tab and regeneration requests (Claude Opus 5.5)
**Goal:** Milestone M4 of [`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md), C++ side: review the
catalog's models in the client and file regeneration requests exactly per
[`requests/README.md`](../../assets-work/World1/requests/README.md).

**Done:**
- Data code without ImGui or engine (`src/MuEditor/Assets/`): catalog and `client-review.json`
  readers/writer (nlohmann `json.hpp`, now on the editor build's include path), `request.json`
  (fields in contract order) and `brief.md` builders, request ids and slugs, next free
  new-variant name, the request folder writer (all or nothing), the checkout's `HEAD` read from
  `.git` (loose refs before packed ones, detached HEAD, worktrees), SHA-256 (OpenSSL) and the
  capture JPEG (box downscale to 1920 px, libjpeg-turbo: the vendored `stb_image_write.h` only
  writes PNG).
- `Editor::ViewCapture`: the frame after a request is drawn without the ImGui overlay, the game
  cursor and the cameras' editor-only debug text, read back with the renderer's
  `RequestFramePixels`, retried when the GPU skipped the frame. `Editor::Camera::ActivateFreeFly`
  (shared with `OfflineWorld::ResetCamera`).
- Map Editor: **Assets** tab (`CMapAssetReview`), **Flag for regeneration** dialog
  (`CRegenRequestDialog`), catalog block in the Objects tab, `g_MapEditorHighlightedTypes`
  outline in `ZzzObject.cpp`, live object queries in `Editor::ObjectPlace`. The selection now
  survives switching tabs and is cleared only on a map change.
- `build_editor_catalog.py` folds `client-review.json` into `client_verified` and
  `client_review` (strict: unknown model, verdict or field fails); `catalog.json` regenerated;
  README documents the file and the editor's behaviour; MAP_EDITOR.md "Assets tab"; plan status.

**Verified:**
- Editor and player builds; `ctest` 257/257 (236 + 21 new `editor_asset_request_tests`).
- `--world 1` with a temporary hook (deleted; grep finds nothing): Tree01 selected, 80 live
  instances, highlight on, stepped to instance 2 (camera moved), a repaint request with Tree02 as
  partner written with a 1920x1080 capture (no editor, cursor or debug text in it),
  `validate_request.py` OK, `obj_index` 15 confirmed against the decrypted `EncTerrain1.obj`;
  verdict written and folded by the catalog builder (`client_verified` true, request listed).
  Highlight outlines checked on fences. Test folder, `client-review.json` and `imgui.ini` table
  settings removed; `catalog.json` equals the generator output without them; `src/bin` clean.

**Open / next:**
- Trees hide most of their outline under their leaves (the outline is an inflated copy of the
  mesh). **Open folder** and the preview buttons call `SDL_OpenURL(file://...)` and were not
  clicked in the scripted run. Windows not built. Next: M5 (A/B compare).

## 2026-09-23 - World editor M5: A/B compare of current and original assets (Claude Opus 5.5)
**Goal:** Milestone M5 of [`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md): switch Lorencia's models
between their current and original files in the running client, and reload a model from disk.

**Done:**
- `tools/world_editor/materialize_variant.py original [--world N]` (stdlib): builds
  `out/ab/original/Data/Object{N}/` and `manifest.json` from git using each catalog model's
  `original.revision`/`sha256` (archive as fallback) and the textures the original BMD names
  (decrypted in Python), cross-checked against `coordination/texture-baseline`; deterministic.
- `Editor::Assets` `ModelPreflight` (BMD header/version, limits, triangle and bone indices, one
  texture slot per mesh, texture files, `.OZJ`/`.OZT` headers, 1024 px, path length) and
  `AssetVariant` (variant folders, command, manifest vs catalog SHA-256), unit-tested.
- `Editor::Assets::HotReload` (`MuEditor/Core/ModelHotReload`): queued reloads run at the start of
  a frame, preflight, `BMD::Open2` re-open with the world code's action speeds/stream mesh kept,
  textures into the model's old bitmap indices via the new editor-only
  `CGlobalBitmap::ReloadImage` (keeps the other references, keeps the old image on failure),
  placed objects clamped to the new actions, the type's thumbnail dropped.
- Assets tab (`CMapAssetVariants`): **All models: Current / Original**, per model **Show: Current /
  Original** and **Reload from disk**, a **Shows** column, the build command with Copy / Check
  again when `out/ab/original` is missing or stale. MAP_EDITOR.md section, plan status, HANDOFF.

**Verified:**
- Editor and player builds; `ctest` 269/269 (257 + 12 new `editor_model_preflight_tests`, which
  also run every shipped Object1 BMD and texture through the preflight).
- `materialize_variant.py original`: 115 models, 105 textures (3.8 MB, 4.2 MB on disk), identical
  tree hash on a second run; pilot models and their textures from `9a8b2027`.
- `--world 1` with a temporary hook (deleted; grep and `nm` find nothing), four runs, 60-130 s
  each: all 112 typed models to original and back (0 refused), Tree01 alone back to current
  (Tree02 then `mixed`), House01 reloaded from disk, a broken BMD and a texture-less copy refused
  with messages, a map reload while everything was original. Bitmap count and texture memory
  returned exactly to the start values (2108 / 169191176) each time; shared-texture reference
  counts unchanged. Clean captures: town 21 % of pixels changed current vs original, 3-5 % between
  two captures without a switch; tree view 17 %. Missing-folder hint shown, **Check again** after
  restoring the folder enabled **Original**. Also under `MTL_DEBUG_LAYER=1`. `src/bin` clean,
  `imgui.ini` restored.

**Open / next:**
- Windows not built. A texture shared by models switched to different sides shows the side of
  the model switched last (the UI says `mixed`). Not clicked by hand: the buttons themselves (the
  hook called the same functions). Next: M6.

## 2026-09-23 - World editor workflow: Metal overlay, M3-M5 and their review (Claude (world editor workflow))
**Goal:** Take the world editor from M2 to M5 on `feat/world-editor` (plan in
[`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md)), then review the result and fix what the review
found.

**Done:**
- Metal overlay, M3, M4 and M5: see the four entries above (ImGui pipeline for the main pass's
  depth buffer; saves into the repo with backups, safe object delete/undo, terrain normals,
  hardened `.obj` save, ground drag; Assets tab and regeneration requests; A/B compare).
- Review fixes:
  - A map unload (another map, the same map again, the panel closed meanwhile, or the Target
    world override set) now drops the Map Editor's selection and every undo snapshot. Key:
    `ObjectListGeneration()`, which `DeleteAllObjects` bumps (editor-only, `ZzzObject`); the
    editor's own object undo re-syncs it, so the other tabs keep their undo. Before, the old
    selection could be read, moved or deleted after its object was freed, and Cmd+Z could write
    one map's objects, heights or walls into another.
  - Requests: an id that collides keeps its `-2`/`-3` within 80 characters (a single over-long
    word is cut); the dialog warns for the BMD of every target, not only the clicked one; text is
    trimmed like Python's `strip()` (no-break and other Unicode spaces); "pushed" means a branch
    on `origin`, not `upstream`; `obj_index` is set only when `EncTerrain1.obj` equals the blob in
    git's index (new `Editor::Git::IsFileUnchanged`, index versions 2/3, and
    `Editor::Files::GitBlobIdHex`), because an editor save reorders Lorencia's records.
  - A/B: a model the map load read shows `as built` (the build's `Data` copy), not `current`;
    MAP_EDITOR.md and `requests/README.md` say to press **Current** before a verdict on pulled or
    delivered files. The materialize command names the script by its full path (`py -3` on
    Windows) and Copy command copies exactly that.
  - Windows: messages build paths from UTF-8 (`PathToUtf8`) instead of `generic_string()`, which
    throws on MSVC for characters outside the ANSI code page; `MU_EDITOR_REPO_ROOT` is read with
    `SDL_getenv`. The duplicated path, case, join and warning-colour helpers are shared
    (`Assets/EditorText.*`, `Editor::StatusLine::WarningColor`).
  - Docs: MAP_EDITOR.md (overlay pipeline row and gotcha 15 for imgui updates, record order,
    map-unload behaviour, `as built`, command, origin), plan status, HANDOFF item 2. `__pycache__/`
    added to `.gitignore` and the stray `tools/world_editor/__pycache__` removed.

**Verified:**
- Editor build and player build (all targets) exit 0, no new warnings in changed files; the
  player `Main` has none of the new symbols. `ctest` 277/277 (269 + 8 new: long id collisions,
  origin-only push check, git index and blob id, worktree index, `Editor::Text`). The index reader
  also matched `git status` on this checkout (clean and modified files).
- `--world 1` under `MTL_DEBUG_LAYER=1` with a temporary hook (deleted; files byte-identical to
  their backups, grep and `nm` find nothing): after a same-map reload with the panel closed, the
  freed selection's address already held a new object (the old world-number check would have kept
  it); on reopening, selection and all undo steps were empty, and Delete/Undo did nothing. The
  same with the Target world override set. The editor's own object undo kept the texture, height
  and attribute undo. Dialog: `obj_index` 4 on the clean file, left out (with a note) after an
  objects save; a partner BMD mismatch (faked in memory) is warned about. Assets tab shows
  `as built` and the full-path command. The saved `EncTerrain1.obj` was restored with
  `git checkout`, copied back into the game's `Data`, `out/editor-backups` removed.
- Final run of the hook-free build, `--world 1`, `MTL_DEBUG_LAYER=1`: about 60 s, no validation
  assertion, frame 300 shows Lorencia with the full Map Editor. `src/bin` clean, `out/ab/original`
  unchanged, runtime `imgui.ini` and `config.ini` equal to before.

**Open / next:**
- Not changed: saving objects writes records in grid order, so Lorencia's first save still
  renumbers most records (documented in MAP_EDITOR.md; a stable order needs a per-object file
  index, a candidate for M6). The dialog warns but does not block Create when a target's BMD
  differs from the catalog (the catalog may just be out of date); such a request fails
  `validate_request.py` when the BMD is not committed.
- Windows and Linux were not built. Next: M6 (editing comfort).

## 2026-09-23 - World editor M6: editing comfort (Claude (world editor workflow))
**Goal:** M6 of [`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md): multi-level undo/redo, stable
object save order, multi-select, Outliner, transform gizmo.

**Done:**
- `MuEditor/Editing/` (no ImGui, no engine, unit-tested): `CommandStack` (64 MB limit, oldest
  dropped), object steps by key (`ObjectEditCommand`, `ObjectWorld`), terrain steps that keep the
  changed rectangle (`TerrainStroke`, `TerrainPatchCommand`), `ObjectSelection`, gizmo math
  (`GizmoMath`: projection and rays as `CameraProjection` does them, axis/plane drags, turning
  `Angle[]` around a world axis as `AngleMatrix` reads it; `ObjectTransform`: group move, turn and
  scale around the centre, snapping).
- Map Editor: one Undo/Redo bar for all editing tabs (`CMapEditHistory`, adapters
  `CMapObjectWorld`, `CMapTerrainLayers`), replacing the per-tab snapshots and `RestoreAll`;
  Select & edit moved into `CMapObjectEditor` (selection, group ground drag, fields as deltas, Drop
  to ground, Duplicate, Delete, Esc); `CTransformGizmo` (W/E/R, snap, one step per drag, keys kept
  from the game while used); `CMapOutliner` window; shared keys in `Editor::Shortcuts`;
  `Editor::Camera::FocusOn` and `Editor::Text::ContainsIgnoringCase` shared with the Assets tab.
- Engine, editor build only: `OBJECT::SaveOrder` (record index from the loader; `SaveObjects`
  sorts by it through the new `WorldObjectFile::InSaveOrder`), `g_MapEditorSelectedObjects`
  (orange outline for every selected object besides the yellow primary). Player build: the save
  goes through `InSaveOrder` with no orders, the same block order as before.
- Docs: MAP_EDITOR.md (keys, files, Objects, Transform gizmo, Outliner, Undo and redo, recipe,
  free-fly close-up culling limit), plan status, HANDOFF item 2.

**Verified:**
- Editor and player builds exit 0, no new warnings in changed files; `ctest` all green (new:
  command stack, object and terrain steps, selection, gizmo math against `AngleMatrix`, stable
  save order on `EncTerrain1.obj` and every shipped `.obj`, `ContainsIgnoringCase`).
- `--world 1` under `MTL_DEBUG_LAYER=1` with a temporary hook (deleted; grep finds nothing, the
  touched files match their pre-hook copies except for later deliberate edits): every check passed
  (see the plan's M6 status), including a move across an object-grid block (re-created object
  keeps its record index; the save changes only that record; undo, save: equal to `HEAD`). Screenshots of the gizmo in each mode on a Lorencia cannon and on a
  group of three, mid-drag with its label, the duplicates and the Outliner. `src/bin` and the
  game's `Data` unchanged (`EncTerrain1.obj` equals `HEAD`), no `out/editor-backups`, runtime
  `imgui.ini` restored.

**Open / next:**
- The free-fly camera culls objects near the bottom of a close view (under about 1000 units);
  existing M2 culling, documented, not changed.
- Not clicked by hand: the buttons (the hook drove the same functions and injected mouse and key
  events). Windows and Linux were not built. Next: M7 (terrain comfort).

## 2026-09-23 - World editor M7: terrain comfort (Claude (world editor workflow))
**Goal:** M7 of [`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md): usable editor lines, round
brushes, partial relighting, objects that follow sculpted ground, a Light tab that saves
`TerrainLight.OZJ`.

**Done:**
- Renderer: `RenderScreenLines` (camera-facing, pixel width, untextured; `Render::Lines`), GL shim
  line strips/loops and `glLineWidth` (`LineTopology.h`); overlays set state through the wrappers
  (`TerrainOverlayState`) instead of `glPushAttrib`; brush outline on the ground
  (`Render::Terrain::BrushOutline`). `RenderLines` and its non-editor callers unchanged.
- `MuEditor/Editing`: `TerrainBrush` (circle, smoothstep falloff, clipped footprint),
  `FieldBrush` (add, move towards, 5-point smooth, clamp for heights and light), `SurfaceBrush`
  (overlay paint/fade, hard cells), `EditCommandGroup`. `MuEditor/Assets/TerrainLightFile`
  (`.OZJ` encode/decode as the loader reads it).
- Map Editor: `CMapHeightTool` (four tools, Objects follow terrain via `CMapGroundFollowers`),
  `CMapLightTool` + `Editor::LightSave` (new Light tab), `Editor::BrushControls` (sliders, [ ]
  keys, outline); Texture layer 2 and Attribute use round brushes; every brush clips at the map's
  edges. `CMapTerrainLayers` gained the light layer and rectangle relighting.
- Engine (player build too, no behaviour change): `CreateTerrainNormal_Rect`,
  `CreateTerrainLight_Rect`, `CreateTerrainLight` sharing its per-cell code.
- Docs: MAP_EDITOR.md (Round brushes, Texture, Height, Attribute, Light, formats, gotcha 16,
  recipe), plan status, HANDOFF item 2.

**Verified:**
- Editor and player builds exit 0, no new warnings in changed files; `ctest` 328/328 (23 new:
  screen lines and line modes, brush math and edge clipping, overlay paint, command groups, light
  map file round trips and the shipped Lorencia file, rectangle vs whole-map normals and light).
- `--world 1` under `MTL_DEBUG_LAYER=1` with a temporary hook (deleted; grep and `nm` find
  nothing, the touched files equal their pre-hook copies): every check passed (see the plan's M7
  status), including save + map reload of the light map bit for bit. Screenshots of the height,
  overlay, square, attribute and light brushes from 45 degrees and straight down. `src/bin`,
  the game's `Data` and the runtime `imgui.ini`/`config.ini` restored and equal to before; no
  `out/editor-backups`.

**Open / next:**
- The brush acts once per frame (strength per frame, not per second), as the legacy editor did.
- On a rising hill the ground point under a still cursor creeps towards the camera, so a long
  raise moves a little (documented).
- Battle Castle and Crywolf load `TerrainLight1/2.OZJ` in some states; the Light tab always saves
  `TerrainLight.OZJ` (documented).
- Not clicked by hand (the hook drove the same functions and injected mouse and key events).
  Windows and Linux were not built.

## 2026-09-23 - World editor: M6/M7 review fixes and owner quick start (Claude (world editor workflow))
**Goal:** Fix what the review of M6 (editing comfort) and M7 (terrain comfort) found, and give the
owner a short quick start at the top of [`MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md).

**Done:**
- A click that closed a popup (Outliner row menu or type list, Light colour picker, combos) also
  selected, placed or painted under the window it landed on. `Editor::Editing::PopupMouseGuard`
  (unit-tested) keeps the mouse with the editor while a popup is open and until that button is
  released; Esc closes open menus and lists and no longer also clears the selection.
- The tile grid and the Attribute overlay were cut off at 4096 quads per `RenderQuad3D` call (a
  corner of the view; a warning every frame). Both submit in runs of 4096 now
  (`Render::Topology::ForEachQuadBatch`, unit-tested); `RenderQuad3D` itself is unchanged.
- An Outliner or Assets-tab pick switches the Objects tab to Select & edit.
- `OBJECT::SaveOrder` in every build (one `OBJECT` layout; only the editor sets it).
- Cleanups: shared `Editor::ObjectPlace::ForEachLiveObject` (replaces four grid loops, including the
  ground followers' copy of `CreateObject`'s block numbering), `Transform::Describe` (tested),
  `RenderAttributeTab` and `CMuEditorCore::Render` split; docs corrected.
- Docs: quick start in MAP_EDITOR.md (build, open offline, fly, objects and gizmo keys, undo, ground
  tools, save and commit, Assets review, A/B, regeneration requests, known limits), popup and
  batching notes; plan status; HANDOFF item 2.

**Verified:**
- Editor and player builds exit 0, no new warnings in changed files; `ctest` 335/335 (7 new).
- `--world 1` under `MTL_DEBUG_LAYER=1` with scripted SDL input from a driver library in the
  scratchpad (no hook in the tree): the review's failing cases (row-menu dismissal over the Outliner
  in Place new and Select & edit, colour-picker dismissal over the panel and on the 3D view) change
  nothing now, Esc closes the menu and keeps the selection, a later click still paints; tile grid and
  overlay cover the whole view and the whole map top-down with no `clamping draw` warning. Final run
  without the driver: no validation error. `src/bin`, the game's `Data` and the runtime ini files are
  unchanged; no `out/editor-backups`.

**Open / next:**
- `RenderQuad3D` still clamps other callers above 4096 quads; the player build's logs, including a
  full game session, show no such warning.
- Windows and Linux were not built; the buttons were not clicked by hand.

## 2026-09-23 - World editor published as PR #15 (Claude Opus 5.5)
**Goal:** Commit the world editor (M1-M7), bring the branch up to date and open the PR.

**Done:** Split `feat/world-editor` into eight commits (presets, engine/renderer, editor units,
editor UI, A/B tool, coordination state, catalog and request contract, agent docs). Merged
`origin/main` (upstream sync PR #14); the only conflict, `OpenObjectsEnc`, keeps the
`WorldObjectFile` decoder. Deleted the stale hand-configured `out/build/macos-arm64-editor`.
Opened [PR #15](https://github.com/vaskodagamo/MuMain/pull/15) against `main`.

**Verified:** after the merge, the `macos-arm64-mueditor` and `macos-arm64` builds pass,
339/339 tests pass, and `./Main --editor --world 1` opens Lorencia under `MTL_DEBUG_LAYER=1`
with no Metal assertion (screenshot checked).

**Open / next:** owner review of PR #15; try the file dialog and the Open folder / preview
buttons by hand; Windows/Linux builds untested.

## 2026-09-23 - Object48 integrated as a focused review checkpoint (ASTRA / Codex)
**Goal:** Publish independently accepted static-asset work in small, mergeable checkpoints while continuing the remake.
**Done:** Refreshed an isolated worktree from owner-fork main after PR #12 merged. Integrated the reviewed Object48 BMD and recorded export, placement, texture and converter evidence. Updated current assessment: Lorencia remains artistically unfinished despite first-pass 22-replacement and 84-retention counts.
**Verified:** The integrated BMD matches the reviewed export hash; the official converter validates its static and animation SMDs; the frozen texture container passes; all 63 Object2 models resolve their texture dependencies. Exactly one Object2 game path changes in this branch. Client and runtime verification remain pending.
**Open / next:** Opened [PR #16](https://github.com/vaskodagamo/MuMain/pull/16), ready for review and mergeable against main. Resume production with a Lorencia quality batch from refreshed main.

## 2026-09-23 - Item editor I0: shared request/save/reload code (Claude Opus 5.5)
**Goal:** Milestone I0 of `ITEM_EDITOR_PLAN.md`: make the Map Editor's request, repo-mirror and
hot-reload code usable by a second editor without changing the Map Editor.

**Done:** `RequestDomain` for request folders, branches, schema and protected paths
(`WorldRequestDomain` for maps); `Core/RepoMirror` and `Core/EditorFiles` moved out of
`UI/MapEditor`; `HotReload::ModelRange`/`AllowRange`. Details in the plan's Status list.

**Verified:** golden dump of `request.json` + `brief.md` byte-identical before/after; 340/340
tests (`macos-arm64-mueditor`), editor tests in `macos-arm64`; offline Lorencia run with a frame
capture.

**Open / next:** I1 (Item Editor on the Mac + `--items`) and I2 (item catalog tools) can start in
parallel. Reload of a non-world range is untested until I6 allows one.

## 2026-09-23 — Dungeon inventory review checkpoints

**Goal:** Continue the environment remake after the user merged PR #12, and publish bounded, reviewable updates against the owner-fork main.

**Done:** Confirmed PRs #13, #16 and #17 are merged. Opened retention checkpoints #18–#20 and #22; the user has since merged PRs #18–#20. PR #21, an unrelated editor-docs change, also merged. PR #23 now records the rejected Object06/13/15 studies after independent review. Updated the current assessment and Dungeon handoff to distinguish merged replacements, retained baselines, active inventory and offline-only evidence.

**Verified:** Pulled owner-fork main at `1256aeed` after those merges. Object45–47 source BMDs, shared `bons.OZJ`, packed Blender scenes and rendered previews are hash-bound; six exact Object47/Object48 cluster records match the main placement manifest. PR #20 independently passed 181 evidence-hash checks and all 120 placement records; its protected raw fire meshes and anchors remain unchanged. PR #23 independent review checked 218 evidence hashes and 446 placements. Lorencia's final offline gate remains `bb641d04`: 22 replacements and 84 retentions cover all 106 in-scope assets. New client verification remains pending.

**Open / next:** Owner review of PRs #22 and #23; continue the Dungeon static inventory with actual-placement context. The latest triage found no coherent next 3–5 asset production batch among Object05/07/08/09/10/11/14; keep Object09's animated Object12 clearance dependency explicit.

## 2026-09-23 - Item editor I1: Item Editor on the Mac, offline --items (Claude Opus 5.5)
**Goal:** Milestone I1 of `ITEM_EDITOR_PLAN.md`.

**Done:** portable Item Editor paths, saves mirrored into the repository with a backup,
`--editor --items`, byte-preserving save of the item table in the layout it was loaded from,
29-byte name limit for the legacy layout, console `std::cout` hang fixed, `ITEM_EDITOR.md`.

**Verified:** `editor_item_table_tests` (four shipped tables byte-identical, rename changes one
record + checksum); 343/343 editor-build tests, 342/342 player-build tests; scripted in-client
run (search, columns, rename, save, no-change save, copy, exports), screenshots.

**Open / next:** hand test by the owner; I3 browse tab. Item stats of 13-121/125/127 are
corrupted in the shipped file (long names spill into later fields).
## 2026-09-23 — Merged review checkpoints and portable Lorencia evidence (ASTRA / Codex)

**Goal:** Continue the environment remake after the user merged PR #12, keeping owner-fork checkpoints current and reviewable.

**Done:** Pulled owner-fork main into the isolated integration worktree after the user's merge; main is now `0cc611bc`. PRs #12, #13, and #16–#24 are merged. Preserved portable, hash-bound Blender-imported contexts for the HouseEtc stack, south gate, and siege wall after identifying stale workstation-only image links. Independent challenge review still supports the existing Lorencia retention decisions; the 22 accepted replacements plus 84 justified retentions represent coverage of 106 assets, not 106 remakes.

**Verified:** The 52 files in the portable masonry evidence package match its SHA-256 manifest. The independent review revalidated 29 model/texture appearances, all 17 selected placement transforms and model types, and exact old/current composition bounds. It found no additional placed-scale defect or production batch. This is selective offline evidence; newly changed game assets still need client verification.

**Open / next:** Object44 has one bounded skeleton-silhouette prototype in progress on `codex/dungeon-remains44`, with `bons.OZJ`, `wood01.OZJ`, and neighboring models frozen. Independently review actual reduced/normal placement views before accepting it. Continue the Dungeon inventory and publish focused, ready-for-review PRs against `vaskodagamo/MuMain` as checkpoints pass.

## 2026-09-23 - Item editor I2: item catalog, tiers, request contract (Claude Opus 5.5)
**Goal:** Milestone I2 of `ITEM_EDITOR_PLAN.md`.

**Done:** `tools/item_editor/` (item table decoder, model table generator, BMD facts, tiers,
OpenMU export, catalog builder), `assets-work/Items/` (catalog, OpenMU export, tiers/assignments
files, request README/schema/validator).

**Verified:** 56 Python tests, registered in ctest; catalog `--check`; OpenMU export from the
local database (read-only, no credentials).

**Open / next:** I3 browse tab reads the catalog; I5 writes requests in this contract. Engine
check for `ItemSetType` "no set" = 0 vs `0xFF`.

## 2026-09-23 - Item art baseline study (Codex)
**Goal:** Inventory the item and player armor BMD families, create comparable offline "before" renders, and record a style/rework baseline without modifying game assets.

**Done:** Added `assets-work/Items/study/baseline.json`, README, collection/UV/finalization scripts and 13 fixed-camera previews for seven gear families, three wing generations and three five-part armor sets. The inventory covers 207 Item-folder models and 463 Player armor-part models, with bmdconv structure, texture sizes/sharing and a model-level UV review screen. Recorded family scores, 20 model/set rework targets, a tier palette and geometry/texture budgets, and risks for shared textures, origins, mesh order and armor compatibility. Updated the Blender importer to skip action-manifest parsing when `--no-anims` is requested so legacy non-UTF-8 manifests do not block static imports.

**Verified:** `bmdconv info` completed for 670/670 scoped models; 0 unresolved texture references; 131 shared texture files recorded. UV conversion completed for all 670 models. Blender imported the 25 representative BMD parts and rendered all 13 previews at 1024×1024, orthographic scale 360, model scale 1.0. Checked representative PNG output visually. No files under `src/` changed.

**Open / next:** Offline baseline only; review candidates and palettes with the owner, and use the running client to verify pivots, equipped placement, alpha and glow before accepting any future item rework. PR opened against `main` on `vaskodagamo/MuMain`; not merged.

## 2026-09-23 - Item editor I3: Browse tab, studio mode, launcher (Claude Opus 5.5)
**Goal:** Milestone I3 of `ITEM_EDITOR_PLAN.md`, with the owner's studio mode and launcher.

**Done:** item studio for `--editor --items`, `MU Item Editor.app`, Browse tab (filters, tier
sort, list/grid thumbnails, details), shared class rule `CanClassEquip`, Unicode search.

**Verified:** 366/366 editor-build and 365/365 player-build tests; scripted in-client run with
screenshots (studio, DK swords by tier, grid, sync, launcher, unchanged `--world 1`), Metal
validation clean, ~75 fps with 959 items.

**Open / next:** hand test by the owner; I4 preview (level/excellent/ancient, equipped).

## 2026-09-23 - Item style pilot: Axe01, Shield01 and Wing01 (Codex)
**Goal:** Prepare faithful A and bolder B offline style variants for three item attachment types so the owner can choose an art direction before mass rework.

**Done:** Created six editable Blender sources, exported BMDs and matching 256×256 game texture containers, before/A/B study-camera renders, per-item comparison sheets and an owner review README under `assets-work/Items/pilot/`. Preserved the original model paths, one-mesh layout, attachment bounds/transforms, bone order and action key counts. Pulled and fast-forwarded to `origin/main` at `6f93a708` before finalizing the pilot.

**Verified:** `bmdconv validate` passed for all six mesh and action exports; `bmdconv compare` ran against each original and confirmed matching mesh/skeleton/action structure and bone motion (geometry differences are intentional); all six texture containers passed `mu_texture.py check`. All variants are below 1500 triangles and use 256×256 maps. Nothing was installed under `src/bin/Data`; no client check was performed.

**Open / next:** Owner review and per-item A/B selection. Route chosen assets through the item editor request flow, then verify equipped placement and in-client materials before acceptance.

## 2026-09-23 - Item editor I4: live 3D preview (Claude Opus 5.5)
**Goal:** Milestone I4 of `ITEM_EDITOR_PLAN.md`.

**Done:** turntable, inventory, ground and equipped views with +level, excellent and ancient,
drawn by the game's own code on a preview character of its own; `RenderDroppedItem` /
`PlaceItemOnGround` shared with the game (same behaviour).

**Verified:** 382/382 editor-build and 381/381 player-build tests; scripted in-client run with
screenshots of every acceptance view; Metal validation clean; ~75 fps.

**Open / next:** owner hand test; compare the inventory slot scale and sword stance with the game;
I5 needs a texture readback for clean captures.

## 2026-09-23 - Item concept image tool (Claude Opus 5.5)
**Goal:** Owner's idea: generate 2-3 concept variants for 10-20 items in parallel through the
OpenAI Images API, pick the best, and hand the pick to Codex for Blender modeling.

**Done:** `tools/item_editor/concepts.py` (plan, refs, run, sheet, pick) with presets
explore (gpt-image-2.5-flare medium) and final (gpt-image-2.5-sunburst high), cost caps and key
safety; docs in `assets-work/Items/concepts/README.md`. Model choice researched from OpenAI's
docs and the owner's pricing page.

**Verified:** 82 Python tests against a mock server; reference renders checked by eye.

**Open / next:** the owner sets `OPENAI_API_KEY` and a project budget limit, then a first real
explore batch (study top 10 x 3); compare estimated and actual `usage`.

## 2026-09-23 - Item concepts backend for the editor (I5b part 1) (Claude Opus 5.5)
**Goal:** Let the Item Editor drive `concepts.py`: the owner asked for concepts inside the editor
(select items, generate, pick or refine with a comment, hand to Codex).

**Done:** key from `$OPENAI_API_KEY` or the macOS Keychain item `openai-api-key` (an app started
from Finder does not read `~/.zshrc`); Python 3.9 support (Apple's `/usr/bin/python3`); paths
from the repository root; a versioned JSON protocol (`--json` one-shot, `--json-progress` JSON
Lines, exit codes incl. 5 no key, 6 busy, 130 cancelled); SIGTERM cancellation with resume;
batch locks; `list`/`discard`/`undiscard`; refine from a variant with a comment
(`run --from <batch>/<key>/vN --note ...`, editable `## refine` prompt). Protocol in
`assets-work/Items/concepts/README.md`.

**Verified:** 113 tests on Python 3.9.6 and 3.12.6 (mock server, injected Keychain runner,
subprocess cancellation); a real `plan --json` found the Keychain key without printing it.

**Open / next:** I5b editor panel on top of I5 (PR #33) and this branch.
## 2026-09-23 - Item editor I5: Ask Codex, captures, Requests tab (Claude Opus 5.5)
**Goal:** Milestone I5 of `ITEM_EDITOR_PLAN.md`.

**Done:** editor-only texture read-back and scripted clean captures, the Ask Codex dialog writing
validated `mu-item-regen-request/1` folders with the picked concept, verdicts, the Requests tab
with withdraw / accept / reject / re-file, live request status; the catalog check ignores request
status.

**Verified:** 395/395 editor-build, 394/394 player-build, 87 tools tests; scripted in-client run
of every acceptance point; Metal validation clean.

**Open / next:** owner files the first real request (I7 pilot); I5b concepts inside the editor.

## 2026-09-23 - Item editor I5b: concepts in the editor (Claude Opus 5.5)
**Goal:** The owner's workflow inside the Item Editor: select items, generate concept images,
pick or refine with a comment, hand the pick to Codex; plus pointer, full screen and UI size.

**Done:** multi-select, Generate dialog with the real estimate, background job with cancel and
resume, Concepts section (pick, refine, discard), concept in Ask Codex, visible pointer in the
studio, full screen, remembered UI scale, front captures show the broad face.

**Verified:** 408/408 and 407/407 tests, 114 tools tests; scripted in-client run against a local
fake API (no spending) with the owner's real concept batch copied in.

**Open / next:** the owner's first real generate from the editor; I6 A/B compare; I7 pilot.

## 2026-09-23 - Item editor I6: A/B compare (Claude Opus 5.5)
**Goal:** Milestone I6 of `ITEM_EDITOR_PLAN.md`: see original, current and candidate item models
side by side in the running client before accepting.

**Done:** `materialize_variant.py original --items`; item hot reload with fixed-slot reuse;
candidates from deliveries and the style pilot; side-by-side preview with a linked camera;
family/all switching; Compare in the Requests tab; A/B capture sheets in `out/item-ab/`.

**Verified:** 416/416 and 415/415 tests plus Python tests; scripted in-client run incl. the pilot
A/B for Axe01, Shield01, Wing01 and a texture/memory round trip back to the start values.

**Open / next:** owner picks the pilot direction; wing textures must be painted for blended
drawing; I7 pilot through the full request flow.
## 2026-09-23 - Map Editor: visible pointer over every panel (Claude Opus 5.5)
**Goal:** The pointer vanished over parts of the Map Editor on macOS (images, child regions, gaps
between panels): the game cursor is drawn under ImGui, and the OS pointer was only shown where a
window set `SetHoveringUI`.

**Done:** `CMuEditorCore::UpdateCursors()` shows the OS pointer whenever ImGui has the mouse
(`WantCaptureMouse`, any hovered window) or a window claimed it, and forces it through the
`ShowCursor` display counter on every platform; over the world the game cursor, with ImGui's
backend kept from re-showing the OS pointer (`NoMouseCursorChange`). Replaces I5b's studio-only
rule (PR #35), which is now one condition in `IsMouseOverEditorUI()`.

**Verified:** 408/408 ctest (editor build, after merging main with I5b); `./Main --editor --world 1`
with posted mouse moves and full-screen captures (`screencapture -R` leaves the pointer out, `-m`
does not): game cursor only over the world, OS arrow over palette images, gaps between tiles,
panel text, toolbar and console gap, text beam over console text.

**Open / next:** Windows build not compiled here; the item studio (`--items`) not re-run by eye
after the merge (the owner was using the mouse).

## 2026-09-23 - M8: eyes for an AI agent (Claude Opus 5.5, branch feat/ai-map-editing)
**Goal:** Let an agent see a map through the control socket on the Mac editor build, keep the client
answering while its window is hidden, and fix the terrain loaders' memory-safety bugs.

**Done:**
- `macos-arm64-mueditor` turns `ENABLE_CONTROL_SOCKET` on; the transport works on macOS
  (`SO_NOSIGPIPE`, 256 KiB send buffer on accepted sockets; the socket tests give their client a
  wider buffer on macOS, where a blocking 16 KiB write into the default 8 KiB deadlocked them).
- Hidden window: no stall found on macOS; while the socket serves, frames of a minimized, hidden or
  covered window are drawn offscreen (`Render::HiddenWindow`), so captures keep working.
- New commands (editor builds): `map-open`, `map-info`, `map-camera` (tile / top-down rectangle),
  `map-export` (layer PNGs, `legend.json`, `objects.json`), `map-query`, and `screenshot` with
  `clean`, `region` and PNG output. Pure logic in `src/MuEditor/MapInspect/` (unit-tested),
  engine adapter `Editor::LiveMap`, thin handlers in `App/Control/ControlCommandsMap*.cpp`.
- Player build: bounds checks in `OpenTerrainMapping`, `OpenTerrainHeightNew`, `OpenJpegBuffer`
  (logged refusal instead of overrun) and the row-255 corner reads of `RenderTerrainTile(_After)`.
- Docs: `docs/control-socket.md` (macOS, "Editor commands", hidden-window note), `MAP_EDITOR.md`,
  plan status M8.

**Verified:** editor and player builds exit 0, no new warnings in touched files; `ctest` player
358/358, editor build 386/386 (socket tests included). Release editor client on World1/World3
driven from Python: every command and its argument errors, clean/overlay shots, a top-down shot of
Lorencia cropped to the map, the layer PNGs (safe zone, walls, moat, river; north up as in the
top-down shot), map switches, and all of it again while the window was minimized, hidden (Cmd+H)
and covered by a floating window (log: offscreen on/off at each change). `src/bin` unchanged.

**Open / next:** M9 (undoable edits over the socket), M10 (new maps and gates). Windows and Linux not
built; the Linux compositor stall is only covered where SDL reports the window as occluded.

## 2026-09-23 - M9: hands for an AI agent (Claude Opus 5.5, branch feat/ai-map-editing)
**Goal:** Let an agent make high-level, undoable map edits through the control socket, dry-run
them, save them like the Map Editor does, and throw them away again.

**Done:**
- Edit scripts (`mu-map-edit/1`) for terrain, textures, walkability, light and objects (place,
  seeded Poisson-disk scatter with avoid rules, move/rotate/scale/delete/drop to ground) over
  circles, rectangles, polygons and paths; parsed, checked and run on plain arrays in
  `src/MuEditor/MapScript/` (unit-tested), applied to the live map as one undo step by
  `Editor::LiveMapEdit`.
- Socket commands `map-apply` (script or path, dry run), `map-undo`, `map-redo`, `map-history`,
  `map-save`, `map-revert` (`App/Control/ControlCommandsMapEdit.cpp`); saves and reverts through
  `Editor::LiveMapFiles` and the tabs' own saves (now able to report the paths they wrote).
- `Editing/` brushes take masks of any shape (`WeightMask`); `CommandStack` lists its labels; the
  height save moved to `Editor::HeightSave` and refuses 24-bit height maps.
- Docs: new `docs/agents/AI_MAP_EDITING.md`; pointers in `docs/control-socket.md` and `MAP_EDITOR.md`;
  plan status M9.

**Verified:** editor build and player build exit 0 with no new warnings; `ctest` editor preset
416/416, player preset 388/388 (30 new cases). Release editor client on World1 over the socket:
grove script dry run then apply, before/after/undo/redo/revert screenshots from one pose (undo
and revert match the before shot within frame noise), unsaved flags back to false after undo,
map-save of all five files with backups and a reload, every error path. `src/bin` restored from
git, `out/editor-backups` removed.

**Open / next:** M10 (maps 82+ joined by gates). Windows and Linux not built (MSVC reviewed by
reading only). `busy` while a stroke is held was not exercised at run time; edits in a
logged-in session were not tried (offline only).

## 2026-09-23 - M10 part A: new maps and gates (Claude Opus 5.5, branch feat/ai-map-editing)
**Goal:** Let the world grow the MU way: new 256 x 256 maps (82 and up) joined by gates, from the
Map Editor and over the control socket, plus the files OpenMU needs (never applied).

**Done:**
- New maps: `MuEditor/NewMap/` (renumbered template copies, flat maps, Lorencia's named models
  under numbered names, writing with the repository copy; refuses existing folders), adapter
  `Editor::NewMapFiles`, the Map Editor's **New map...** window and socket `map-new`.
- Names: `World::MapNames` (player build too, inert for stock data) gives maps 82+ the name in
  `Data/World{N}/MapName.txt`; `--world` and `map-open` accept any existing folder 1 to 255.
- Gates: `MuEditor/Gates/` (Gate.bmd byte-exact reader/writer, edits of gates 345+, checks),
  adapter `Editor::LiveGates`, the **Gates** tab (list, ground rectangles over the walkability
  overlay, draw, add, move, remove, look at), socket `gate-list`, `gate-add`, `gate-remove`,
  `gate-show`. Gate 344 turned out to be a Karutan 2 spawn record, so custom gates start at 345.
- OpenMU export: `MuEditor/ServerExport/` + `Editor::ServerExportFiles`, Gates tab button and
  `map-server-export` -> `out/openmu-export/map{N}/` (walk map for new maps, map.json, gates.json,
  HOWTO.md, openmu.sql marked not applied).
- Docs: `MAP_EDITOR.md` (New maps, Gates, OpenMU export, formats, files), `AI_MAP_EDITING.md`
  (Growing the world), `docs/control-socket.md`, plan status, handoff.

**Verified:** editor and player builds exit 0, no new warnings; `ctest` editor 441/441, player
413/413 (25 new cases). Socket run on the Release editor client: flat map 82 created and opened,
booted with `--world 83`, a Lorencia copy as map 83 opened with its 2870 objects, gate pair
Lorencia <-> 82 added (only records 345-348 of Gate.bmd changed), shown by `gate-list`, `map-info`
and the Gates tab (`gate-show`), export written and read. Test maps deleted, Gate.bmd restored from
the editor's backup (identical to the main checkout's), backups and export folder removed.

**Open / next:** walking through the gate needs the server (not tried, no login). The Gates tab's
mouse drawing and buttons and the New map window were only seen, not clicked (ImGui takes real
input only). A copy of Lorencia loses Lorencia-only code (effects, blending). Windows and Linux not
built. `openmu.sql` was never run against a database.

## 2026-09-23 - M10 part B: mapctl and the agent guide (Claude Opus 5.5, branch feat/ai-map-editing)
**Goal:** Make map editing easy for AI agents and the owner: a command line over the control
socket, and one page an agent follows when the owner says "expand X with Y".

**Done:**
- `tools/world_editor/mapctl.py` (standard library only, CLI and module): `launch` (finds the
  editor build, sets `MU_CONTROL_SOCKET`, waits for the world scene, prints the PID), `ping`, `info`,
  `open` (refuses while the loaded map has unsaved edits, which `map-open` drops silently;
  `--discard`), `camera`, `shot` (clean PNG, `--topdown` frames and crops), `export`, `query`,
  `apply`/`dry-run` (script files inline, by path above the 256 KiB line limit), `undo`, `redo`,
  `history`, `save`, `revert`, `new-map`, `gates`, `gate-add`, `gate-remove`, `gate-show`,
  `server-export`, `quit` (waits until the client has gone), `send`, and `sketch`
  (`map_sketch.py`: a labelled tile grid and a script's shapes over an export layer or a top-down
  shot). JSON out; exit 0 ok, 1 refused, 2 bad input, 3 no client or no answer.
- Tests: `tools/world_editor/tests/test_mapctl.py`, 47 cases without a client (fake socket server),
  ctest `world_editor_mapctl`.
- `docs/agents/AI_MAP_EDITING.md` rewritten as the entry point: prompt template and defaults, ground
  rules, setup, the loop with sketches and layered applies, MU design rules, growing the world,
  hand-back (review in the editor, or branch + PR after the owner's OK), the server side; the
  reference kept. Pointers in `AGENTS.md`, `HANDOFF.md`, `docs/control-socket.md`, `MAP_EDITOR.md`;
  plan row and status M10 part B.

**Verified:** every mapctl command against the Release editor client (Lorencia and a new map 82,
launch 1.7 s), screenshots looked at (clean, overlay, top-down, side, sketches; the sketched gate
rectangles lie on the engine's gate areas). Saved height file restored from git, test map and gates
removed (`Gate.bmd` byte-identical to before), backups and logs deleted. No server login.

**Open / next:** mapctl on Windows needs a Python with `AF_UNIX` (CPython there has none). The
branch + PR hand-back and the `assets-work/map-edits/` record folder are documented, not yet used.

## 2026-09-23 - M8-M10 review fixes (Claude (AI map editing workflow))
**Goal:** Verify and fix every finding of the M8-M10 reviews and every friction item of the
acceptance run (map 82 "Lorencia Outskirts"), keeping the acceptance deliverables.

**Done:**
- Crash: Korean (CP949) model names ended the client in `map-query`, `map-export` and `map-apply`
  on most game maps (nlohmann's strict UTF-8 check threw with no catch). `ModelName` escapes
  non-UTF-8 names as `%XX` (`Editor::Text::ValidUtf8`); the dispatcher answers `failed` for a
  handler exception; response lines use the replace handler.
- Hidden window: frames drawn offscreen are submitted with a fence, at most two in flight, at most
  60 a second (`Render::HiddenWindow::SubmitPaced`); EndFrame keeps its one direct submit (the
  renderer contract test).
- Gates: `gate.bmd` found in any letter case (`Editor::NewMap::FindIgnoringCase`; tests read the
  tracked spelling); every gate edit, `gate-list` and export first re-read the file
  (`LiveGates::Refresh`); traps (no walkable arrival tile, arrival inside its own enter area) are
  refused unless `allow_trap`; `faces` in gate answers; gate-show and the Gates tab share
  `Editor::Camera::FrameGateArea`.
- OpenMU export: only the gates the editor added; a stock arrival they land on is looked up, never
  inserted; rectangles low corner first; no spawn gates; the map name as hex in `openmu.sql`, which
  also sets `ON_ERROR_STOP`; `gates.json`/`map.json` carry their own `FormatVersion` so OpenMU's
  spawn import refuses them; the HOWTO says so and gives `git add src/bin/Data/gate.bmd`.
- Scripts: `light.bake`, `attribute.set` `under`, a cost budget (`MapScript/ScriptCost`), no FP
  contraction for MapScript (CMake), the unknown-model error lists types. Commands: `map-tab`,
  `map-info` `models`, `pid` in `ping` and `quit`, `Quit requested: ...` in `MuError.log`
  (editor builds), captures retry skipped frames for 3 s.
- PNG writer (vendored stb, editor only): row filters plus deflate with per-block Huffman codes.
- mapctl: private default socket folder (`/tmp/mu-<uid>`, 0700, or `$XDG_RUNTIME_DIR`) and an
  owner check; `launch` refuses a busy socket and another client's pid; `quit` waits for the pid;
  `shot` retries; JSON on usage errors; `info --models`, `gate-add --allow-trap`, `tab`.
- `MapManager.cpp`: the double blank line (CI clang-format gate) removed; the `MapName.txt` lookup
  moved behind `_EDITOR`. Smaller: `MIN_OBJECT_SCALE` and `TileOf` reused instead of copied.
- Docs: `AI_MAP_EDITING.md` (socket, loop saves, design rules 1/10/11/12 with Lorencia's measured
  walkability per model, new-map light and names, gates, git, server export, reference, gotchas),
  `docs/control-socket.md`, `MAP_EDITOR.md`, `WORLD_EDITOR_PLAN.md`, `HANDOFF.md`.
- Deliverables: `out/openmu-export/map82` regenerated with the new format; map 82 and `gate.bmd`
  unchanged (checksums before and after).

**Verified:** editor preset 449/449 and player preset 421/421 tests (new: ValidUtf8, gate traps,
export scope and SQL quoting, light bake, `under`, script cost, PNG size, mapctl 53 cases);
clang-format 21 clean on the changed `src/source` lines. Release editor client over mapctl:
Devias whole-map query (2,237 objects) and objects export with escaped names, unknown-model error,
a dry run placing a model by its escaped name with `under` and `light.bake`, a heavy script refused
in 0.07 s; window hidden on Devias and Lorencia top-downs: footprint flat at 1.2 and 2.85 GB, CPU as
when visible, shots correct; trap refusals and `--allow-trap`; `gate-show` then `tab texture`
(overlay gone in the clean shot); `gate.bmd` swapped behind the client and picked up; exports of map
0 (gates 345-348 only) and 82; `quit` waited for the pid, `Quit requested` logged; end to end:
`launch --world 1`, `info`, `open --map 82`, clean shots of map 82 looked at. PNG round trips
against Python's zlib (noise, gradients, three 1920 x 1080 frames), also under ASan/UBSan. The
player binary has none of `Quit requested`, `map-tab`, `MU_CONTROL_SOCKET`. No server login.

**Open / next:** Windows and Linux not built. The owner decides whether the player build names new
maps (`MapName.txt`). Map 82 would profit from an art pass (focal points in the meadow, a road end,
softer river edges). New maps made with `models_from` still name models by file, not by the source
catalog. The acceptance deliverables are saved, not committed.


## 2026-09-22 - Second Mac (`lukasmac`): build and local OpenMU (Claude Opus 5.5)
**Goal:** Set up a fresh Apple Silicon Mac (macOS 26.1) to build the client and run OpenMU.

**Done:**
- Homebrew: cmake 4.4.3, ninja, pkgconf. .NET 10.0.301 was already in `~/.dotnet`; added
  the PATH block to `~/.zshrc`. Initialized the SDL and imgui submodules.
- Configured `macos-arm64` with two machine workarounds: the stale libc++ folder is present
  here too (used the `-nostdinc++` flag from the macOS guide), and Homebrew `python@3.14`
  3.14.5 has a broken `pyexpat` (`gen_wire_sizes.py` fails), so
  `-DPython3_EXECUTABLE=/usr/bin/python3` is set in the cache.
- OpenMU cloned to `../OpenMU`; `deploy/all-in-one/docker-compose.mumain-local.yml` is
  standalone, localhost-only, admin panel `127.0.0.1:8090`, `RESOLVE_IP=127.0.0.1`. It uses
  project `mumain-openmu` and containers `mumain-openmu` / `mumain-openmu-db` (logs:
  `docker logs mumain-openmu`) because an older `all-in-one` stack from
  `~/Documents/new-mu` already owns the default container names. Port 80 is Herd's; no nginx.
- The runtime `config.ini` in the app bundle is set to `127.0.0.1:44406` (the template
  seeds 44405, which is the original client's port).

**Verified:** Release build OK; `ctest` 213/213 pass. OpenMU initialized its database (test
accounts `test0`..`test9`, `testgm`, ...), game servers advertise `127.0.0.1:5590x`. Client
started under Metal, connected on 44406, received the server list and a game-server connect
response (`MuError.log`).

**Open / next:** The disk ran full mid-build once (228 GB volume, little free space);
Docker Desktop crashed and had to be force-quit. Keep >10 GB free.

## 2026-09-23 - Pull latest MuMain changes (Codex)

**Goal:** Update the local main checkout from origin.

**Done:** Fast-forwarded main from `7b808473` to `60dc4463` and restored both
local documentation edits, retaining both sets of appended worklog entries.

**Verified:** HEAD matches origin/main; local documentation edits are preserved.

**Open / next:** No build or tests run for this repository sync.

## 2026-09-24 - Item Editor on the Mac: item window, zoom, console, concept renders (Claude Opus 5.5)
**Goal:** First owner session with the Item Editor on the second Mac; fix what got in the way.

**Done:**
- Browse opens the selected item in its own resizable window instead of the narrow right
  panel (Side by side doubles its width). The preview picture takes the mouse wheel (the
  panel around it scrolled instead of zooming out) and has - / + zoom buttons.
- The toolbar's Console box (editor and game consoles) is kept in `MuEditor.ini` (`ShowConsole`).
- `concept_refs.find_blender` also finds `../astra-tools/Blender.app` and passes
  `../astra-tools/blender-user/scripts` as `BLENDER_USER_SCRIPTS`, so reference renders work
  where the art agents' Blender lives, without `/Applications` or a Source Tools install.
- Built the `macos-arm64-mueditor` preset (same `-nostdinc++` / system Python workarounds).

**Verified:** Editor build OK; ctest `preview|browse|item` 56/56; concept tool unittests OK.
Reference renders 0-1 and 0-2 ran through the new lookup with the manual links removed. A real
concept run (3 Short Sword variants) succeeded. Window, zoom and console change not yet
screen-checked by the owner.

**Open / next:**
- `security add-generic-password ... -w` without a value cut the pasted OpenAI key to 128
  characters (OpenAI rejected it with a 401). The concepts README should recommend
  `-w "$(pbpaste)"`.
- Opening `MU Item Editor.app` with `open` did not start the client here; running the script
  inside it did. Not investigated.
- The disk was nearly full again (about 1-2 GB free); 16 `MuMain-*` agent worktrees of about 3.3 GB each.
