# Item editor plan

Goal (owner, 2026-09-23): open every item on the Mac and see it exactly as the game draws it
(inventory, on the ground, in the hand/on the back, with +level glow, excellent shine and ancient
effect), filter by character class, sort from basic to rarest, and hand any item to the art
builder (Codex agents following [`ASTRA.md`](../../ASTRA.md)) with one click to improve its
quality, repaint it or redesign it, then compare original and new in the running client.

Branch: `feat/item-editor` (one PR per milestone, see section 7). Usage docs for the finished
tool go into `src/MuEditor/UI/ItemEditor/ITEM_EDITOR.md`. This plan builds on
[`WORLD_EDITOR_PLAN.md`](WORLD_EDITOR_PLAN.md); read its sections 3-4 first.

## 1. Decision: extend the in-client Item Editor

The editor build already has an Item Editor (`src/MuEditor/UI/ItemEditor/`, about 1000 lines):
one table of `ITEM_ATTRIBUTE` rows with in-place editing, a name search, column visibility, and
Save / Export S6E3 / Export CSV of `Data/Local/<lang>/Item_<lang>.bmd`. It has no preview, no
class filter, no sorting, no Codex hand-off, and has never been run on macOS.

Everything else the goal needs was built for the Map Editor and only has to be generalised:
offscreen model capture (`ObjectThumbnail`), view capture (`Core/ViewCapture`), the request
pipeline (`Assets/RegenRequest`, `RequestBrief`, `RequestFolder`, `RequestNaming`,
`GitCheckout`, `FileDigest`, `CaptureImage`, `ClientReview`), runtime model/texture reload
(`Core/ModelHotReload`), original materialisation (`tools/world_editor/materialize_variant.py`)
and the repo mirror with backups (`UI/MapEditor/MapEditorRepoMirror`). Staying in the client is
the only way to see the engine-side effects, which live in code, not in the asset:
`RenderPartObjectEffect` (`ZzzObject.cpp:9575`, chrome tiers and +7..+15 glow at
`:10384-10525`, excellent at `:10557`, ancient at `:10613`) and the per-type inventory placement
in `RenderObjectScreen` (`ZzzInventory.cpp:7653`).

Scope of "change the design": **replace an existing item's look in place** (same group/index,
same file names). Items that are new to the game need a table entry, an `OpenItems()` code
change and an OpenMU definition; they are out of scope until section 8.

## 2. Facts the design rests on (investigation 2026-09-23)

- **Table:** 16 groups x 512 = `MAX_ITEM` 8192 (`_define.h:352-385`, `ITEMINDEX` at `:645`).
  Loaded by `OpenBasicData` -> `ItemDataLoader::Load` (`ItemDataLoader.cpp:21`), per-record XOR
  `BuxConvert` `{0xFC,0xCF,0xAB}` plus a checksum (key `0xE2F1`). Fields in `ItemFieldDefs.h`.
- **Class:** `RequireClass[7]` in the order DW, DK, Elf, MG, DL, SUM, RF; the value is the
  minimum class stage (0 = not usable, 1 = base, 2 = second class, ...). MG may also use items
  allowed for both DW and DK. Engine check: `IsRequireEquipItem` (`ZzzInfomation.cpp:2208-2231`)
  - the filter must call the same rule, not re-implement it.
- **Rarity is per instance, not per type.** An `ITEM` carries +level, excellent flags, ancient
  discriminator, luck, 380 option, sockets (`_struct.h:161-231`). Per type we only know what the
  item *can* be: set-capable (`CSItemOption::m_ItemSetType`), socket item
  (`CSocketItemMgr::IsSocketItem`, hard-coded), 380 option (`ItemAddOption.bmd`), and its base
  drop level `ITEM_ATTRIBUTE::Level`. OpenMU knows more: `DropLevel`, `DropsFromMonsters`,
  `MaximumItemLevel`, `MaximumSockets`, `PossibleItemSetGroups`, `PossibleItemOptions`
  (`OpenMU/src/DataModel/Configuration/Items/ItemDefinition.cs`).
