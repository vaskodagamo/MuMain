# Editing and growing maps as an AI agent

This is the one page an agent (Claude, Codex, or a script) follows when the owner says "expand
Lorencia to the east with a forest field" or "put a small hill with a grove east of the town".
The agent looks at the map, plans the change as JSON **edit scripts**, sketches them on a top-down
picture, dry-runs them, applies them as undoable steps, looks again from the same angles, fixes
what is off and hands the result back with before and after pictures. It can also add new maps
joined by gates, and prepare (never apply) what the OpenMU server needs.

Everything goes through the developer control socket of an editor build
([`docs/control-socket.md`](../control-socket.md), "Editor commands"), driven with
[`tools/world_editor/mapctl.py`](../../tools/world_editor/mapctl.py). The Map Editor panel shares
the same undo history, so the owner can take a scripted step back with Cmd+Z and the agent can
undo the owner's steps. Nothing reaches the map's files until someone saves.

Contents: [1 What the owner types](#1-what-the-owner-types),
[2 Ground rules](#2-ground-rules), [3 Setting up](#3-setting-up), [4 The loop](#4-the-loop),
[5 Design rules for MU maps](#5-design-rules-for-mu-maps),
[6 Growing the world](#6-growing-the-world-new-maps-and-gates),
[7 Handing back](#7-handing-back-to-the-owner), [8 The server side](#8-the-server-side-openmu),
reference: [9 Coordinates](#9-coordinates-and-units), [10 The edit script](#10-the-edit-script),
[11 Commands](#11-commands), [12 A worked example](#12-a-worked-example),
[13 Limits and gotchas](#13-limits-and-gotchas)

## 1. What the owner types

A prompt template for Claude or Codex; copy it, fill in the brackets, delete what does not matter:

```text
Map job, follow docs/agents/AI_MAP_EDITING.md.
Map:       [Lorencia (world 1) | a new map east of Lorencia]
Where:     [tiles x0 y0 x1 y1 | "the open grass east of the town"]
What:      [the change in plain words: a small hill with a grove and a gravel path to the road]
Keep:      [what must stay as it is: the paved road, the camp with the fire, the gates]
Style:     [sparse or dense, models or textures you want, mood of the light]
Hand back: [leave the editor open, unsaved, for my review | save on a branch and open a PR]
Server:    [no server changes | prepare the OpenMU export; I apply it]
```

A short prompt works too ("expand Lorencia to the east with a forest field, leave it open for me");
the agent takes these defaults and says which it used:

- **Hand back:** the editor stays open with the edits applied and **unsaved** (section 7).
- **Server:** nothing is applied; when gates or walkability change, the agent prepares the export
  and tells the owner what to upload (section 8).
- **Style:** like the nearest similar area of the same map (its models, textures, density, scale
  and light).

What the agent does with it, in this order:

1. Reads this page; builds the editor preset if needed; starts its **own** client with
   `mapctl.py launch` (section 3).
2. Looks at the area: a layer export, a top-down shot, two or three shots from the player's angles,
   `query` numbers, the gates and the safe zone around it.
3. Plans the change as scripts, one per layer, sketches them on the top-down picture, and checks
   them against the design rules (section 5). What the rules or the map's models do not allow, it
   says instead of improvising.
4. Dry-runs, applies layer by layer, looks from the same angles after each, fixes.
5. Hands back as asked, with a summary (template in section 7): what changed in numbers, before and
   after pictures, the undo steps, what the owner still has to do, and what it could not do.

It never saves, commits, pushes, opens a PR or touches the server unless the prompt asks for it.

## 2. Ground rules

**Free to do** (no need to ask): build the editor preset; launch and quit your own client; every
reading command (`info`, `query`, `export`, `camera`, `shot`, `sketch`, `gates`, `history`); dry
runs; `apply`, `undo`, `redo` and `revert` on the loaded map (all in memory; the owner can undo
every step except a `revert`, which only drops unsaved work).

**Only when the prompt or the owner says so** (ask in chat otherwise, per action):

- `save`: it writes the game's `Data` and the repository's `src/bin/Data`, files git tracks.
- `new-map`, `gate-add`, `gate-remove`: they write files at once (new folders, `Gate.bmd` for every
  map); a prompt that asks for a new map or a gate is the permission for exactly that. Dry-run first.
- `git commit`, `git push`, a pull request: only on the fork `vaskodagamo/MuMain` (`gh ... --repo
  vaskodagamo/MuMain`, base `main`), never on `upstream` (`sven-n/MuMain`); see
  [`AGENTS.md`](../../AGENTS.md).
- Opening another map in a client that holds unsaved edits (`open --discard` drops them).

**Never:** log in to a game server with the client; stop Docker or touch OpenMU's database, files or
Admin Panel (the owner applies the server side, section 8); kill a client you did not start (the
owner may run their own: use `mapctl.py quit` on your socket, or kill your own PID); edit files under
`src/bin/Data` by hand; place models or textures the map does not have (section 5, rule 9); change
the client's environment beyond `MU_CONTROL_SOCKET` (leave the Metal validation variables alone).

## 3. Setting up

**Build** the editor client once (repository root; the first build takes several minutes; details
in [`MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md)):

```sh
PATH=/opt/homebrew/bin:$PATH cmake --preset macos-arm64-mueditor -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
PATH=/opt/homebrew/bin:$PATH cmake --build --preset macos-arm64-mueditor-release --target Main
```

The `-mueditor` presets turn the control socket on (`ENABLE_CONTROL_SOCKET`); the player presets do
not have it. Windows and Linux have `windows-x64-mueditor` and `linux-x64-mueditor`; `mapctl.py`
needs a Python with `AF_UNIX` sockets (macOS, Linux; CPython on Windows has none).

**Launch** your client offline on a map (`--world` is the `Data/World` folder, Lorencia is 1):

```sh
python3 tools/world_editor/mapctl.py launch --world 1
```

It takes the editor build under `out/build` (this platform's `-mueditor` preset, Release first, or
`--main PATH`), starts it with `MU_CONTROL_SOCKET` set to the default socket, waits until the map
is loaded (about 2 s on the owner's Mac) and prints `pid`, `socket`, `log` (the client's stdout,
in `out/mapctl/`) and `client_log` (`MuError.log` beside `Main`). The default socket is
`mu-mapctl.sock` in a folder only you can use: `$XDG_RUNTIME_DIR` (Linux), else
`/tmp/mu-<your uid>/`, which `launch` creates with mode 0700; mapctl talks only to a socket file
your user owns. `launch` refuses when anything already listens on that socket, also a client too
busy to answer, and stops its own client again if another one (another pid) answers there. The
window opens on the owner's screen; the client keeps answering and capturing while it is hidden,
minimized or covered, at the pace a visible window would have (so a hidden client neither spins
the CPU nor piles up GPU memory), so there is no need to touch it. A sandboxed agent shell
(Claude Code) has to run `launch` and every command that talks to the socket with the sandbox off.

**Every command** prints the client's answer as JSON and exits 0; exit 1 is a refusal by the client
(`{"ok": false, "error": ..., "message": ...}`, the message names the problem), 2 bad arguments or
input files (also printed as JSON, `"error": "bad_arguments"`), 3 no client on the socket or no
answer in time. `--socket PATH` (or `MU_CONTROL_SOCKET` in the same command line) picks another
client; two agents at once use different sockets. `--compact` prints one line; `--timeout` waits
longer (default 60 s). `python3 tools/world_editor/mapctl.py <command> --help`
lists a command's options. Below, `mapctl` stands for `python3 tools/world_editor/mapctl.py`, run
from the repository root; paths it passes to the client are made absolute first.

| mapctl | Socket command | What for |
|---|---|---|
| `launch --world N`, `quit` | (`quit`) | start your client and wait for it; close it and wait until its process has ended (`exited`) |
| `ping`, `info [--models]` | `ping`, `map-info` | is it there (and its `pid`); which map, objects, gates, unsaved files, and with `--models` every model with its type and names |
| `open --world N` / `--map N` | `map-open` | switch maps (refused while the loaded map has unsaved edits; `--discard`) |
| `camera --tile X Y [--yaw --pitch --distance/--height]`, `camera --topdown X0 Y0 X1 Y1` | `map-camera` | point the camera |
| `shot OUT.png [--topdown R] [--region R] [--overlay]` | `map-camera` + `screenshot` | a clean PNG (no editor, HUD or cursor); `--topdown` frames and crops a rectangle; a frame that could not be read back is asked for again (twice) |
| `export --out DIR [--rect R] [--layers ...]` | `map-export` | layer PNGs (one pixel per tile), `legend.json`, `objects.json` |
| `query --rect R` / `--tile X Y` | `map-query` | heights, walkability and texture counts, objects |
| `sketch IMAGE.png --out OUT.png [--rect R] [--script S.json ...]` | (local) | a labelled tile grid and the scripts' shapes on a top-down image |
| `dry-run S.json`, `apply S.json` | `map-apply` | try a script; apply it as one undo step |
| `undo`, `redo`, `history` | `map-undo`, `map-redo`, `map-history` | the shared undo history |
| `save [FILE ...]`, `revert FILE ...` | `map-save`, `map-revert` | write the map's files; read them back |
| `new-map`, `gates`, `gate-add`, `gate-remove`, `gate-show` | `map-new`, `gate-*` | new maps and gates (section 6) |
| `tab NAME` | `map-tab` | switch the Map Editor's tab (`tab texture` after `gate-show`: the Gates tab paints the ground) |
| `server-export` | `map-server-export` | the OpenMU files (section 8) |
| `send '{"cmd": ...}'` | any | anything else, as raw JSON |

`mapctl` is also a Python module (`mapctl.Client(mapctl.default_socket_path()).call('map-info')`),
and without it the socket is one JSON object per line each way (section 11).

If a command answers `unreachable` although your client was running, it has quit: the last lines of
`MuError.log` beside `Main` say why (`Quit requested: ...`: the socket's `quit`, a closed window,
or the system's Quit, such as Cmd+Q typed while the client's window had the focus). Unsaved edits
are gone with it; when the job asks for saves, save after every layer (section 4.5).

## 4. The loop

Keep your files in a work folder of the repository, called `$W` below:
`out/map-edits/<YYYY-MM-DD>-<slug>/` (git ignores `out/`), with `scripts/`, `export/`, `before/`,
`after/` and `sketches/`. The scripts are the record of the change: keep every one you applied.

### 4.1 Orient

```sh
mapctl info                                      # map, objects, gates, unsaved flags (all false?)
mapctl shot $W/before/map.png --topdown 0 0 255 255
mapctl export --out $W/export-map --layers attribute texture1 texture2 objects
mapctl sketch $W/export-map/attribute.png --out $W/sketches/map-grid.png --grid 20
```

Look at the whole-map picture and the gridded walkability image (read the PNGs): the town (green
safe zone), roads (layer 2 in `texture2.png`), water, walls, the gates (`info` lists their areas).
Pick the working rectangle `R` with a margin of a few tiles around the change. If `unsaved` is not
all false in a client someone else used, stop and ask.

### 4.2 Observe the area

```sh
mapctl export --out $W/export --rect 195 110 235 150      # legend.json names texture slots and values
mapctl query --rect 195 110 235 150                        # numbers, and the objects standing there
mapctl shot $W/before/top.png --topdown 195 110 235 150
mapctl camera --tile 215 130 --yaw -45 --pitch 45 --distance 3000 && mapctl shot $W/before/player.png
mapctl camera --tile 215 130 --yaw 135 --pitch 30 --distance 2500 && mapctl shot $W/before/side.png
```

- **Poses:** the top-down shot, the player's angle (yaw -45, pitch 45: the direction the game camera
  looks; 2000 to 4000 units away) and a low view from another side. Write the poses down; the after
  shots use exactly the same ones.
- **Look at the pictures.** Names hide a lot: `TileGround01` on Lorencia is paving stones, not dirt.
  Check the textures, models and scales of the nearest similar area (`query` it: `objects` lists
  model `name`, `scale`, `height_above_ground`), and what already stands where you plan to build.
- Note what must be kept: roads, gate areas, the camp, the anti-tamper tile (section 5, rule 6).

### 4.3 Plan and sketch

- Split the change into **layers, one script each**, applied in this order: `1-terrain.json`,
  `2-textures.json`, `3-walkability.json`, `4-objects.json`, `5-light.json` (leave out what the job
  does not need). Objects follow the ground and scatter reads slope, walkability and textures from
  the map as it stands, so the ground comes first; the light is tuned last, on the finished scene.
  Each script is one undo step: label it `"<feature> 1/5 terrain: ..."` so the history reads well.
- Take model names from `objects.json` or `query` (`name`), texture names from `legend.json`; the
  error for an unknown name lists what the map has loaded. Reuse scale ranges and densities from the
  area you imitate (section 5, rule 7).
- **Sketch** the scripts over a top-down picture, then look at the sketch:

  ```sh
  mapctl sketch $W/export/texture2.png --script $W/scripts/1-terrain.json --script $W/scripts/4-objects.json \
      --out $W/sketches/plan.png                           # an export layer: its legend gives the tiles
  mapctl sketch $W/before/top.png --rect 195 110 235 150 --script $W/scripts/4-objects.json \
      --out $W/sketches/objects.png                        # a top-down shot: say which tiles it shows
  ```

  It draws a grid labelled in tiles and every op's shape with the op's number (ops are numbered on
  across several `--script`s): terrain yellow, textures magenta, walkability red, light orange,
  objects cyan (a placed object is a square), selections green, scatter `avoid` areas white. Export
  images (one pixel per tile) are enlarged up to 16 times, towards 1024 pixels. Check: does the path meet the existing road? Do
  the avoid areas cover the road and the gates? Does anything touch a gate or the anti-tamper tile?
- Go through the design rules (section 5) before the first dry run.

### 4.4 Dry run

```sh
mapctl dry-run $W/scripts/1-terrain.json
```

Nothing changes; the answer says what would: per op its `area`, per layer the changed `cells` and
their bounding `area`, objects `added`, `removed` and `changed` (the first 200 listed with model,
tile and transform), each scatter's `requested` and `placed`, and `warnings`. A mistake refuses the
whole script and names the field (`ops[3].shape.radius: is a number from 0.1 to 256`). Iterate until
the numbers match the intent: cells only inside `R`, no objects changed where you did not expect
them, a scatter placing what you asked for.

### 4.5 Apply, layer by layer

```sh
mapctl apply $W/scripts/1-terrain.json && mapctl shot $W/after/top-1.png --topdown 195 110 235 150
```

After each layer: keep the answer (labels, cells, ids of placed objects), take the top-down shot and
look at it before the next layer. `history` lists the steps. When the job asks for the result to be
saved, `save` after each layer that looks right: a client that quits (section 3) loses what is not
saved.

### 4.6 Look

Take the after shots from the same poses as the before shots, then check with numbers:

- `query --rect <road>`: only walkable (0) or, in a town, safezone (1) tiles, and no trees or rocks
  among its `objects`.
- `query --rect <gate area>` for every gate near the change (`info` lists them): walkable, nothing
  standing in it.
- the anti-tamper tile (on Lorencia `query --tile 135 123` still reports 5).
- `export --layers attribute --rect R` and look at it: tree trunks blocked if you marked them, no
  stray blocked tiles on paths.
- `info`: the object count went up by what you placed.

### 4.7 Fix

- `undo` the step and apply a corrected script (a new apply drops the redo steps). Undo goes back in
  order: to change layer 2 after layer 4, undo 4, 3 and 2, fix 2, apply 2, 3 and 4 again from their
  files.
- Or apply a small fix script on top, such as an `object.delete` with `select.inside` a circle where
  a tree blocks a path. Object ids move after deletes, saves and reverts: select by `inside` and
  `model`, or `query` again for fresh ids.

### 4.8 Hand back

Section 7: leave it open for review, or save on a branch with the owner's permission. Then quit your
client, unless the owner is going to review in it.

## 5. Design rules for MU maps

1. **Walkability matches what stands there.** Tiles under tree trunks, rocks, walls and buildings
   are blocked (4); grass, flowers, small stones and decals stay walkable. Scatter trees with
   `"mark_attribute": "blocked"`; for objects already placed, `attribute.set` with `"under":
   {"model": ...}` (or `{"placed_by": n}`) blocks the tiles they stand on; under a building or a big
   rock set a blocked `rect` over its footprint. A player must never walk through a solid object, nor
   be stopped on open ground. Lorencia's own convention, measured on its 2870 objects (share of each
   model standing on a blocked tile):

   | Lorencia models | On blocked tiles | Treat as |
   |---|---|---|
   | Tree01, 02, 03, 04, 06, 08, 11, 12; Stone01, 02, 03; StoneStatue*, Tomb*, TreasureDrum01, TreasureChest01, Carriage*, Straw*, Bonfire01, Cannon*, House*, HouseWall01/02/04, HouseEtc*, StoneMuWall02-04, Fence02, Sign01, Well02/03 | 85 to 100 % | solid: block the trunk or footprint tile |
   | Tree07, Tree09, Tree10 (bushes and saplings), Grass01-08, BridgeStone01, Furniture06/07 | 0 to 30 % | walkable |
   | Tree05, Tree13, Stone04, Tent01, Fence01/03/04, Steel and Stone walls | about half | depends on the spot: block what a player should not pass |
   | FireLight01/02, Light01-03, PoseBox01 | (effects) | flames, light sources and sit markers, not things of their own: place them only with the post, lamp or bench they belong to (FireLight02 is the floating flame of the palisade posts) |
2. **Roads stay walkable and free.** Every road and path tile is walkable (0, or 1 in a town). Give
   every scatter an `avoid.areas` path 2 to 3 tiles wider than the road, never mark attributes on a
   road, and connect a new path to an existing road or square, not into a wall or the moat.
3. **Safe zones.** Towns are safe zones (1): no fighting there, and OpenMU's monsters neither walk
   into them nor spawn in them. Keep a safe zone compact and bounded by walls or clear edges (a town,
   a camp); hunting ground is walkable (0). Paint only the clean values walkable 0, safezone 1,
   blocked 4, void 8 and water 16, never combinations: the client and OpenMU read combined values
   differently ([`MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md), gotcha 12). Water
   (16) is walkable in the client but blocked on OpenMU: for water nobody should enter, use blocked.
4. **Do not cover gates.** Keep every gate area (`info`, `gates`) and two tiles around it walkable,
   free of objects and on even ground; an arrival area must never lie inside an enter gate.
5. **Client and server walk maps agree.** Every walkability change on a game map needs the server's
   copy too (section 8); until the owner uploads it, OpenMU lets players walk through the new trees
   and walls and pulls them back where the client lets them walk (rubber-banding). Say so in the
   summary.
6. **Anti-tamper tiles.** The client closes a map whose walk map changed one magic tile: Lorencia
   (135, 123) = 5, Dungeon (227, 120) = 4, Devias (208, 55) = 5, Noria (186, 119) = 5, Lost Tower
   (193, 75) = 5. Scripts that would change it are refused; keep it out of your shapes.
7. **Density and performance.** Lorencia holds 2870 objects; its busiest 16 x 16-tile block (the
   engine's culling unit) holds 82 of them, its densest woods about 9 trees per 10 x 10 tiles and
   15 per block. Stay near that: trees at a density of 0.05 to 0.1 per tile with `min_spacing` 2 or
   more, small decorations denser but not in the hundreds per block. The object file holds at most
   32767 objects and one scatter places at most 5000.
8. **Heights.** A map stores 0 to 382.5 in steps of 1.5. The game does not block steep ground by
   itself: where you make a cliff or a steep bank a player should not climb, block its tiles. Soften
   new hills with `terrain.smooth` and soft edges so they meet the old ground without steps; use
   ramps (`terrain.ramp`) for paths up a slope.
9. **Only what exists.** Place only models and textures this map has loaded. When the owner asks for
   something the map does not have (a windmill, a snow texture), do not fake it with an unrelated
   model: say so, offer the closest existing one, and point to the art pipeline. Reworking an existing
   model is a regeneration request (the Map Editor's Assets tab, **Flag for regeneration...**,
   [`assets-work/World1/requests/README.md`](../../assets-work/World1/requests/README.md)); a
   brand-new model is art work for the owner to commission ([`ASTRA.md`](../../ASTRA.md)).
   Placement, terrain and walkability are never art requests.
10. **Match the surroundings.** Textures, models, scale ranges and light of the nearest similar area;
    soft transitions (falloff on terrain and layer-2 paint); gentle light (`light.tint` strength 0.4
    or less; strong light shows as stains on the ground). The painted light map is what shades the
    ground: the engine itself darkens only steep slopes that face away from its south-east sun, so
    new hills look flat until `light.bake` shades them (section 10, Light).
11. **Borders.** Leave a map's outer border as the game has it. On a new map, block a rim of a few
    tiles: beyond the edge nothing is drawn, and a player there looks into the void. A road should
    end somewhere (a camp, a gate, a clearing), not run into the rim.
12. **Water and bridges.** Layer 1 has one texture per tile, so water painted there has stair-stepped
    edges. What worked on map 82: water on layer 1 a little narrower than the river bed, a rocky or
    grassy layer-2 overlay over both banks that reaches a tile into the water, the overlay erased
    (`texture.erase`) in the open channel, and the river blocked (4). An overlay over a bridge deck
    hides its planks: keep overlays off the deck, and keep the deck and its approaches walkable.

## 6. Growing the world: new maps and gates

**Why not a bigger map:** a map is always 256 x 256 tiles. The game sends every position as one
byte, the terrain files are fixed 256 x 256 arrays, and the map number itself is a byte, so a map
cannot grow. The world grows the way MU always grew: new maps (82 to 254) joined to the others by
gates. The commands work in any offline session and write files at once (no undo step): new map
folders and `Gate.bmd`.

**Numbers.** `map` is the number OpenMU and `Gate.bmd` use (82 to 254; the game's own maps are 0 to
81); `world` is its `Data/World` folder, `map + 1`. Map 82 lives in `Data/World83` and
`Data/Object83` and opens with `mapctl open --world 83` (or `--map 82`). Every map reference is
`{"map": N}` or `{"world": N}`, and every answer names both. Custom gates take numbers 345 to 511.

**Planning a new map:**

1. **Template or flat.** A copy of a game map brings its whole look but loses what the game ties to
   that map number in code: a copy of Lorencia has no fires or street lights, some house and
   carriage parts draw black, and every new map is silent with black fog. A flat map with the tile
   set (`textures_from`) and models (`models_from`) of a similar map is a clean start that you shape
   with scripts (section 4). A flat map's light is flat too: after the terrain, `light.bake` over the
   whole map (then your mood light on top) makes its hills visible. On the new map the models go by
   the model files' own names (`Tree01.smd`, `Data2\Object1\treea_10.smd`), not the source map's
   catalog names; their type numbers are the source map's, so a script written for Lorencia works
   with types (`"model": 10` for Lorencia's Tree11). `mapctl info --models` lists every model with
   its type and names.
2. **Where the way in goes.** On the source map, at the edge of its walkable ground, on open, level
   tiles: `query` a strip to find where walking ends (Lorencia's east side is walkable up to
   x = 246; x = 247 is blocked). Usually out in the hunting ground, as the game's own gates are, not
   in the middle of a town or on a road junction.
3. **The arrival** on the new map lies a few tiles in from the edge, walkable, facing into the map
   (`dir` is the way players face when they arrive); the way back is an enter gate between the
   arrival and the edge, so a player who arrives is not sent straight back. `gate-add` refuses an
   arrival without a walkable tile and one inside its own enter area (players would be stuck or
   bounce without end) unless you pass `--allow-trap`. Add both directions.
4. **Shape it** with the loop: ground, textures, a blocked rim (rule 11), paths from the arrival,
   objects, light. A small camp by the arrival can be a safe zone.
5. **Keep it:** `save` (with the owner's permission, section 7) and prepare the server files
   (section 8).

With mapctl (the socket commands and their answers follow):

```sh
mapctl new-map --map 82 --name "Lorencia Outskirts" --blank --height 150 --texture TileGrass01 \
    --light 0.85 --textures-from-world 1 --models-from-world 1 --dry-run
mapctl new-map --map 82 --name "Lorencia Outskirts" --blank --height 150 --texture TileGrass01 \
    --light 0.85 --textures-from-world 1 --models-from-world 1
mapctl query --rect 236 88 250 101                       # on Lorencia: where does walking end?
mapctl gate-add --from-map 0 --from-rect 245 92 246 97 --to-map 82 --to-rect 4 120 6 124 --dir east --dry-run
mapctl gate-add --from-map 0 --from-rect 245 92 246 97 --to-map 82 --to-rect 4 120 6 124 --dir east
mapctl gate-add --from-map 82 --from-rect 1 120 2 124 --to-map 0 --to-rect 240 93 242 96 --dir west
mapctl open --map 82                                     # shape it with the loop
mapctl gate-show 347                                     # show the owner the way back in the Gates tab
mapctl shot $W/after/gates.png --topdown 0 110 20 130    # the gate areas are drawn while that tab is open
```

**`map-new`** `{"map": 82, "name": "...", "from": ..., "models_from": {...}, "minimap": true, "dry_run": false}`:

- `from: {"template": {"world": 1}}` copies a map: ground, heights, walkability, light, objects,
  textures and (with `minimap`, default true) its minimap. Models come from the template unless
  `models_from` names another map. Lorencia's named models are copied under the numbered names of
  their types, so its objects keep their models.
- `from: {"blank": {"height": 150, "texture": "TileGrass01", "attribute": "walkable", "light": 0.85,
  "textures_from": {"world": 1}}}` makes flat ground (all fields optional: height 0 to 382.5, a
  texture slot or name of the tile set, a clean walkability value, a light 0 to 1 or [r, g, b]; the
  tile set of `textures_from`, default Lorencia's, is copied). No models unless `models_from`.
- The answer: `map`, `world`, `folders`, `world_files`, `file_count`, `warnings` (what does not come
  along, missing models), `repo_results` (`{"created": 35}`) and `open_with`
  (`{"cmd": "map-open", "world": 83}`). `dry_run` writes nothing. A number that is taken (either
  folder exists in the game's `Data` or in `src/bin/Data`) or outside 82 to 254 is refused.

```json
{"cmd":"map-new","map":82,"name":"Lorencia Outskirts","from":{"blank":{"height":150,"texture":"TileGrass01","light":0.85}}}
{"ok":true,"result":{"map":82,"world":83,"dry_run":false,"folders":["Data/World83","Data/Object83"],
 "world_files":["Data/World83/EncTerrain83.map","Data/World83/EncTerrain83.att",...,"Data/World83/MapName.txt"],
 "file_count":35,"repo_results":{"created":35},"open_with":{"cmd":"map-open","world":83},
 "warnings":["no models were copied: import them with the Map Editor's O. Browse tab ..."],"report":"..."}}
```

**`gate-list`** (`mapctl gates [--map N]`; default: the loaded map): `gates` on the map (`number`,
`kind` enter/arrival/spawn, `custom` for numbers 345 and up, `area`, `level`; an enter gate's
`target` with its map, area and `direction_name`; an arrival's `direction` and `arrivals_from`),
`ways_in` (enter gates on other maps that land here), `free_numbers`, `next_free`.

**`gate-add`** `{"from": {"map": 0, "rect": [245, 92, 246, 97]}, "to": {"map": 82, "rect": [4, 120, 6, 124], "dir": "east"}, "level": 0, "dry_run": false}`:
one way from `from` to `to`; `tile: [x, y]` instead of `rect` for a single tile; `dir` is OpenMU's
direction name (undefined, west, southwest, south, southeast, east, northeast, north, northwest) or
0 to 8: the way players face when they arrive (gate-list says so in `faces`). It takes the two
lowest free numbers (the enter gate, then its arrival), saves `Gate.bmd` (game `Data`,
`src/bin/Data`, backup) and answers `enter`, `arrival`, both `gates`, `saved` and `warnings`: tiles
that are blocked or without ground, tiles the client walks on but OpenMU blocks, and areas that
overlap other gates (an arrival inside an enter gate sends players straight on). Two things are
refused, not warned about, because players would be stuck: an arrival with no walkable tile, and an
arrival that overlaps its own enter area (an endless bounce); `"allow_trap": true`
(`--allow-trap`) adds such a pair anyway. Both maps must exist. For the way back, add a second gate.
Every gate edit first reads `Gate.bmd` from disk: if another client (or a checkout) changed it since
this client loaded it, the edit builds on the file and a warning says so, so two agents never hand
out the same numbers or erase each other's gates.

**`gate-remove`** `{"number": 345, "dry_run": false}`: removes a gate the editor added (345 and up)
and its arrival when no other gate lands there (`removed: [345, 346]`). The game's own gates (0 to
344) and an arrival that an enter gate still uses are refused (remove the enter gate instead).

**`gate-show`** `{"number": 347, "look": true}`: opens the Map Editor on its Gates tab with the
gate selected (and, with `look`, points the camera at it), so the owner sees what you mean. The
gate must lie on the loaded map.

**`map-server-export`**: section 8.

## 7. Handing back to the owner

**A. Leave it open for review** (the default). The client stays open with the edits applied and not
saved. Tell the owner the client's PID and socket, the undo labels, and how to judge it: fly around
(arrow keys, PgUp/PgDn, right-drag), and **Undo**/**Redo** at the top of the Map Editor step through
your layers. To keep it they say "save" (you run `mapctl save`) or press each tab's Save button; to
drop it, Undo or "revert". Do not quit that client.

**B. Save on a branch and open a PR** (only when the prompt or the owner asks for it):

1. Work on a branch of the fork, never on `main`: `git switch -c map/<slug>` from an up-to-date
   `origin/main` (`git fetch origin` first; use a separate worktree if the owner's checkout has work
   in progress, and never reset it).
2. `mapctl save` (the files with unsaved edits; the answer lists every file, its repository copy and
   the backup in `out/editor-backups/`). A new map is saved while it is the loaded map; run
   `mapctl server-export` for it if the server should get it (section 8).
3. `git status` shows the changed `src/bin/Data/World{N}/` files and, after gate edits,
   `src/bin/Data/gate.bmd`: git tracks the gate table under that lowercase name (the client finds it
   in any case), so add it by that name, `git add src/bin/Data/gate.bmd`; `Gate.bmd` would stage
   nothing on macOS and add a second file on Linux. New files under `src/bin` are ignored by git
   until added with `-f`: `git add src/bin/Data/World1/<files>`, and for a new map
   `git add -f src/bin/Data/World83 src/bin/Data/Object83`.
4. The record, next to the data: `assets-work/map-edits/<YYYY-MM-DD>-<slug>/` with `README.md` (the
   summary below), `scripts/` (the scripts you applied) and `captures/` (before and after as JPEG, at
   most 1600 pixels wide: `sips -s format jpeg -s formatOptions 80 -Z 1600 before/top.png --out
   captures/before-top.jpg` on macOS). The repository has no LFS: keep it to a handful of pictures.
5. Commit in the repository's style (`feat(map): a knoll with a grove east of Lorencia's town`,
   ending with the attribution lines your harness asks for), then **ask the owner** before
   `git push -u origin map/<slug>` and `gh pr create --repo vaskodagamo/MuMain --base main`. The PR
   body is the summary, with the pictures linked from the branch
   (`https://github.com/vaskodagamo/MuMain/blob/map/<slug>/assets-work/map-edits/<id>/captures/after-top.jpg?raw=true`).
6. PRs are merged on GitHub; after a merge a local `main` lags behind until it is fetched and
   fast-forwarded, so pull before the next build.

**The summary** (the chat answer, and the text of the PR or the record):

```text
Done: a knoll with a stone, a gravel path from the paved road and a grove of 20 trees, east of the town.
Map: Lorencia (map 0, world 1), area [195, 110, 235, 150].
Steps (undo labels, oldest first): "Knoll 1/3 terrain: ...", "Knoll 2/3 textures: ...", "Knoll 3/3 objects: ...".
Changes: height 190 cells (+90 at the top), gravel overlay 102 cells, 20 trees and 1 stone added
  (trunk tiles blocked: 20), 7 objects moved with the ground, light 145 cells.
Checked: path tiles walkable, gates 1/4/18/21/23/26/102/108 untouched, tile (135, 123) still 5.
Pictures: out/map-edits/2026-09-23-knoll/before/ and after/ (top-down, player view, side view).
State: applied, NOT saved; client pid 68127 on /tmp/mu-501/mu-mapctl.sock.
You: say "save" to keep it, or Undo (3 steps) to drop it. Walkability changed: after saving,
  the server needs its copy (Attribute tab > Save server .att, then the Admin Panel upload).
Not done: -
```

## 8. The server side (OpenMU)

The client and the OpenMU server each keep their own copy of what a map is: its walk map, its gates
and, for a new map, its existence. **The agent prepares; only the owner applies** (or an agent the
owner explicitly gives that job). Never run `openmu.sql`, never open the Admin Panel, never touch
Docker or the database on your own.

- **New map:** save its files first (the export reads the saved walk map and warns about unsaved
  edits), then `mapctl server-export --map 82`. It writes `out/openmu-export/map82/` in the
  repository (or `--out DIR`): `HOWTO.md` (the Admin Panel steps with this map's numbers: create the
  game map with its Terrain Data, host it on the game server, add the exit and enter gates on every
  map involved, restart), `terrain_map82_server.att` (the walk map in OpenMU's layout; without it
  OpenMU lets players walk only on rows y < 128), `map.json`, `gates.json` and `openmu.sql` (the same
  steps as one transaction, marked not applied: the owner reads it, backs the database up and runs
  it with psql, which stops at the first error). The answer says `"applied": false`. `map.json` and
  `gates.json` are for reading while filling in the Admin Panel: its Map Editor's Import button is for
  monster spawns and would delete the map's spawns (the files carry a format of their own, so that
  import stops before it deletes anything).
- **Gates on game maps:** `mapctl server-export --map 0` writes the gates the editor added only (no
  walk map): the game's own gates are on the server already, and some of their Gate.bmd records
  differ from OpenMU's seed, so they are never exported.
- **Walkability changed on a game map:** the server's walk map is exported from the Map Editor's
  Attribute tab (**Save server .att**, which counts scripted tiles as edited) after the client's
  file is saved, and uploaded in the Admin Panel as that map's **Terrain Data**, then the game
  server restarts ([`MAP_EDITOR.md`](../../src/MuEditor/UI/MapEditor/MAP_EDITOR.md), Attribute tab).
  That is a button for the owner; tell them.
- Walking through a new gate needs the server to know it: that test is the owner's, online, after
  they applied the export.

## 9. Coordinates and units

- A map is 256 x 256 tiles; one tile is 100 world units. +x is east, +y is north (up in a top-down
  shot, in the exported images and in sketches).
- **Points** (`center`, `points`, an object's `tile`) are in tiles with fractions: world units /
  100. Tile (x, y) covers x..x+1 and y..y+1, so its centre is [x + 0.5, y + 0.5]. An object
  placed at [130, 120] stands at world (13000, 12000), and `map-query` then reports its tile as
  [130, 120].
- **Rectangles** (`"type": "rect"`, `--rect`) are whole tiles with both corners included, exactly as
  `map-query` and `map-export` take them: [100, 90, 109, 99] is 10 x 10 tiles.
- **Heights** are world units. A map's height file stores 0 to 382.5 in steps of 1.5 (the login
  scene 0 to 765 in steps of 3): edits are clamped to that range, a target outside it is an
  error, and saving rounds each height down to its step. The ground of Lorencia's town is about
  165; a hill of +100 is clearly visible, +300 is a cliff.
- **Light** is the painted light map, red, green and blue from 0 to 1, which multiplies the
  ground's colour (and lights most objects standing on it). Most of Lorencia is about 0.7.
- **Angles** are degrees. An object's `angle` is [x, y, z]; z is its heading (turning it about the
  vertical axis). A single number means the heading. The camera's `yaw` is the compass heading of
  the view (0 looks north, 90 east), `pitch` how far it looks down (5 to 89.7).
- **Object ids** are the `index` values `map-query` and `objects.json` report: the object's place
  in the next save of the object file, as the map stands when the script starts. Objects a script
  places get the ids after the last one (the answer lists them). Deleting objects or saving
  renumbers them: query again before a later script names ids.

## 10. The edit script

```json
{
  "schema": "mu-map-edit/1",
  "label": "Hill, dirt road and a grove",
  "ops": [
    {"op": "terrain.raise", "shape": {"type": "circle", "center": [216, 131], "radius": 6}, "amount": 120}
  ]
}
```

- `schema` is required and must be `mu-map-edit/1`. `label` (optional, up to 120 characters) names
  the undo step; without it the step is called after its ops ("Script: terrain.raise, ...").
- `ops` (1 to 256) run in order, each on the result of the ones before. Every op is checked
  before anything changes; if any op fails, nothing of the script is applied.
- A script runs on the client's main loop, which does not draw or answer anything meanwhile. One
  that would take more than about 1.5 s (many ops over the whole map, paths and polygons with
  hundreds of points) is refused before it starts, naming its most expensive op: split it into
  several scripts (each is its own undo step).
- A field an op does not know is an error, so a typo ("radus") never passes silently.
- `mapctl apply FILE` sends it inline, or by path when it is bigger than the socket's 256 KiB line
  limit (`"script": {...}` or `"path": "/abs/path/script.json"` on the socket).

### Shapes

Every op that works on an area takes a `shape`:

| Shape | Fields |
|---|---|
| circle | `{"type": "circle", "center": [x, y], "radius": r}` (0.1 to 256 tiles) |
| rectangle | `{"type": "rect", "rect": [x0, y0, x1, y1]}` (whole tiles, both corners included) |
| polygon | `{"type": "polygon", "points": [[x, y], ...]}` (3 to 256 points; a concave outline works) |
| path | `{"type": "path", "points": [[x, y], ...], "width": w}` (2 to 256 points, width 0.1 to 64 tiles) |

Any shape may add `"falloff": f`, the width in tiles of its soft edge, measured inwards from the
outline: full strength `f` tiles inside it, fading smoothly (smoothstep) to nothing at it.
`"falloff": 0` is a hard edge. Without it:

- **Soft ops** (every `terrain.*` op, every `light.*` op, `texture.paint` on layer 2,
  `texture.erase`) fade over the outer half of the shape's inner radius (the distance from the
  outline to the shape's deepest point): a circle is full strength out to half its radius, exactly
  the Map Editor's round brushes; a rectangle fades over a quarter of its shorter side along its
  rim; a path of width w over w/4 on each side, full strength in its middle half.
- **Hard ops** use the outline itself: `texture.paint` on layer 1 and `attribute.set` change the
  tiles whose centre lies inside (a soft falloff there moves the edge inwards: a tile needs half
  weight), `object.scatter` places inside the shape (a falloff thins the objects towards the
  edge), and a selector's `inside` picks the objects whose position lies inside.

Points must lie on the map (0 to 256); a shape may reach over the map's edge and is cut there.

### Height targets

`terrain.flatten`'s `to`, `terrain.set`'s `height` and `terrain.ramp`'s `from` and `to` take:

- a number: that height;
- `"average"`, `"min"`, `"max"`: the weighted mean, lowest or highest ground inside the shape
  before the op;
- `"center"`: the ground at the shape's centre (a circle's centre, the middle of other shapes);
- `"ground"` (ramps only): the ground at that end of the ramp;
- `{"of": <one of those>, "plus": n}`: that height plus n (n may be negative).

### Terrain

| Op | Fields (default) | What it does |
|---|---|---|
| `terrain.raise` | `shape`, `amount` | adds `amount` x weight |
| `terrain.lower` | `shape`, `amount` | takes it away |
| `terrain.flatten` | `shape`, `to` ("average"), `strength` (1) | moves the ground towards the target by weight x strength |
| `terrain.set` | `shape`, `height`, `strength` (1) | the same, with a height you give |
| `terrain.ramp` | `shape` (path or rect), `from`, `to`, `axis` (rect: "x" west to east, "y" south to north), `strength` (1) | a straight slope: along a path from its first point to its last, or across a rectangle |
| `terrain.smooth` | `shape`, `iterations` (1, up to 64), `strength` (1) | each pass moves every corner towards the mean of itself and its four neighbours (the Height tab's Smooth) |
| `terrain.noise` | `shape`, `amplitude`, `scale` (8 tiles), `octaves` (3, up to 8), `seed` (1) | adds bumps up to +-amplitude; `scale` is the size of the broad bumps, each further octave adds finer ones at half the size and strength; the same seed always gives the same bumps |

Every terrain op also takes `objects_follow` (true): objects standing on ground that moves keep
their height above it, as with the Height tab's "Objects follow terrain". The ground is relit
(normals and lit colours) exactly as after a brush stroke.

### Textures

| Op | Fields (default) | What it does |
|---|---|---|
| `texture.paint` | `shape`, `tile`, `layer` (1) | layer 1, the base texture, one per tile: every tile inside gets `tile` |
| `texture.paint` | `shape`, `tile`, `layer`: 2, `opacity` (1), `strength` (1) | layer 2, the overlay blended over the base per corner: towards `opacity` by weight x strength; painting over another overlay replaces it |
| `texture.erase` | `shape`, `strength` (1) | fades the overlay out; a corner that reaches 0 loses it |

`tile` is a slot number (0 to 29) or a name: the slot's name (`TileGrass01`, `TileGround01`,
`ExtTile03`) or the file the map loaded into it (the export legend lists both). Only slots that
hold a texture on this map are accepted. Lorencia's roads are overlays on layer 2. On Lorencia
`TileGround01` is paving stones, `TileRock02` gravelly dirt, `TileGrass01`/`TileGrass02` grass,
`TileWater01` water: look at a texture before you pick it.

### Walkability

`attribute.set` with `shape` and `value`: `"walkable"` (0), `"safezone"` (1), `"blocked"` (4),
`"void"` (8, no ground) or `"water"` (16), as a name or its number. Only these clean single
values: the client and the OpenMU server read combinations differently (MAP_EDITOR.md, gotcha
12). The tiles whose centre lies inside the shape get the value. With `"under"` (an object
selector, as `select` below) instead of `shape`, the tile each selected object stands on gets it,
as a scatter's `mark_attribute` would have: `{"op": "attribute.set", "under": {"models": ["Tree01",
"Tree11"], "inside": {...}}, "value": "blocked"}`.

### Light

| Op | Fields (default) | What it does |
|---|---|---|
| `light.add` | `shape`, `color` ([1, 1, 1]), `strength` (0.2) | adds color x strength x weight |
| `light.subtract` | the same | takes it away (darker) |
| `light.tint` | `shape`, `color`, `strength` (0.5) | moves towards `color` by weight x strength |
| `light.set` | `shape`, `color`, `strength` (1) | the same; with strength 1 the core becomes `color` |
| `light.smooth` | `shape`, `iterations` (1), `strength` (1) | evens the light out |
| `light.bake` | `shape`, `azimuth` (135), `elevation` (35), `contrast` (1, up to 4) | shades the relief from a sun at compass heading `azimuth` (0 north, 90 east; 135 is the engine's own south-east light) and `elevation` degrees high: flat ground keeps its light, a slope that faces the sun gets brighter and one that turns away darker, by Lambert's cosine against flat ground's, times `contrast` |

Values stay within 0 to 1. The lit colours of the ground are rebuilt at once. `light.bake`
multiplies the light that is there, so bake first and tint afterwards, and bake a map once: a
second bake deepens the shading again. A 10-degree slope turned away from the sun loses about 15 %
at contrast 1; 1.5 to 2 reads well from the player's camera.

### Objects

| Op | Fields (default) | What it does |
|---|---|---|
| `object.place` | `model`, `tile` ([x, y]), `height` ("ground"), `angle` (0), `scale` (1) | one object |
| `object.scatter` | see below | many objects at once |
| `object.move` | `select`, `by` ([dx, dy] tiles) or `to` ([x, y]), `height` ("keep") | moves the objects (`to` moves the group's middle there) |
| `object.rotate` | `select`, `by` or `to` (degrees), `pivot` ("each") | turns each object's heading; `"pivot": "center"` with `by` turns the group around its middle, as the gizmo does |
| `object.scale` | `select`, `by` (factor) or `to` (0.05 to 20) | scales each object on its spot |
| `object.delete` | `select` | deletes the objects |
| `object.drop_to_ground` | `select` | puts each object on the ground under it |

- `model` is the model's name as `map-query` shows it (the asset catalog's name, else the model
  file's own; any case) or its type number. Only models this map has loaded can be placed.
- `height`: `"ground"`, `{"offset": n}` (n above the ground), `{"absolute": n}`, and for moves
  `"keep"` (as far above the ground as before; the default).
- `select` picks objects by any mix of `ids` (list), `model` or `models`, `inside` (a shape) and
  `placed_by` (the index of an earlier `object.place`/`object.scatter` op in this script); an
  object must match all that are given. A selector that matches nothing is an error, as is an id
  no object has.

`object.scatter` fields:

| Field | Default | Meaning |
|---|---|---|
| `model` or `models` | | one model, or a list of names, types or `{"model": ..., "weight": w}` (picked in proportion to the weights) |
| `shape` | | where |
| `count` or `density` | | how many objects, or objects per tile of the shape (up to 5000 per scatter) |
| `min_spacing` | 1 | no two new objects closer than this many tiles |
| `seed` | 1 | the same seed and map give the same objects, models, sizes and headings |
| `scale_range` | [1, 1] | each object's scale, uniformly in the range |
| `yaw_range` | [0, 360] | each object's heading |
| `align` | "ground" | objects stand upright on the ground (the only choice) |
| `avoid` | | `attributes` (walkability values: a tile with any of the listed flags, or for "walkable" a tile with none), `textures` (layer-1 slots), `slope` (degrees: steeper ground), `objects` (tiles from any object already there, including ones placed earlier in the script), `areas` (a list of shapes, e.g. a road as a path a little wider than the road) |
| `mark_attribute` | none | writes this walkability value on each new object's tile, e.g. `"blocked"` under tree trunks as Lorencia's own trees mostly have it |

Scatter throws random points into the shape (Poisson-disk sampling by dart throwing: every
point at least `min_spacing` from the others, spread over the whole shape rather than grown
from one spot) and keeps the ones the rules allow. When the shape is full it places fewer than
asked and says so in a warning ("placed 25 of 30: ..."); that is not an error.

## 11. Commands

All need an editor build and the `world` scene (a map open offline, or a logged-in session with the
editor). `mapctl` wraps them (section 3); on the socket each is one JSON line with `cmd` and its
fields, answered by one line (`{"ok": true, "result": {...}}` or `{"ok": false, "error": ...,
"message": ...}`). Without mapctl, from Python:

```python
import json, socket
def send(request):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect("/tmp/mu-501/mu-mapctl.sock")  # mapctl's default: /tmp/mu-<your uid>/
    s.sendall((json.dumps(request) + "\n").encode())
    answer = json.loads(s.makefile().readline())
    s.close()
    return answer
```

The looking commands (`map-info`, `map-camera`, `map-export`, `map-query`, `screenshot` with `clean`
and `region`) are described in [`docs/control-socket.md`](../control-socket.md), "Editor commands",
with the export's file formats; the commands that grow the world in sections 6 and 8 above.

**`map-apply`** `{"script": {...}}` or `{"path": "..."}`, optional `"dry_run": true`:

```json
{"ok":true,"result":{"label":"Hill, dirt road and a grove","dry_run":false,"changed":true,"applied":true,
 "ops":[{"op":0,"name":"terrain.raise","area":[210,125,222,137]},
        {"op":3,"name":"object.scatter","area":null,"requested":30,"placed":30,"ids":[2870,2871,...]}],
 "changes":{"height":{"cells":190,"area":[209,124,223,138]},"texture1":{"cells":0,"area":null},
            "texture2":{"cells":102,"area":[197,122,212,144]},"attribute":{"cells":30,"area":[200,118,223,140]},
            "light":{"cells":145,"area":[210,125,222,137]},
            "objects":{"added":30,"removed":0,"changed":7,"listed":37,"list":[{"change":"changed","id":2528,
              "model":"Stone03","type":32,"tile":[210,136],"position":[...],"angle":[...],"scale":1.42,
              "before":{...}}, ...]}},
 "warnings":[],"unsaved":{"texture":true,"height":true,"attribute":true,"light":true,"objects":true}}}
```

`area`s are [x0, y0, x1, y1] (terrain corners for height, light and the overlay; tiles for the
base texture and walkability). The object list holds the first 200 changes; the counts are
complete. A script that changes nothing adds no undo step (`"applied": false`). Errors:
`bad_request` (the script, with the field), `busy` (the owner holds a brush stroke or a drag in
the Map Editor; send it again after they let go).

**`map-undo`**, **`map-redo`**: one step of the shared history, the Map Editor's own steps
included. The answer names the step (`"undone": "..."`), the next ones (`next_undo`,
`next_redo`) and the `unsaved` flags; `failed` when there is nothing to undo or redo. An undo
restores the map bit for bit (the unsaved flags go back to false when it returns to the saved
state). The Map Editor's selection is kept; objects a step deleted drop out of it.

**`map-history`**: `undo` (oldest first; the last is the next undo), `redo` (next first),
`next_undo`, `next_redo`, `memory_bytes` (the history keeps up to 64 MB, then forgets the oldest
steps).

**`map-save`** `{"layers": ...}`: `"all"`, one file's name or a list of them; without `layers`
it saves the files that hold unsaved edits (map-info's `unsaved`). The names are `texture`
(`EncTerrain{N}.map`), `height` (`TerrainHeight.OZB`), `attribute` (`EncTerrain{N}.att`, the
client's walk map), `light` (`TerrainLight.OZJ`), `objects` (`EncTerrain{N}.obj`). Each is written
with the Map Editor's own save into the game's `Data/World{N}` and copied into the repository's
`src/bin/Data`, the file it replaces kept in `out/editor-backups/<time>/`:

```json
{"ok":true,"result":{"world":1,"saved":[{"layer":"height","saved":true,"file":".../MacOS/Data/World1/TerrainHeight.OZB",
 "repo_result":"replaced","repo":".../src/bin/Data/World1/TerrainHeight.OZB",
 "backup":".../out/editor-backups/20260923-124045/Data/World1/TerrainHeight.OZB","report":"..."}],
 "unsaved":{...}}}
```

`repo_result` is `replaced`, `created` (a new file: git ignores new files under `src/bin`, add it
with `git add -f`), `unchanged` (the same bytes; no backup) or `in_place`. A file that could not
be written answers `failed` with the whole result attached. Saving does not touch the undo
history.

**`map-revert`** `{"layers": ...}` (required; the same values as for `map-save`): reads those files
back from `Data/World{N}` as the map load reads them, throwing away what was not saved, and clears
the undo history (a revert cannot be undone). Every file is checked first; if one cannot be read,
nothing is reverted (`failed`, with the file and the reason).

**`map-open`** `{"world": N}`: switches the offline session to `Data/World{N}` and drops the old
map's unsaved edits, selection and undo history without asking; `mapctl open` checks `map-info`
first and refuses while there are unsaved edits (`--discard` opens anyway).

**`map-tab`** `{"tab": "Texture"}`: shows the Map Editor on that tab (Texture, Objects, Height,
Attribute, Light, Gates, T. Browse, Minimap, O. Browse, Assets; any case). The Gates tab, which
`gate-show` opens, draws the walkability and the gate areas on the ground, also in clean shots;
`mapctl tab texture` takes them away again.

**`map-info`** with `"models": true` (`mapctl info --models`) adds `models`: every model the map has,
by `type`, with its `name` (the model file's own) and `catalog_name` where the map has an asset
catalog. Names that are not UTF-8 (the Korean names of many game models) show each byte outside
ASCII as `%XX`; a script may use that spelling or the type.

A command that fails inside the client (a bug, not a refusal) answers `failed` with the reason
instead of ending the client.

## 12. A worked example

Lorencia (`--world 1`), the open grass east of the town, "a knoll with a stone, a gravel path from
the paved road and a sparse grove". This ran on 2026-09-23 as one script; section 4 would split it
into layers.

```sh
mapctl launch --world 1
mapctl query --rect 195 110 235 150                 # heights 135 to 235.5, 64 objects, grass; 30 tiles blocked
mapctl export --out $W/export --rect 195 110 235 150
mapctl shot $W/before/top.png --topdown 195 110 235 150
mapctl camera --tile 212 130 --yaw 0 --pitch 50 --distance 4200 && mapctl shot $W/before/view.png
mapctl sketch $W/before/top.png --rect 195 110 235 150 --script $W/scripts/knoll.json --out $W/sketches/knoll.png
mapctl dry-run $W/scripts/knoll.json                # 20 of 20 trees placed, no warnings
mapctl apply $W/scripts/knoll.json                  # one undo step, "Knoll with a path and a grove"
mapctl shot $W/after/top.png --topdown 195 110 235 150
mapctl camera --tile 212 130 --yaw 0 --pitch 50 --distance 4200 && mapctl shot $W/after/view.png
mapctl undo                                          # every unsaved flag back to false: the map as loaded
```

`knoll.json`:

```json
{
  "schema": "mu-map-edit/1",
  "label": "Knoll with a path and a grove",
  "ops": [
    {"op": "terrain.raise", "shape": {"type": "circle", "center": [221, 124], "radius": 6}, "amount": 90},
    {"op": "terrain.smooth", "shape": {"type": "circle", "center": [221, 124], "radius": 8}, "iterations": 2},
    {"op": "texture.paint", "layer": 2, "tile": "TileRock02", "opacity": 1.0,
     "shape": {"type": "path", "points": [[203, 146], [209, 138], [214, 131], [219, 126]], "width": 2.5, "falloff": 0.8}},
    {"op": "object.scatter",
     "models": [{"model": "Tree03", "weight": 2}, {"model": "Tree04", "weight": 2}, {"model": "Tree05", "weight": 1}],
     "shape": {"type": "circle", "center": [214, 128], "radius": 12},
     "count": 20, "min_spacing": 2.2, "seed": 11, "scale_range": [0.85, 1.15],
     "avoid": {"areas": [{"type": "path", "points": [[203, 146], [209, 138], [214, 131], [219, 126]], "width": 5},
                         {"type": "circle", "center": [221, 124], "radius": 3}],
               "objects": 1.2, "attributes": ["blocked", "water"]},
     "mark_attribute": "blocked"},
    {"op": "object.place", "model": "Stone03", "tile": [221.5, 124.5], "scale": 1.3},
    {"op": "light.tint", "shape": {"type": "circle", "center": [221, 124], "radius": 6},
     "color": [1.0, 0.85, 0.6], "strength": 0.3}
  ]
}
```

The same with the socket directly, one object per line: `{"cmd": "map-apply", "script": {...},
"dry_run": true}`, then without `dry_run`, then `{"cmd": "map-undo"}`.

## 13. Limits and gotchas

- **Anti-tamper tiles.** The client closes a map whose walk map changed one magic tile:
  Lorencia (135, 123) = 5, Dungeon (227, 120) = 4, Devias (208, 55) = 5, Noria (186, 119) = 5,
  Lost Tower (193, 75) = 5 (MAP_EDITOR.md, gotcha 13). Any op that would write another value
  there (`attribute.set`, a scatter's `mark_attribute`) is refused; leave that tile out of the
  shape. Lorencia's is inside the town, right of the fountain.
- **Client and server walk maps are separate.** `attribute.set` and `mark_attribute` change the
  client's `EncTerrain{N}.att`. OpenMU keeps its own copy (section 8). Until the owner uploads it,
  the server lets players walk where the client blocks them.
- **Heights** stop at 382.5 and save in steps of 1.5. Maps whose height file stores 24 bits a
  corner (Doppelganger 2, the PK field, ChangeUp 3rd) refuse terrain ops and height saves.
- **The light map** saves as a JPEG: after `map-save` the light of the whole map moves by a few
  steps of 255 where it has detail (what you see after the save is what loads next time).
- **Objects follow the ground** by default when a terrain op moves it, including the map's own
  grass and stones: they show up in `changes.objects` as changed.
- **Ids move.** After `object.delete`, a save or a revert, `map-query` again before naming ids.
- **The object file** holds at most 32767 objects; a script that would pass that is refused.
- **Models and textures are per map.** A Devias script cannot use Lorencia's trees unless the
  Map Editor's O. Browse imported them. The error for an unknown name lists what the map has.
- **Scatter is seeded.** Change `seed` for another arrangement; keep it to get the same one again.
- **Undo is shared.** A scripted step appears in the Map Editor as "Undo: <label>"; the owner's
  own strokes appear in `map-history`. While the owner holds a stroke or a drag, edits and steps
  answer `busy`.
- **Switching maps drops unsaved work.** `map-open` forgets the loaded map's unsaved edits and undo
  history; `mapctl open` refuses while there are any.
- **A revert is final** for the unsaved edits, and forgets the undo history.
- **Screenshots move a little by themselves** (swaying grass, fire, drifting cloud shadows): two
  shots of an unchanged view a second apart differ in about 1.5 % of their pixels. Compare
  before and after shots taken close together, from the same camera pose. The proof that an undo
  restored the map is `unsaved` going back to false, not the picture.
- **Sketches are approximate on shots.** A top-down shot is a perspective view: hills shift by a
  few pixels. An export layer is exact (one pixel per tile).
- **New maps and gates are files at once.** `map-new` writes two folders and `gate-add` /
  `gate-remove` rewrite `Gate.bmd` (one file for every map) as soon as they run; only a dry run
  does not. Take a gate back with `gate-remove`; a map by deleting its folders (MAP_EDITOR.md, "New
  maps"). Git tracks the gate table as `src/bin/Data/gate.bmd` (lowercase): `git add
  src/bin/Data/gate.bmd`, never `Gate.bmd`.
- **Copies lose map-bound code.** A copy of Lorencia has no fires or lights, some of its house and
  street-light models draw black parts, and every new map is silent with black fog: look at the
  copy before you build on it.
- **Walking through a gate needs the server.** Offline there is no hero; the gates are checked only
  as data. The server learns the map and its gates from the export (section 8).
- **The Gates tab paints the ground.** While it is the Map Editor's open tab (`gate-show` opens it),
  the walkability overlay and the gate areas are drawn on the ground, so they also show in `clean`
  screenshots; `mapctl tab texture` switches back.
- **Korean model names.** Many game models (Devias, Dungeon, Noria and most later maps) carry names
  in the Korean code page; answers and exports show them with each byte outside ASCII as `%XX`
  (`Data2\Object3\%B0%DC...01.smd`). Scripts can use that spelling, or simply the type number.
- **Screenshots can miss a frame.** Right after a map-open, a save or while the window is being
  covered, a frame may not be read back; the client tries later frames for 3 s and `mapctl shot`
  asks twice more before it reports `failed`.
- **Saved files are the game's data.** `map-save` changes files git tracks under `src/bin/Data`
  (`git status` shows them; `git checkout -- <file>` or the backup in `out/editor-backups/` undoes
  a save). Do not save unless the owner asked for it.
- **Timeouts.** A command that gets no answer within `--timeout` (60 s) exits 3. The client serves
  the socket once per frame; if it seems stuck, `MuError.log` beside `Main` says why, and a client
  that has quit wrote `Quit requested: <why>` there last.
- **Keep the owner's typing out of your client.** Its window opens on the owner's screen and can take
  the focus: keys meant for another program then fly the camera or, with Cmd+Q, quit your client.
  Check the `pose` a `camera` answer reports when a shot looks wrong, and frame again.
