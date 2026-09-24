# Item Editor (MuEditor)

Every item the client knows, in three tabs: **Browse** (a list or thumbnail grid you filter by class,
family, tier and status, sorted from basic to rare, with a live 3D preview and the facts of the
selected item, an **A/B compare** of its current, original and new versions, its concept images, your
verdict on it and **Ask Codex...**), **Stats table** (every field of
`Data/Local/<lang>/Item_<lang>.bmd`, edited in place and saved into the game's own file) and
**Requests** (every item request you filed for the art builder, with its status, and your withdraw /
accept / reject). It exists only in editor builds (`ENABLE_EDITOR`), so the normal game is unaffected.
The quick start is for the Mac; everything after it is reference. The Map Editor's guide,
[MAP_EDITOR.md](../MapEditor/MAP_EDITOR.md), covers building, logs and the editor in general.

### Quick start (macOS)

1. **Build** (repository root; the first build takes several minutes):
   ```sh
   PATH=/opt/homebrew/bin:$PATH cmake --preset macos-arm64-mueditor -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
   PATH=/opt/homebrew/bin:$PATH cmake --build --preset macos-arm64-mueditor-release
   ```
   Next to `Main.app` the build makes **`MU Item Editor.app`**
   (`out/build/macos-arm64-mueditor/src/Release/`).
2. **Start it:** double-click `MU Item Editor.app` in Finder. It starts the client offline (no server,
   no login) in the Item Editor's **studio**: no map, a plain dark backdrop, and the Item Editor
   filling the window under the MU Editor toolbar. From a terminal the same is
   ```sh
   cd out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS && ./Main --editor --items
   ```
3. **Keep it at hand:** drag `MU Item Editor.app` onto the Dock (the Dock keeps a shortcut; the app
   stays in the build folder). You can also copy it into `/Applications`: a copy that is not next to
   `Main.app` starts the `Main.app` of the build that made it, so it keeps working as long as that
   build folder exists. The build itself installs nothing outside the build folder.
4. **New look, step by step** (see [Concepts](#concepts)):
   1. **Select** the items in Browse: click one, **Cmd-click** more (Ctrl-click off the Mac),
      **Shift-click** a range, or tick the boxes.
   2. **Generate concepts (N)...** above the list: choose *explore* (3 cheap variants per item) or
      *final*, add notes, and read the estimate. Nothing is spent until **Generate for $X** (the
      exact amount the tool estimated); the run goes on in the **Concepts job** window.
   3. **Pick** the variant you like in the item's **Concepts** section (under the preview), or
      **Refine with comment...** a variant for a new round from it.
   4. **Ask Codex with it...** opens the request dialog with the picked concept attached.
5. **Browse:** pick a class (**DW DK Elf MG DL SUM RF**) and its stage (for example *Dark Knight*,
   *Blade Knight*, *Blade Master*) to see only what that character can equip, a family (swords,
   helms, wings-2, ...), and click an item: its 3D preview and facts open in the item's own window. See
   [Browse](#browse) and [Preview](#preview).
6. **Edit stats:** the **Stats table** tab is the table of every field; an item picked in Browse is
   selected and scrolled to there, and the other way round. See [Stats table](#stats-table).
7. **Ask Codex:** under the preview, **Ask Codex...** files a request for the art builder to upscale,
   repaint, remodel or redesign the item, with clean captures of the preview; commit and push the
   folder it writes. The **Requests** tab follows it from there. See [Ask Codex](#ask-codex).
8. **Compare versions:** under the preview, **A/B compare** switches the item between what the build
   installed, your checkout, the originals and new versions (a Codex delivery, the style pilot's A and
   B), and **Side by side** shows two versions next to each other. See [A/B compare](#ab-compare).
9. **Screen:** **Full screen** on the toolbar (or **Cmd+Ctrl+F**; **F11** on Windows and Linux) fills
   the display, **Window** goes back. **- 125% +** sets the size of all editor text and buttons. Untick
   **Console** to hide the editor and game consoles at the bottom and give the Item Editor their room.
   All three are remembered for the next start (`MuEditor/MuEditor.ini`); the first start on a display at least
   1440 points high (a 1440p or 5K screen) uses 125%, else 100%.

The other editors stay on the toolbar. Opening the **Map Editor** from the studio shows the map
(Lorencia) and turns the Item Editor into a normal window; closing the Map Editor brings the studio
back.

### Starting options

| Command | What opens |
|---------|------------|
| `MU Item Editor.app`, or `./Main --editor --items` | The studio: no map drawn, the Item Editor fills the window. |
| `./Main --editor --items --world N` | Map N (Data folder number, 1 = Lorencia) offline behind a floating Item Editor. |
| `./Main --editor --world N` | Map N offline with the Map Editor (unchanged). |
| **F12** in a game session | The editor over the running game; the toolbar's **Item Editor** button shows the window. |

**Why the studio still loads a map:** it skips drawing (terrain, objects, sky, weather, effects,
fog), not loading. The Map Editor on the toolbar edits and saves the loaded map, so it needs one, and
the preview's dropped item and dressed character stand on that map's ground and light (at the start
point, where the hidden hero waits).
World1 is loaded once at start (about a second) and not drawn while the Item Editor fills the window.

## Browse

**Left - filters.**

- **Search:** part of the name, in any letter case (also accented, Greek and Cyrillic letters), or
  the item key such as `0-19`.
- **Class and stage:** shows only what that class may equip at that stage, by the game's own rule
  (see [Who may equip an item](#who-may-equip-an-item)). **All** switches the filter off.
- **Tier range** (drag either end) and **status** (original / changed / at Codex / delivered / unknown).
- **Only items with a model** hides table rows the client loads no model for (the unused "-J"
  duplicates and a few tickets).
- **Family:** swords, axes, ..., potions, with how many items of each pass the other filters.

**Middle - the items.** **List** shows a check box, a thumbnail, name, key (`group-index`), tier, drop
level, required level, the classes that may use it (class and lowest stage, e.g. `DK 2`), the status
and the concepts (`picked`, or how many concept images it has); **Grid** shows bigger thumbnails with
tier and name, the concepts in the tile's corner and a check box on selected or pointed-at tiles.

**Selecting several items:** a click selects one item; **Cmd-click** (Ctrl-click on Windows and
Linux) or its check box adds or removes one; **Shift-click** selects the range from the item clicked
before. The bar above the list shows how many are selected, a chip per item (click it to take the
item out), **Select all shown** (the items the filters show), **Clear**, and **Generate concepts
(N)...**. The item window shows the item clicked last; it stays there after **Clear**.
Items the filters hide stay selected. The sort choice and **Ascending/Descending** are
above it:

| Sort | Order |
|------|-------|
| **Tier (basic -> rare)**, the default | Named items first; items that drop from monsters before shop, event and quest items; then tier T1..T7, the item's place in its family, name. |
| Group / index | The item number (`group * 512 + index`). |
| Name, Drop level, Required level, Status | That value; ties in the tier order. |

Items without a catalog entry (no tier) stay at the end of the tier order in both directions. Within
one family the tier order is the catalog's family order, so for example the Dark Knight's swords run
Short Sword, Kris, Rapier ... Knight Blade, Bone Blade, then the Divine Sword (a quest reward that
never drops).

**The item window:** a click on an item opens it (drag it anywhere, resize it by its corner, close it
with its x; the next click opens it again, with the same place and size). It holds: name, key, the live 3D preview (see [Preview](#preview)), the
[A/B compare](#ab-compare), your verdict
(**Looks good** / **Needs work**), the item's open requests and **Ask Codex...** (see
[Ask Codex](#ask-codex)), its **Concepts** (see [Concepts](#concepts)), classes, required and drop
level, status, family, tier (its place among the family's items and whether it drops from monsters),
what it can be (socket, ancient set, 380 option, excellent, maximum +level), size in inventory slots,
then every model file with triangles, meshes and textures and the original commit.

**Thumbnails** are rendered from the loaded model the first time a row is shown (six per frame) and
kept; long items lie corner to corner, flat ones show their broad side. Armour parts load from
`Data/Player` per class; the thumbnail shows the item's own model, the male (first) one, and the
details list the class variants after it. Items without a model show an empty frame.

### Status

| Status | Meaning |
|--------|---------|
| original | Every model file in your checkout has the SHA-256 the catalog recorded as original. |
| changed | A model file differs from its original (or is missing). |
| at Codex | A request for the item is open or claimed. |
| delivered | A request for the item is delivered: the art builder's version waits for your Accept or Reject. |
| unknown | The item has no catalog entry, or there is no catalog. |

The files are hashed once, the first time Browse opens (about 20 MB, well under a second);
**Rescan files** hashes them again after a pull or a delivery.
"at Codex" and "delivered" come from the request folders on disk (`assets-work/Items/requests/`), read
again whenever one is filed, withdrawn, pulled or delivered, so they are right without rebuilding the
catalog (the catalog's own list of requests is only used without a checkout).

### Tier and family

Tiers, families and the order within a family come from `assets-work/Items/catalog.json`; the rule is
in [`assets-work/Items/README.md`](../../../../assets-work/Items/README.md#tiers-basic-to-rare) and the
editor only reads the result. Without a catalog (no checkout above the game folder, or the file is
missing) Browse still lists every item of the client table with its classes and levels, and the
family list says why tier, family and status are missing. Rebuild the catalog with
`tools/item_editor/build_item_catalog.py` after a model or table change.

### Who may equip an item

Each item has a class entry per base class (DW, DK, Elf, MG, DL, SUM, RF): the lowest stage that may
use it, 0 for never. Stage 1 is the base class, 2 the second class (Soul Master, Blade Knight, Muse
Elf, Bloody Summoner), 3 the third (Grand Master, Blade Master, High Elf, Duel Master, Lord Emperor,
Dimension Master, Fist Master); Magic Gladiator, Dark Lord and Rage Fighter have no second class. A
class may equip the item when its entry is not 0 and not above its stage. As in the original client,
a Magic Gladiator may also use an item that both the Dark Wizard and the Dark Knight may use, even when
the MG entry is 0. The game then also checks strength, agility, energy, vitality, command and level;
Browse filters by class and stage only. Browse and the game share this rule
(`GameLogic::Items::CanClassEquip`, called by `IsRequireEquipItem`).

## Preview

The item as the game draws it - the engine's own item and character code, not a picture of the model
- in the item window, with four views:

| View | What you see | Mouse |
|------|--------------|-------|
| **Turntable** | The item alone, lit like a dropped item; long items (swords, staffs, spears, bows) stand upright on their grip. | Drag to turn, wheel or **- +** to zoom. |
| **Inventory** | Exactly what the inventory shows: the game's own placement, angle and size for that item in a slot of its size (the lines are the slot's cells). | Point at it: it turns, as in the game. |
| **Ground** | The item dropped on the map's ground (Lorencia's town square in the studio), as a player finds it. | Drag, wheel, **- +**. |
| **Equipped** | Worn by a character: see below. | Drag, wheel, **- +**. |

Under the picture: **Front / Side / Back** put the camera there (front is the item's broad face: the
flat of a blade or a shield's face, the front of armour and wings), **Reset** frames the item again, **-** and **+** zoom out and in,
**Turn** turns the view slowly by itself (on at start; any of the buttons stops it).

**+level, Excellent, Ancient:** the slider sets +0..+15; the game's effects follow it (+3/+5 tints,
the gold and chrome passes from +7, the moving shine from +9 up). **Excellent** adds the excellent
shine (the pulsing colour), **Ancient** the ancient one; when both are on, the game shows only the
excellent one, and so does the preview. Next to the boxes "(never in the game)" or "(no set)" says
when the catalog knows the item cannot be excellent or ancient; the preview still shows it.

**Every +level effect:** the game draws the +7..+15 effects only up to its render-level option (the
option window's effect slider; the preview shows the current value, "4 of 4" = all). Tick the box to
see them all whatever the option says; the option itself is not changed.

**Equipped:** a character of its own (never the game's hero) wears the item the way the game puts it
on:

| Item | Worn |
|------|------|
| Sword, axe, mace, spear, staff | In the right hand (two-handed ones too). |
| Shield | On the left arm. |
| Bow | In the left hand, arrows as a quiver on the back. |
| Crossbow | In the right hand, bolts as a quiver on the back. |
| Arrows, bolts | The quiver on the back. |
| Wings, capes | On the back; with wings the character floats in the flying pose, as outside a town. |
| Helm, armour, pants, gloves, boots | With the rest of its set: the parts of the other armour groups with the same index (a set without a helm leaves the bare head). The +level and options go on every part. |
| Pets, rings, pendants, everything else | The character without it ("worn, but not drawn on the body" / "cannot be worn"). |

The character is the **class chosen in the class filter** at its stage (e.g. *DK* + *Blade Master*);
with **All** it is the first class that may use the item, at the lowest stage that may (the line under
the controls names it). **In a town** puts weapons on the back, as the game does in safe zones.
Hand-held effects that the game draws as sprites (glows of some +level weapons) show too; in the
studio also the particles.

**Speed:** the preview draws every frame into one picture the size of the panel (made again only when
the panel grows or shrinks by 64 pixels) and is freed when the Browse tab or the Item Editor is
closed. With the preview open the studio keeps its ~75 frames per second.

### Preview code

| File | What it holds |
|------|---------------|
| `UI/ItemEditor/ItemPreview.h/.cpp` | `CItemPreview` - the panel: view choice, picture, mouse, buttons, +level/options; owns the render target and asks for one picture per frame. |
| `UI/ItemEditor/ItemPreviewScene.h/.cpp` | `CItemPreviewScene` - draws each view through the engine (`RenderPartObject`, `RenderItem3D`, `RenderDroppedItem`, the map's `RenderTerrainTile`, sprites) into the open offscreen capture, and puts back every engine value it changes. |
| `UI/ItemEditor/PreviewCharacter.h/.cpp` | `CPreviewCharacter` - the preview's own `CHARACTER`, dressed and drawn by `SetPlayerStop`, `MoveCharacter` and `RenderCharacter`. |
| `Editing/PreviewCamera.h/.cpp` | Orbit, zoom, automatic turn, the upright pose of long items, look-at matrices, target sizes (pure math). |
| `Editing/PreviewSlot.h/.cpp` | The inventory view's slot and camera numbers in the target (pure math). |
| `Editing/PreviewOutfit.h/.cpp` | Which hand, back or body part the item goes on, the rest of an armour set, and the class to dress (pure rules). |
| `Core/ModelPose.h/.cpp`, `Core/ScopedOffscreenCapture.h` | Shared with the thumbnails: a posed model's bounds, and closing an offscreen capture on every path. |
| `UI/ItemEditor/ItemAbCompare.h/.cpp`, `ItemAbCaptures.h/.cpp`, `ItemModelTypes.h/.cpp` | The A/B compare panel, its capture sheets, and which `Models[]` slots an item uses. |
| `Core/ModelCopy.h/.cpp`, `Core/ModelFileCheck.h/.cpp` | The right picture's model copy (swapped into the engine's model only while that picture is drawn) and the file check it shares with the hot reload. |
| `Assets/ItemCandidates.h/.cpp` | Finding deliveries, pilot variants and folders for an item (unit-tested in `tests/editor/test_item_candidates.cpp`). |

The pure parts are unit-tested in `tests/editor/test_preview_camera.cpp` and `test_preview_outfit.cpp`.
The scene hooks are `Editor::ItemStudio::RenderInsteadOfWorld()` (studio) and `RenderAfterWorld()`
(map drawn), called by the main scene; the engine side shares `RenderDroppedItem` and
`PlaceItemOnGround` with the game's own dropped items (`ZzzObject.cpp`).

## A/B compare

Under the preview of the selected item: which files the running client draws the item with, and a
second picture of it from other files. Nothing here writes a file; the switch lasts until you switch
again or restart the editor.

**The client shows:** *as built* (what the client loaded at start from the game's own `Data` folder,
which the last build copied from `src/bin/Data`), *current*, *original*, a candidate's name, or *mixed*
(the item's models show different versions). Next to it: how many textures the client holds and their
memory, so you can see that switching back gives the start values again.

| Button | What the client shows then |
|--------|----------------------------|
| **as built** | The game's own `Data` files again (the start state). |
| **current** | Your checkout's `src/bin/Data`: pulled or delivered files show here before the next build. |
| **original** | The files from before the art rebuild, `out/ab/original/Data` (see below). |
| **Candidates:** *delivery ...*, *before ...*, *pilot A*, *pilot B*, *folder ...* | New files next to the data tree (below); what a candidate does not hold comes from `src/bin/Data`. |
| **Reload from disk** | The shown version read again (e.g. after a pull); also looks for new deliveries and pilot variants. |

The switch goes everywhere the client draws the item: preview, thumbnail, inventory, ground, characters.
A texture keeps its place, so **other items that use the same texture switch with it**; the panel lists
them, and warns in yellow when a candidate replaces a texture other items use (accepting it changes
them too). A file the client could not load (wrong format, too many meshes or vertices, a missing or
oversized texture) is **refused with a message** in the status line; the item keeps what it showed.

**Family ...: / All items:** as built, current or original for every item of the selected item's family,
or for all items (about 900 models, about two seconds). The status line and `MuError.log`
(`[Items] All items: ...`) give how many switched, which were refused and the texture count and memory
before and after. Seven items are always refused, whatever the version: their models name textures
the repository never had (Holy Storm Claw, Crimson Glory, Red Wing Helm, Seal of Healing,
Talisman of Guardian, Rare Item Ticket, Elite SD Potion).

### Side by side

Tick **Side by side** and choose the right picture's version (**right:** as built, current, original or
a candidate). Both pictures use the same view, +level, options and camera: dragging or zooming either
picture, and **Front / Side / Back**, turn both. The item window gets twice as wide (and half again when you untick it). The left picture is the
client's version (its label starts with *client:*), so to compare two candidates, show one in the
client (e.g. **pilot A**) and pick the other on the right (**right: pilot B**). A candidate the client
cannot load leaves the right picture empty with the reason.

**Capture A/B sheet** takes the request capture angles (front, side, back, three-quarter, inventory,
worn, +0/+9/+13 glow) of both pictures and saves, per angle, the two pictures and one sheet with both
side by side into `out/item-ab/<item key>-<time>/` (`01-front-current.jpg`, `01-front-pilot-a.jpg`,
`01-front-sheet.jpg`, ...); **Open folder** shows them. They stay out of the request folder: the request
contract lets you write only `owner-decision.json` there (attach a sheet to a note or a follow-up
request yourself).

### Candidates

| Candidate | Where the editor finds it |
|-----------|---------------------------|
| *delivery &lt;request&gt;* | `assets-work/Items/requests/<request>/delivery/<item key>/exports/` (after you pulled the worker's branch) |
| *before &lt;request&gt;* | The same delivery's `original/`: the files the request started from |
| *pilot A*, *pilot B* | `assets-work/Items/pilot/<model file name>/<A, B, ...>/`, e.g. `pilot/Axe01/A/` for the Small Axe |
| *folder &lt;name&gt;* | **Load candidate from folder...**: pick any `.bmd`, `.OZJ` or `.OZT` in a folder; the folder is offered for this item until the editor closes |

A folder counts for an item when it holds the item's model file or one of its textures under the
game's file name (any letter case), for example `Axe01.bmd` and `Axes02.OZJ`.

**Codex's style pilot** (`assets-work/Items/pilot/README.md`): select **Small Axe** (1-0), **Small Shield**
(6-0) or **Wings of Elf** (12-0); **Candidates** shows *pilot A* and *pilot B*. Tick **Side by side**:
current on the left, pilot A on the right. Click **pilot A** under Candidates and choose **right: pilot B**
to see A and B together; **Front** stops the turn on the broad face, **Equipped** puts them in the hand,
on the arm or on the back (**Back** for wings). **as built** puts the client back.

**A delivered request:** in the Requests tab, **Compare** (next to Accept and Reject) opens the item in
Browse with your checkout's files on the left and the delivery on the right. When your checkout already
holds the delivery (the worker's branch is checked out), the left picture shows the delivery's *before*
files instead (or the originals), and the panel says so.

### Original files

*original* needs the files from before the art rebuild, built from git once (about 30 seconds, 45 MB):

```sh
python3 tools/world_editor/materialize_variant.py original --items
```

It writes each catalog model at its original commit (SHA-256 checked against `catalog.json`) and every
texture it names into `out/ab/original/Data/Item/`, `Data/Player/`, ... with `items-manifest.json`. When
they are missing or were built from other originals than the catalog lists, the panel shows the command
with **Copy command** and **Check again**, and *original* is not offered. The Map Editor's world
originals (`... original --world 1`) live in the same folder; each run keeps the other's files.

**Not switched:** the extra models of Rage Fighter gloves (left and right hand) and the models only the
inventory draws (lucky boxes, some event items) stay as they are. In **Equipped**, the right picture
shows only the selected armour part in its other version; the rest of the set is the client's.

## Concepts

Concept images are pictures of a new design for an item, made by the OpenAI Images API from the item's
current look (its reference render). You pick one, and the pick goes with the request to Codex, who
models it. The editor drives `tools/item_editor/concepts.py` (usage and prices in
[`assets-work/Items/concepts/README.md`](../../../../assets-work/Items/concepts/README.md)); it never asks
for, stores or shows the API key: the tool reads it from the macOS Keychain (or `OPENAI_API_KEY`), and
the dialogs only say whether one was found and where.

### Generate concepts (N)...

For the items selected in Browse (or **Generate concepts for this item...** in the Concepts section):

| Setting | Meaning |
|---------|---------|
| **explore** / **final** | The presets of `tools/item_editor/image_prices.json`: explore is a faster model at medium quality, 3 images per item (about 7 cents per item); final is the best model at high quality, 1 image. |
| **Variants per item** | How many images per item (the preset's number to start with). |
| **Turnaround sheet** | Front and side view of the item on one 1536 x 1024 image. |
| **Note for every item** / **Note for this item** | Added to the prompt (what to change, style, colours). |

The dialog runs `concepts.py plan` (a dry run: nothing is sent, nothing is paid) whenever a setting
changes and shows per item whether its reference render exists and what it costs, then the total split
into text prompt, reference image and output images, the caps (at most 30 images and $5 per run), the
API key status, and why the tool would refuse. Items without a reference render: **Render missing
references (N)** renders them offline with Blender (free; setup in
[the macOS build guide](../../../../docs/build/macos/console.md#item-editor-concept-renders)), then the estimate updates. **Generate for $X**
is enabled only when the tool would run; it starts the run in the background and closes the dialog.

### Concepts job

The **Concepts job** window follows the run (it stays when the dialog closes, also with the Item Editor
closed; one job at a time): per item *waiting*, *generating*, *retry in N s* (rate limit or a server
error; the tool waits and tries again), *done (3 images)* or *failed* with the error.

- **Cancel** starts no new request; requests already sent finish (they are paid for) and their images
  are kept. The window says "cancelling" until they are back.
- **Resume** after a cancel, or after failed requests, sends only the missing images of the same batch.
- At the end: the estimate and the **actual** cost from the API's usage (of the requests this run sent).
  **Open contact sheet** shows the batch's `sheet.html`.
- If another `concepts.py` run holds the batch (for example one started in a terminal), the job ends
  with "busy" and nothing is sent.

### The Concepts section

Under the preview of the selected item: the **current look** (the reference render), the **picked**
concept, and every variant of every batch, newest batch first. A refine round names the variant it
came from and your comment under each image. Click an image to see it big (**Zoom**, **Fit**).

| Button | What it does |
|--------|--------------|
| **Pick** | Makes the variant the item's concept: `assets-work/Items/concepts/<key>/concept.jpg` (commit that folder to keep it). Browse shows "picked". |
| **Unpick** | Removes the pick. |
| **Refine...** / **Refine with comment...** | A new round from this variant: write what should change, check the estimate, **Refine for $X**. The new variants remember their parent. |
| **Discard** / **Undiscard** | Hides a variant you do not want (no image is deleted); **Show discarded** shows them again. |
| **Ask Codex with it...** / **Ask Codex with this concept...** | Picks the variant if needed and opens Ask Codex, which attaches the pick as `captures/ref-concept.jpg`. |

An armour set shares one concept, filed under its body armour; the spell books that share one texture
share one concept too. Batches live in `out/item-concepts/` (never committed).

**Testing without spending:** with `MU_OPENAI_API_BASE=http://127.0.0.1:<port>/v1` in the editor's
environment, runs go to a local test server instead of OpenAI (only `http://127.0.0.1` and `localhost`
are accepted); the dialogs then say "Test mode".

## Ask Codex

**Ask Codex...** (under the selected item's preview in Browse) asks the art builder (Codex, following
[`ASTRA.md`](../../../../ASTRA.md)) to rework the item's look in place: same group and index, same file
names. The contract Codex works to is
[`assets-work/Items/requests/README.md`](../../../../assets-work/Items/requests/README.md).

1. **Kind:** **upscale** (same design, better textures), **repaint** (new colours or materials, same
   mesh), **remodel** (better geometry, same silhouette), **redesign** (a new look for the same item),
   or **set** (armour only, and chosen for you on an armour part: every part of its armour set together,
   with the kind for the parts under **Every part**). Upscale and repaint let Codex replace textures only;
   remodel and redesign also the models.
2. **Priority**, a one-sentence **Summary** (it also names the request), and one point per line under
   **What to change**, **Keep** and **Avoid**.
3. **Codex may push its branch and open a PR** records your permission for that worker branch (never a
   merge); off by default.
4. **Include captures** (on): the editor takes pictures of the preview for Codex, without any editor
   panel or text: the turntable from the front (the broad face: a blade's flat, a shield's face, the
   front of armour and wings), side, back and three-quarter, the inventory slot, the
   item worn by a character (when it is drawn on the body), and +0 / +9 / +13 excellent (when the item
   can be excellent; else +9 and +13). They are 1024 x 1024 JPEGs, named `captures/01-front.jpg`,
   `02-side.jpg`, ... The preview runs through them in about a second (the dialog shows the progress) and
   then goes back to your view, level and options.
5. **Reference images:** the item's picked concept (`assets-work/Items/concepts/<key>/concept.jpg`, from
   **Pick** in the [Concepts](#concepts) section) is shown as a thumbnail and included as
   `captures/ref-concept.jpg` unless you untick it. **Add image...** adds any JPEG as `ref-01.jpg`,
   `ref-02.jpg`, ... (wider than 1920 pixels: scaled down).
   **With a picked concept the form starts filled in** for building the item as the concept shows
   it: kind **redesign** (for a set, **Every part** redesign), the summary "Rebuild <item> exactly as
   the picked concept", and What to change / Keep / Avoid lines that say so. Edit or delete them
   for anything else. Without a concept the fields start empty.
6. **Scope** lists the files Codex may replace and the textures it must leave alone because other items
   or models use them too.

Warnings above **Create**: your checkout is on a commit no branch on origin has (the request records it
as `base_commit`; push first or file from an up-to-date `main`), a model file differs from the catalog
(an uncommitted change, or the catalog is out of date), or the item already has an open request.
**Create** is refused while a model file is missing.

**Create** writes the whole folder `assets-work/Items/requests/<date>-<key>-<words>/` (`request.json`,
`brief.md`, `captures/`) at once, then checks it with `validate_request.py` when `python3` is installed.
If the check fails, the folder is removed again and the dialog shows the validator's errors next to your
text. When it passes (or no Python was found; the dialog says so), the dialog shows the commands that
hand the request to Codex, with **Copy commands**:

```sh
git add assets-work/Items/requests/<id>
git commit -m "docs(assets): file item request <id>"
git push
```

Run them on `main` (from another branch, cherry-pick the commit onto `main`). Codex only sees a request
that is on `origin/main`; the coordinator then assigns it to a worker. The editor never commits or
pushes by itself.

### Your verdict

**Looks good** / **Needs work** (with an optional note) record what you think of the item as the client
draws it in `assets-work/Items/client-review.json` (`{"<key>": {"verdict", "note", "date"}}`); the
catalog builder folds it into the catalog. A verdict is not a request. Commit the file when you want to
keep it.

## Requests

The third tab lists every folder under `assets-work/Items/requests/`: request, item, kind, status
(open, claimed, delivered, accepted, rejected, withdrawn), your verdict on the delivery, when it was
filed, the worker branch and whether a `delivery/` folder is there. It follows the folders by itself (a
check every second and a half, reading only file times); **Refresh** reads them at once. Clicking a
row selects its item, as in Browse and the Stats table; **Show in Browse** goes there.

For the selected request:

| Button | When | What it writes | Then |
|--------|------|----------------|------|
| **Withdraw...** | open or claimed | In `request.json` only `status` `withdrawn`, a `decision` and one `status_history` entry, with your optional reason. | Commit and push on `main` (the commands are shown). |
| **Accept** / **Reject with notes** | delivered (the worker's branch pulled) | `owner-decision.json` next to `request.json`: `{"verdict": "accept" or "reject", "notes", "date"}`. Nothing else. | Commit it on the worker branch and push it; the coordinator accepts or rejects the request on `main`. |
| **Re-file with notes...** | rejected, or your verdict was reject | Nothing yet: opens Ask Codex for the item with the same kind and notes, the rejection notes added to **What to change**, and the new request superseding the old one. | As for any new request. |
| **Validate** | any | Nothing: runs `validate_request.py` on the folder and shows its report. | |
| **Open folder** | any | Nothing: shows the folder in Finder. | |

A request folder whose `request.json` cannot be read is listed as "unreadable".

## Stats table

The table of every item field:

1. **Find an item:** type in **Search** (part of the name, any letter case, also outside A-Z).
   **Columns** picks which fields are shown; the choice is kept in `MuEditor/MuEditor.ini`. **Freeze
   Index/Name** keeps those two columns in view while you scroll sideways.
2. **Edit:** click a cell and type. The change is live at once (the running game uses the same
   table) and is logged in the Editor Console. A name holds at most 29 bytes (letters outside
   A-Z take two or more); the cell stops there. Browse shows the edit the next time you switch to it.
3. **Save:** **Save Items** writes the game's file and your checkout's
   `src/bin/Data/Local/Eng/item_eng.bmd`, and first copies the file it replaces to
   `out/editor-backups/<time>/`. The lines under the buttons list the paths; the Editor Console lists
   every changed field. Nothing edited means nothing is written.
4. **Commit or undo** like code: `git status` shows `src/bin/Data/Local/Eng/item_eng.bmd` modified;
   `git add` + `git commit` keeps it, `git checkout -- src/bin/Data/Local/Eng/item_eng.bmd` throws the
   edit away. The game's copy keeps the edit until the next build copies `src/bin/Data` over it
   again (the quick start's build command), so rebuild before you start the client again.
5. **Copy a row:** select it (click any of its cells), then **Cmd+C** (Ctrl+C on Windows) copies it as
   `Field = value` text plus a CSV line, for pasting into a note or a request.

**Stats and names are only half of it:** the server (OpenMU) sends just group and number for each
item. Size, class and requirements must match the server's item definitions, or the game shows items
it cannot place or equip; names, damage and prices on the client are display only. A change you want
to keep in play must be made in OpenMU too (admin panel, Items).

### Files

| What | Where | Notes |
|------|-------|-------|
| Launcher | `out/build/<preset>/src/<Config>/MU Item Editor.app` | Made by editor builds on macOS; runs `Main --editor --items` from `Main.app/Contents/MacOS` (the game reads `Data/` from there). |
| Item catalog (Browse) | `assets-work/Items/catalog.json` in the checkout | Tiers, families, model files, originals, requests; generated, never edited by hand. |
| Item requests (Ask Codex, Requests) | `assets-work/Items/requests/<id>/` | `request.json`, `brief.md`, `captures/*.jpg`, `owner-decision.json` after Accept / Reject; commit and push them yourself. |
| Your verdicts | `assets-work/Items/client-review.json` | Written by Looks good / Needs work; created on the first verdict. |
| Picked concepts | `assets-work/Items/concepts/<key>/concept.jpg` | Offered as the request's `captures/ref-concept.jpg`. |
| Original item files (A/B) | `out/ab/original/Data/...`, `out/ab/original/items-manifest.json` | Built by `materialize_variant.py original --items`; not in git. |
| A/B sheets | `out/item-ab/<key>-<YYYYMMDD-HHMMSS>/` | **Capture A/B sheet**; not in git. |
| Model files (status) | `src/bin/Data/Item/...`, `src/bin/Data/Player/...` in the checkout | Hashed and compared with the catalog's originals. |
| Item table the game loads | `Data/Local/<lang>/Item_<lang>.bmd` next to the executable | `<lang>` is `LanguageSelection` in `config.ini` (`Eng`). On disk the file is `item_eng.bmd`; the editor saves under that spelling. |
| Repository copy | `src/bin/Data/Local/Eng/item_eng.bmd` | Written on every save; this is the file git tracks. |
| Backup of the repo file | `out/editor-backups/<YYYYMMDD-HHMMSS>/Data/Local/Eng/item_eng.bmd` | Taken before the repo file is replaced; `out/` is not in git. |
| Backups next to the game's file | `Data/Local/Eng/item_eng.bmd_N01_Y....bak` | The data layer's own last-ten backups, inside the build output only. |
| **Export as S6E3** | `Data/Local/<lang>/Item_S6E3.bmd`, copy in `out/editor-exports/` | The table in the legacy S6E3 layout for other tools; the game does not read it. |
| **Export as CSV** | `Data/Local/<lang>/Item.csv`, copy in `out/editor-exports/` | UTF-8 with BOM, one row per named item, field names as in the code. |
| Column choice | `MuEditor/MuEditor.ini`, section `[ColumnVisibility]` | Next to the executable. |
| UI size, full screen | `MuEditor/MuEditor.ini`, `[General]` `UIScale`, `StudioFullscreen` | Written when you press - / + or switch full screen. |
| Concept batches | `out/item-concepts/<batch>/`, reference renders in `out/item-concepts/refs/` | Not in git. |
| Logs | `MuError.log`, `MuEditor/MuEditor_YYYYMMDD.log` | The start (`[Editor] Opened the Item Editor studio ...`), the save's paths and the changed fields. |

Without a checkout above the game folder (a copied `Main.app`), a save keeps a copy next to the
executable instead (`Local/Eng/item_eng.bmd`) and Browse has no catalog;
`MU_EDITOR_REPO_ROOT=/path/to/repo` points the editor at a checkout. See
[Where saves go](../MapEditor/MAP_EDITOR.md#where-saves-go).

### Gotchas

- **The file format is kept as loaded.** The shipped tables use the old S6E3 layout (84-byte records,
  30-byte names), detected by the file size. A save writes the same layout back, and every item you
  did not change keeps its exact bytes, so saving without an edit reproduces the file byte for byte
  and a one-name edit changes only that record and the checksum. (A save used to convert the file to
  the newer 50-byte-name layout.)
- **Three ticket names are longer than the old layout allows** ("Open Access Ticket to Chaos
  Castle", "... Doppelganger", "... Varka Map 7"): in the shipped file they run into the fields
  behind the name, so those tickets' Two-Hand and Level values are really letters. The cell shows the
  first 29 bytes; they only change when you edit them.
- **The Index column moves an item:** typing a free index there moves the whole row to that slot
  (the group is `index / 512`). An index already in use is refused.
- **Only the running language's table is edited.** `Por` and `Spn` have their own
  `item_por.bmd` / `item_spn.bmd`, and `Data/Local/Item.bmd` is an older table the game does not load.
- **Browse reads the catalog once per start.** After rebuilding `catalog.json`, restart the editor
  (requests and verdicts are read live);
  **Rescan files** only hashes the model files again.
- **The preview follows the loaded map.** Ground and light come from the map at the start point:
  Lorencia's town floor in the studio, the map you opened with `--items --world N`. The Ground and
  Equipped views therefore look different on other maps, as in the game.
- **Some effects are not in the preview:** effects the game draws as separate world effects (joints,
  model effects such as the flame trails of 3rd wings) and pets/mounts. With the map drawn
  (`--items --world N`, or the Map Editor open) the particles the preview's character makes appear in
  the map at the start point instead of in the preview.
- **The pointer in the studio** is the system's arrow everywhere (over the preview too); the game's own
  cursor is drawn only when the map is shown without the studio (`--world N`).
- **Armour on the turntable and on the ground** uses the male skeleton, as the game does for a
  dropped part; worn armour uses the chosen class's body.