- **Models:** `MODEL_ITEM` (515) + item type (`_enum.h:1451`). Weapons, shields, wings, jewels,
  helpers load from `Data/Item/` by name and index (`OpenItems`, `ZzzOpenData.cpp:613-1251`,
  `Sword01.bmd` = sword index 0). **Armour parts (groups 7-11) load from `Data/Player/`**
  (`HelmMale01`, `ArmorElf..`, `ZzzOpenData.cpp:140-208`), so a set is five models in another
  folder. Textures resolve against the model's folder (`OpenItemTextures`, `:1253`).
- **Repo data:** `src/bin/Data/Item` has 530 BMD, 657 OZJ, 86 OZT (121 BMD in subfolders:
  `Ingameshop`, `LuckyItem`, `xmas`, `cherryblossom`, `partCharge1-8`); `Data/Player` 422 BMD.
  None of the 201 literal `Data\Item` loads is missing. File-name case varies (`sword21` vs
  `Sword21`): harmless on macOS, keep the on-disk spelling in every catalog path.
- **Art constraints for items** (ASTRA.md weapons `:810`, armour `:837`;
  `ASSET_REGENERATION_PLAN.md` phase 3): keep origin, orientation and scale (hands, back and
  shield attach through the model origin, angles hard-coded in `RenderLinkObject`,
  `ZzzCharacter.cpp:6617`); keep mesh count and order (the engine hides/blends meshes by index);
  keep texture-name suffixes (`_R`, `_S`, `_H`, `_N` set render flags); <= 1500 triangles;
  power-of-two textures <= 1024 px; armour keeps the player skeleton and every action.
- **Server sync:** OpenMU sends only group + number. Size (`Width/Height`), class and
  requirements must match the client table; names, damage and prices on the client are display
  only. A **visual** change touches client files only. A **stats/name** change must be made in
  both the client table and OpenMU (admin panel `http://127.0.0.1:8090` Items grid, or the
  `GET admin/backup` JSON).
- **Hot reload today** accepts only `type < MAX_WORLD_OBJECTS` (`ModelHotReload.cpp:347-353`)
  and drops its queue on a map change; `RegenRequest` puts the world name into the branch
  (`RegenRequest.cpp:76-79`) and `RequestFolder` looks in `Object{N}` (`RequestFolder.cpp:30`).

## 3. How it will look

```
+-- Item Editor ------------------------------------------------------------------------------+
| [Browse] [Attributes (existing table)] [Requests]                                           |
| Class: (All)(DW)(DK)(Elf)(MG)(DL)(SUM)(RF)  stage [any v]   Group [Swords v]  Search [____]  |
| Sort: [Tier v] asc/desc    Status: [any v]   [x] only usable   [x] thumbnails                |
+---------------------------------------------+-----------------------------------------------+
| [img] Kris          0-0  T1  drop 6   orig  |  3D preview (turntable, drag to orbit)         |
| [img] Short Sword   0-1  T1  drop 3   orig  |  View: (Inventory)(Ground)(Equipped)(Turntable)|
| [img] ...                                   |  +Level [0..15]  [ ] Excellent [ ] Ancient      |
| [img] Blade         0-5  T3  drop 36  set   |  Show: (as built)(current)(original)            |
| [img] Sword of Destr 0-19 T6 drop 124 exc   |  Model Data/Item/Sword20.bmd  tris 812  tex 2   |
|                                             |  Class DK 1, MG 1   Req lvl 82   2x4            |
|                                             |  [Looks good] [Needs work] [Ask Codex...]       |
+---------------------------------------------+-----------------------------------------------+
```

- **Studio mode** (owner, 2026-09-23): the Item Editor is its own tool, not part of the map. With
  `--items` no map is drawn, the backdrop is plain, and the Item Editor fills the window under the
  MU Editor toolbar (the other editors stay reachable there). `MU Item Editor.app`, built next to
  `Main.app`, starts `Main --editor --items` with a double-click (and can go into the Dock).
- **Browse** is the new default tab: a virtualised list (ImGui clipper) or thumbnail grid of every
  item that has a model; the old table stays as the **Attributes** tab.
- **Class filter:** buttons per class plus a stage choice (base / 2nd / 3rd); "only usable"
  hides items the chosen class cannot equip, via the engine's rule.
- **Tier, basic to rare** (section 4) is the default sort; other sorts: group/index, name, drop
  level, required level, status, last changed.
- **Preview:** the real engine draw of the item (`RenderPartObject`, not the plain body the map
  thumbnails use) so +level glow, excellent shine and ancient effect are visible; views for
  inventory slot, ground drop, equipped on a character (hand, back, full set on the body), and a
  turntable. Armour parts can be shown as a whole set.
- **Ask Codex...** opens the request dialog (section 5).

## 4. "Basic to rare": the tier model

There is no tier field, so the editor computes one and lets the owner override it:

1. `tier_score` = OpenMU `DropLevel` when the item catalog has it, else the client table's
   `Level`; items that never drop from monsters (`DropsFromMonsters = false`: shop, event,
   quest, crafted) sort after all dropping items of their group.
2. Badges shown next to the tier, used as tie-breakers in this order: socket item, set-capable
   (ancient), 380 option, excellent-capable, max item level.
3. `T1..T7` buckets are quantiles of `tier_score` within the item's family (swords, staffs,
   bows, armour sets, wings by generation, jewels, pets), so each family reads basic -> rare.
4. The owner can pin any item to another tier in `assets-work/Items/tiers.json`
   (`{"0-19": {"tier": 7, "note": "..."}}`); overrides win and are marked.

The rules live in one tested function (`Editor::Items::Tier`, no ImGui/engine code) and in the
catalog builder, so the editor and Codex see the same order.

## 5. Asking Codex: item requests

Same contract as the world requests (`WORLD_EDITOR_PLAN.md` section 4,
`assets-work/World1/requests/README.md`), under a new domain:

```
assets-work/Items/
  catalog.json            generated, schema "mu-item-catalog/1"
  tiers.json              owner tier overrides
  client-review.json      owner verdicts (Looks good / Needs work)
  requests/README.md      contract, request.schema.json ("mu-item-regen-request/1"),
                          validate_request.py
  requests/<YYYY-MM-DD>-<item-key>-<slug>/
    request.json  brief.md  captures/*.jpg  delivery/<Model>/
```

**Catalog entry** (key = `<group>-<index>`, e.g. `0-19`): name, group/family, table facts
(size, class stages, required level, drop level), OpenMU facts (drop level, drops from monsters,
sets, sockets, options) when the export is present, tier + badges, model files (BMD + folder,
extra models such as armour's per-class variants), textures and every other item sharing each
texture, skeleton facts (bones, actions, mesh count, triangles), `original` revision + SHA-256,
`batches`, `requests`, `client_review`.

**Request kinds:**

| Kind | Meaning | Must keep |
|------|---------|-----------|
| `upscale` | same design, better textures (resolution, detail, cleaner UVs) | mesh, UVs unless noted, suffixes |
| `repaint` | new colours/materials on the same mesh | mesh, UV layout |
| `remodel` | better geometry, same silhouette and function | origin, orientation, scale, mesh order |
| `redesign` | new look for the same item (owner describes it, may attach reference images) | origin, grip, size in slot, mesh order |
| `set` | any of the above for all parts of an armour set together | per-part skeleton and actions |

The dialog fills in: kind, the owner's notes ("what to change / what to keep"), optional
reference images (copied into `captures/ref-*.jpg`), priority, and generates captures without the
editor overlay: inventory slot, ground, equipped front/side, turntable 4 angles, and +0/+9/+13
excellent glow. Constraints are copied from the catalog: files owned by the request, textures
shared with other items (listed so Codex either leaves them or includes the partners), engine
rules from section 2, triangle and texture limits.

**Lifecycle** (unchanged): `open -> claimed -> delivered -> accepted | rejected`, `withdrawn`.
The request reaches Codex only after it is committed and pushed to `origin/main`
(`docs(assets): file item request <id>`). A worker claims it on
`codex/item-req-<item-key>-<slug>` in its own worktree, delivers into `delivery/`, installs only
the owned files under `src/bin/Data/Item` or `Data/Player`, runs `bmdconv validate`,
`bmdconv compare`, `mu_texture.py check` and `validate_request.py`, and opens a PR on
`vaskodagamo/MuMain` only when `push_allowed`. The owner then pulls the branch, sees it in the
editor (section 6, A/B) and presses **Accept** or **Reject with notes** (a rejection with notes
can be re-filed as a follow-up request in one click).

## 6. A/B compare

`materialize_variant.py original --items` writes `out/ab/original/Data/Item/` and
`out/ab/original/Data/Player/` from git at each catalog item's `original.revision`. The Browse
preview switches one item, one family, or all between *as built*, *current* (`src/bin/Data`)
and *original*; a side-by-side split view renders both at once for the selected item. Reload
goes through `ModelHotReload` with the item model range allowed.

## 7. Milestones

Each milestone is one PR from its own branch, ends with a build of the `macos-arm64-mueditor`
preset, `ctest` green, and an offline visual check; the player build stays behaviour-identical.

| # | Milestone | Acceptance |
|---|-----------|------------|
| I0 | **Shared refactor**: move `MapEditorRepoMirror` + `MirrorSavedFile` to `MuEditor/Core`; give `RegenRequest`/`RequestFolder`/`RequestBrief` a *domain* (asset root, data folders, branch prefix, schema) with `World{N}` as one domain; `ModelHotReload` takes an allowed type range and a "survives map change" flag. No behaviour change for the Map Editor. | Existing editor tests pass; a World1 request written before/after is byte-identical apart from timestamps. |
| I1 | **Item Editor on the Mac + offline**: current table runs on macOS; Save mirrors into `src/bin/Data/Local/...` with backup; `./Main --editor --items` opens the offline scene with the Item Editor open (reusing `OfflineWorld`). | Open, edit a name, save, see the change in `git status`; revert. |
| I2 | **Item catalog + tiers (tools)**: `tools/item_editor/build_item_catalog.py` decodes `Item_<lang>.bmd`, maps each item to its model/texture files (table generated once from `OpenItems`, checked into `tools/item_editor/item_models.json` and verified by a test that parses `ZzzOpenData.cpp`), reads BMD facts through `bmdconv info`, folds in an optional OpenMU export (`tools/item_editor/export_openmu_items.py` reading the admin backup JSON), computes tiers; schema + validator + README for requests. | `assets-work/Items/catalog.json` covers every item with a model; unit tests for the XOR/checksum decoder, the tier rules and the validator. |
| I3 | **Browse tab + studio mode + launcher**: list/grid, class + stage filter using `IsRequireEquipItem`, family/group filter, search, sort by tier and the other keys, status column, thumbnails through a generalised `ObjectThumbnail`. `--items` draws no map (plain studio backdrop) and the Item Editor fills the window; a double-clickable `MU Item Editor.app` next to `Main.app` starts it. | Filter DK stage 1, sort by tier: swords run Short Sword/Kris -> ... -> top tier; screenshot of studio mode; the launcher opens it. |
| I4 | **Preview viewport**: offscreen render through `RenderPartObject` with +level, excellent and ancient controls; inventory-slot view via `RenderObjectScreen`; ground; turntable with orbit; equipped view on the offline hero (weapon in hand, shield, wings on back, full armour set). | Screenshots of one sword at +0/+9/+13 exc, one wing on the hero, one armour set. |
| I5 | **Ask Codex**: request dialog, captures, reference images, `client-review.json` verdicts, Requests tab listing every item request with status, Accept / Reject-with-notes that write the owner's decision file (never the ledger). | `validate_request.py` OK on a Sword01 `upscale` request; test data removed. |
| I5b | **Concepts in the editor** (owner, 2026-09-23): multi-select in Browse, "Generate concepts (N items)..." with preset, variants, per-item notes and the cost estimate before spending; runs `concepts.py` in the background with progress; a Concepts panel per item (current look and every variant, Pick, Refine with comment -> a new round from that variant, Discard); the pick feeds "Ask Codex..."; plus a full-screen toggle and a remembered UI size. Backend first (`concepts.py`: key from `$OPENAI_API_KEY` or the macOS Keychain, JSON progress/list protocol, refine from a variant, Python 3.9), then the panel after I5. | From the editor: generate for 3 items, refine one with a comment, pick, file the request with the concept attached. |
| I6 | **A/B**: `materialize_variant.py original --items`, current/original/as-built switch per item/family/all, split view. | Sword01 toggled original/current in the running client; memory and texture count return to the start after a round trip. |
| I7 | **Pilot with Codex**: the owner files 3 requests (one weapon `upscale`, one wing `remodel`, one armour `set`); Codex workers deliver; owner accepts/rejects in the editor. Lessons folded into README/ASTRA. | Three requests reach `accepted` or `rejected` through the editor alone. |

Parallelism: I0 first (small, touches shared code). Then two lanes in parallel: **C++ lane**
I1 -> I3 -> I4 -> I5 -> I6 and **tools lane** I2 (Python, no C++ overlap). I3 needs I2's
catalog to show status/tier but can start against the client table with the catalog columns
empty. I7 starts after I5; art workers never touch editor code.

Codex task prompt template (one per milestone, paste into a new Codex task):

```
Read AGENTS.md, docs/CODING_RULES.md, docs/agents/HANDOFF.md and
docs/agents/ITEM_EDITOR_PLAN.md. Implement milestone <I#> exactly as its row and sections
<refs> describe, on branch feat/item-editor-<i#> from origin/main. Build preset
macos-arm64-mueditor, run ctest, do the acceptance check offline and attach screenshots.
Player build must not change behaviour. Update the plan's Status list and
src/MuEditor/UI/ItemEditor/ITEM_EDITOR.md, add a WORKLOG entry. Open the PR only on
vaskodagamo/MuMain (--repo vaskodagamo/MuMain); do not merge.
```

Status:

- Plan written 2026-09-23 (PR #21). Split agreed with the owner the same day: Claude sessions
  implement I0-I6, Codex does the art (I7 and every item request afterwards).
- **I0 done (2026-09-23).** Shared code for a second editor, no Map Editor behaviour change:
  - `Editor::Assets::RequestDomain` (asset folder, branch word, optional world, model data
    folders, protected paths, schema, filed-by) replaces `RequestDraft::world`/`worldName`;
    `WorldRequestDomain(world, name)` gives the Map Editor's. `RequestFolderPath`,
    `CapturePath`, `RequestsDir`, `ExistingRequestIds`, `TakenModelNames` and the brief take the
    domain; `request.json` writes `world` only when the domain has one. The request dialog
    keeps one `m_domain`.
  - `MapEditorRepoMirror` is now `MuEditor/Core/RepoMirror`; the generic half of
    `MapEditorFileUtil` (`DataDir`, `ReadWholeFile`, `RepoRoot`, `MirrorSavedFile`,
    `CopyToRepoExports`, `DescribeSavedFiles`, `OpenWithSystem`) is `MuEditor/Core/EditorFiles`;
    `MapEditorFileUtil` keeps the map folders and files and includes it. Their log lines say
    `[Editor]` instead of `[MapEditor]`.
  - `HotReload::ModelRange` + `AllowRange`: the reload accepts every allowed block of `Models[]`
    (world objects always; texture filter/wrap and "follows the map" per range). Requests for a
    range that does not follow the map survive a map change.
  - Checked: `request.json` and `brief.md` for every kind (two targets, new variant, push
    allowed) byte-identical before and after (temporary dump test, removed); new tests for an
    items-like domain and the world domain; `ctest` 340/340 in `macos-arm64-mueditor`, the
    editor tests also in `macos-arm64`; `./Main --editor --world 1` opens Lorencia offline,
    finds the repository and captures a frame. Not exercised at run time: a reload of a model
    outside the world range (nothing allows one yet; I6 does).
  - Found for I1: the shipped `Item_Eng.bmd` loads as the legacy format (30-byte names,
    946 items).
- **I1 done (2026-09-23).** The Item Editor runs on the Mac; usage in
  `src/MuEditor/UI/ItemEditor/ITEM_EDITOR.md`.
  - `./Main --editor --items` opens the offline world (1, or `--world N`) with the Item Editor
    open and the Map Editor closed; `--editor --world N` unchanged.
  - Save writes the game's `Data/Local/<lang>/item_<lang>.bmd` (spelled as git tracks it) and
    mirrors it into `src/bin/Data` with a backup; the exports also go to `out/editor-exports`.
  - **Save keeps the file's layout and bytes:** the saver used to write the 50-byte-name layout,
    so every save converted the shipped legacy files. Editor builds now keep the loaded records
    (`ItemData/ItemFileSnapshot`, recorded inside `#ifdef _EDITOR`) and write that layout back,
    keeping the loaded bytes of every unedited item (the shipped files carry text after names,
    padding and one non-UTF-8 name that a conversion drops). `editor_item_table_tests`: all four
    shipped tables save byte-identical; a rename changes only that record and the checksum.
  - Names are limited to what the layout holds (29 bytes legacy); the three long ticket names
    (13-121, 13-125, 13-127) overflow into their Two-Hand/Level fields in the shipped file, so
    those items' stats are not reliable data.
  - Fixed: the editor console hung the process on the first `std::cout` line.
  - Checked: 343/343 tests (`macos-arm64-mueditor`), 342/342 in `macos-arm64`; a temporary,
    uncommitted ImGui-input hook searched, opened Columns, renamed Kris, saved (`git status`
    showed `item_eng.bmd` modified, backup written), saved again ("No changes"), copied a row,
    both exports; also under `MTL_DEBUG_LAYER=1`. Not tried by hand; Windows not built.
- **I2 done (2026-09-23).** Item catalog, tiers and the item request contract; usage in
  `assets-work/Items/README.md`.
  - `tools/item_editor/`: `item_table.py` (both `Item_<lang>.bmd` layouts, checksum,
    `ItemSetType.bmd`, `ItemAddOption.bmd`), `gen_item_models.py` -> `item_models.json` (walks
    the model loads in `ZzzOpenData.cpp` with a small evaluator for its loops, `--check` keeps
    it current), `bmd_facts.py`, `tiers.py`, `export_openmu_items.py` (admin backup or a
    password-less read-only `docker exec psql` SELECT), `build_item_catalog.py` (`--check`).
  - `assets-work/Items/catalog.json` (`mu-item-catalog/1`, ~2.5 MB): 889 items with a model,
    939 models, no model file missing; 14 texture names without a file and the table items
    without a model (70, 38 of them unused "-J" duplicates) are listed in the catalog.
    `openmu-items.json` is the real export of the local OpenMU (676 definitions, 674 matched).
  - Tiers: section 4 with one change: a percentile rank with ties counted half instead of plain
    quantiles, so a family whose items share one score (third-generation wings) sits at T4
    instead of T1. Swords run Short Sword, Kris, Rapier (T1) ... Knight/Bone/Explosion Blade (T7);
    items that never drop from monsters come last in their family.
  - `assets-work/Items/requests/`: README, `request.schema.json` (`mu-item-regen-request/1`),
    `validate_request.py`; ids and branches use the item key (`2026-09-23-0-0-<slug>`,
    `codex/item-req-0-0-<slug>`); the owner's accept/reject goes to
    `requests/<id>/owner-decision.json`, the coordinator's reservations to
    `assets-work/Items/assignments.json`.
  - Checked: 56 Python tests (`python3 -m unittest discover -s tools/item_editor/tests`), also
    in ctest as `item_editor_tools`; catalog `--check` with bmdconv.
  - For I3/I5: read `tier.value`/`tier.family_rank` from the catalog, but filter classes with
    `IsRequireEquipItem`; I5's request JSON must write the item target shape of the README.
    Possible engine bug, not changed: `ItemSetType.bmd` marks "no set" with 0, while
    `CSItemOption::IsChangeSetItem` tests only for `0xFF`.
- **I3 done (2026-09-23).** Browse tab, studio mode and launcher; usage in `ITEM_EDITOR.md`.
  - **Studio:** `--editor --items` draws no map (flat backdrop, `Core/ItemStudio`; World1 is
    still loaded but not drawn, because the Map Editor on the toolbar needs a loaded map and I4's
    hidden hero needs ground and light) and the Item Editor fills the window. Opening the Map
    Editor shows the map and floats the Item Editor; closing it restores the studio.
    `--items --world N` shows the map behind a floating Item Editor; `--world N` unchanged.
  - **Launcher:** `MU Item Editor.app` next to `Main.app` (`cmake/ItemEditorLauncher.cmake`,
    `if(APPLE AND ENABLE_EDITOR)`); runs `./Main --editor --items` from the game's folder, also
    when moved elsewhere.
  - **Browse:** class buttons + class stage (the engine's rule, now shared as
    `GameLogic::Items::CanClassEquip`; `IsRequireEquipItem` calls it, same result), family list
    with counts, tier range, status (original / changed / at Codex), search in any letter case
    (`EditorText::FoldCase`), sorts (tier default, key, name, drop/required level, status), list
    and grid with thumbnails framed from each model's bounds; details panel with a marked place
    for I4's preview; selection synced with the **Stats table** tab (which now scrolls to the
    exact row). Data: `Assets/ItemCatalog`, `Assets/ItemBrowse`, `Assets/JsonFields`.
  - Checked: 366/366 tests (`macos-arm64-mueditor`), 365/365 (`macos-arm64`); in the client with
    a temporary, uncommitted ImGui-input hook: studio, DK stage 1 swords Short Sword ... Sword of
    Destruction (Blade Master reaches T7 Knight/Bone Blade), grid, selection sync both ways,
    launcher via `open`, `--world 1` and `--items --world 1` unchanged, `MTL_DEBUG_LAYER=1`
    clean, ~75 fps (vsync) scrolling all 959 items with thumbnails. Not tried by hand; Dock
    drag not tried; Windows/Linux not built.
  - For I4: draw the preview in `RenderPreviewArea` (`ItemBrowseDetails.cpp`); offscreen renders
    in the studio run from `ItemStudio::RenderInsteadOfWorld()`. For I5: "at Codex" comes from
    the catalog's requests, so rebuild the catalog after filing.
  - Found, not changed: Map Editor object thumbnails always use a fallback frame (the engine
    never fills the model bounds they read).
- **I4 done (2026-09-23).** Live 3D preview in the Browse details panel; usage in
  `ITEM_EDITOR.md`.
  - Views: **Turntable** (drag, wheel, auto-turn, Front/Side/Back/Reset), **Inventory** (the
    game's `RenderItem3D` placement in a slot of the item's size; turns on hover as in game),
    **Ground** (dropped on the loaded map's terrain, through the game's own drop code),
    **Equipped** (on a preview character of its own, never the hero: weapons in hand or on the
    back in a town, shields, bows with quiver, wings, the item's whole armour set).
  - Look: +0..+15, Excellent, Ancient (labelled when the catalog says the item can never be),
    and "every +level effect" regardless of the game's effect option (the option is not
    changed). The character uses the Browse class filter's class and stage, else the item's
    first allowed class.
  - One offscreen target, remade only when the panel grows or shrinks by 64 px, freed 3 frames
    after the preview is hidden. Files: `UI/ItemEditor/ItemPreview*`, `PreviewCharacter`,
    `Editing/Preview{Camera,Slot,Outfit}` (unit-tested), `Core/ModelPose`,
    `Core/ScopedOffscreenCapture.h`.
  - Player build, same behaviour: the per-item body of `RenderItems` is now
    `RenderDroppedItem` and the landing step of `MoveItems` is `PlaceItemOnGround`, so the
    preview reuses them; one editor-only hook in `MainScene.cpp`.
  - Checked: 382/382 editor-build and 381/381 player-build tests; scripted in-client run with
    screenshots (Sword of Destruction 0-16 at +0/+9/+13 excellent in turntable and inventory,
    ground, Wings of Dragon on a Blade Knight from behind, Dragon Armor set at +11, Dragon
    Shield, Silver Bow with quiver), `MTL_DEBUG_LAYER=1` clean, ~75 fps, `--world 1` unchanged.
  - Not drawn: world joints and model effects (e.g. third-wing trails), pets and mounts. With
    the map drawn, the preview character's particles appear at the map's start point. To check
    against the game: the sword's idle stance, and the inventory slot scale (the sword reaches
    past its 1x4 outline).
  - For I5: `g_ItemPreview.Texture()` is a clean image, but the renderer has no texture
    readback yet (add an editor-only one, or crop a `ViewCapture` frame); scripted captures need
    setters for view, orbit, level and flags. For I6: a second preview scene and target in the
    same pass gives side by side; call `g_ItemPreview.Release()` after a model hot reload.
- **I5 done (2026-09-23).** Ask Codex from the Item Editor; usage in `ITEM_EDITOR.md`.
  - **Captures:** an editor-only read-back of an offscreen texture (`RequestTexturePixels`, SDL_gpu
    is the only backend; it shares one download helper with the full-frame read-back, player
    build unchanged). `ItemCaptureRun` takes 8-9 clean 1024 px JPEGs in about a second (front,
    side, back, three-quarter, inventory, equipped, +0/+9/+13 excellent) and restores the owner's
    preview settings.
  - **Ask Codex dialog:** kind (set preselected for armour), priority, summary, change / keep /
    avoid, push allowed, captures, the picked concept as `captures/ref-concept.jpg`, extra
    reference images; warnings (HEAD not on origin, model differs from the catalog, open request
    exists); writes the folder atomically (`ItemRequestDomain()`, `Assets/ItemRequest*`), runs
    `validate_request.py` in the background and shows the git commands to copy. It never commits
    or pushes.
  - **Verdicts** (Looks good / Needs work -> `client-review.json`), **Requests tab** (live scan of
    `assets-work/Items/requests/` every 1.5 s; Withdraw, Accept / Reject with notes ->
    `owner-decision.json`, Re-file superseding a rejected request), live "at Codex" / "delivered"
    in Browse. The Map Editor's request output is unchanged (shared folder/brief/JSON helpers).
  - `build_item_catalog.py --check` ignores request status, so filing a request no longer makes
    the tools tests fail until the catalog is rebuilt.
  - Checked: 395/395 editor-build and 394/394 player-build tests, 87 tools tests; scripted
    in-client run: Short Sword upscale and Small Shield redesign with a concept filed and
    validated, Requests tab, withdraw, simulated delivery accepted / rejected / re-filed, verdicts,
    armour opens as a set, Metal validation clean, `--world 1` unchanged. Not driven: the Add
    image file dialog; no real delivery validated yet.
  - Known: front/back captures show swords and shields edge-on (the preview's buttons); the
    owner's decision must be committed on the worker's branch (the dialog shows the push).
- **Concept images (added by the owner, 2026-09-23; built the same day).** Before Codex models an
  item, `tools/item_editor/concepts.py` generates concept art through the OpenAI Images API
  (`/v1/images/edits` with the item's current render as reference), the owner picks one, and the
  pick becomes the request's `captures/ref-concept.jpg`. Usage in `assets-work/Items/concepts/`.
  - Model choice (official docs, 2026-09-23): `--preset explore` = `gpt-image-2.5-flare`, medium,
    3 variants in one request, 512 px reference (~$0.05 per item); `--preset final` =
    `gpt-image-2.5-sunburst`, high, 1024 px reference. 2.5 has no Batch API; Tier 1 allows
    5 images/minute.
  - Safety: key only from `$OPENAI_API_KEY` (never printed or written; tested), `plan` is a dry
    run with a text/reference/output cost split, `run` needs `--yes` and refuses above
    `--max-images` (30) or `--max-cost` ($5); batches in `out/item-concepts/` (not in git), only
    picks are committed.
  - References: offline Blender renders through the study's pipeline (`refs`); alpha textures
    render opaque and armour stands in bind pose. Once I4 lands, in-client renders are better.
  - Checked: 82 tests with a mock server; no real API call yet (the owner sets the key).

## 8. Later options

- **Stats and names in sync with OpenMU**: an "Attributes" save that also writes a patch for the
  OpenMU backup JSON (or drives the admin panel), with a check that size, class and
  requirements match on both sides.
- **New items**: new group/index, table entry, data-driven model table instead of `OpenItems()`
  code, OpenMU definition.
- Headless captures for Codex (`--items --capture <key>`) so workers can render their own
  before/after in the true client.
- Excellent/ancient/level glow tuning (engine code, not assets) as its own request kind.
