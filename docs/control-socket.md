# Control socket

The client can be driven from a shell: logging in, selecting a character,
walking, fighting, chatting, reporting what it sees and taking screenshots,
without anyone at the keyboard. It is developer tooling for scripted tests and
demos, off unless a launcher asks for it, and it works the same on Windows, Linux
and macOS. In a build with the editor it also lets a script (or an AI agent) look
at a map the way the Map Editor sees it and edit it: see [Editor commands](#editor-commands)
and, for agents, [`docs/agents/AI_MAP_EDITING.md`](agents/AI_MAP_EDITING.md).

## Turning it on

The socket is a build-time feature. It exists only in a client configured
with `ENABLE_CONTROL_SOCKET=ON` — the `-mueditor` presets (`windows-x64-mueditor`,
`linux-x64-mueditor`, `macos-arm64-mueditor`, …) and hand-configured
developer builds; the plain presets (`linux-x64`, `windows-x64`, `macos-arm64`, …) and any
build that does not opt in compile none of it, and the `control_socket_leak`
test in their test suite proves it. A player build ignores the variable
below entirely: no socket, no log line, no change of behaviour.

In a build that has it, the client opens the socket only when
`MU_CONTROL_SOCKET` names a path:

```sh
MU_CONTROL_SOCKET=/run/user/1000/clients/default.sock ./Main /u127.0.0.1 /p44405
```

On macOS the client runs from the app bundle, and the Map Editor's offline mode
needs no server at all:

```sh
mkdir -m 700 -p /tmp/mu-$(id -u)
cd out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS
MU_CONTROL_SOCKET=/tmp/mu-$(id -u)/editor.sock ./Main --editor --world 1
```

Keep the path short: a socket path holds at most 103 bytes on macOS and 107 on
Linux and Windows. A longer one is refused with a log line
(`control socket path is longer than …`) and the client runs without the socket.
Put it in a folder only you can use: `$XDG_RUNTIME_DIR` on Linux, macOS's own
`$TMPDIR` (about 50 bytes, `/var/folders/…/T/`, fits), or a mode-0700 folder such as
`/tmp/mu-<uid>/`, which `tools/world_editor/mapctl.py` creates and uses by default.
Straight in `/tmp`, which every user may write to, another user could take the
name first: the client then cannot take that name and runs without the socket.

Without the variable nothing is created, nothing is logged and the client
behaves exactly as before. With it, the client logs one line
(`control socket listening on <path>`), creates the socket with owner-only
permissions (`0600`), replaces a stale file left behind by a crashed client,
and removes the file when it exits normally.

An option would not do: the client's command line is split on spaces, so a
path containing one could not be passed.

## Talking to it

Newline-delimited JSON, one request object per line, exactly one response per
line:

```sh
printf '{"cmd":"ping"}\n' | socat - UNIX-CONNECT:/run/user/1000/clients/default.sock
{"ok":true,"result":{"build":"Sep 16 2026 16:22:46","scene":"world"}}
```

macOS has no `socat` out of the box; Python's standard library does the same:

```python
import json, socket
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/mu-editor.sock")
s.sendall(b'{"cmd":"ping"}\n')
print(json.loads(s.makefile().readline()))
```

- A request carries `cmd` and the command's own fields, plus an optional `id`
  that is echoed back so a caller can match answers to requests. `id` belongs
  to the framing: a drop is named by `item`, not by `id`.
- A response is `{"ok":true,"result":{…}}` or
  `{"ok":false,"error":"<code>","message":"…"}` — with a `result` holding the
  progress made when a long-running command was interrupted or timed out.
- Requests are served on the main thread, once per frame, after the packets of
  that frame have been processed. Nothing runs concurrently with game logic. A
  command that fails inside the client (an exception, such as text the JSON
  encoder cannot take) answers `failed` with the reason instead of ending the
  client, and text that is not UTF-8 goes out with U+FFFD in its place.
- Several connections may be open at once. Commands that drive the character
  (`login`, `move`, `attack`, …) run one at a time: a second one interrupts the
  first, whose caller is told how far it got. Reading commands (`state`,
  `events`, `wait-for`, `screenshot`) run alongside.

Error codes: `bad_request`, `unknown_command`, `wrong_scene`, `busy`,
`interrupted`, `timeout`, `not_connected`, `login_failed`, `no_such_character`,
`no_such_skill`, `not_in_view`, `not_attackable`, `no_path`, `not_allowed`,
`warp_refused`, `skill_refused`, `insufficient_mana`, `not_pickable`,
`empty_slot`, `move_refused`, `failed`.

## Commands

| Command | What it does |
|---|---|
| `ping` | build identifier, current scene and the client's process id (`pid`) |
| `scene` | which screen the client is on: `login`, `character_list`, `world`, … |
| `state` | the character and everything around it (see below) |
| `nearby` | the objects the client can see |
| `events` (`since`, `follow`) | recorded events, or a live stream of them |
| `wait-for` (`event`, `match`, `timeout`) | block until a matching event arrives |
| `screenshot` (`out`, and in editor builds `clean`, `region`) | capture the next frame to a path; without `out` it names itself, uniquely per capture. JPEG, or PNG in editor builds for a `.png` `out`, `clean` or `region` (see [Editor commands](#editor-commands)) |
| `hotkey` (`key`) | press one game key for a frame: `esc`, `i`, `home`, `f1`, … |
| `click-ui` (`x`, `y`, `button`) | click a window pixel (`left` by default) |
| `login` (`account`, `password`, `server`) | server selection, credentials, character list |
| `select-char` (`name` or `slot`) | enter the world with that character |
| `logout`, `quit` | back to the character list; close the client (`quit` answers with its `pid`, and `MuError.log` records `Quit requested: …` for every way the client is asked to close) |
| `move` (`x`, `y`) | walk there with the client's own path finder |
| `warp` (`gate`) | use a warp-list entry by name |
| `teleport` (`x`, `y`, `map`) | the game master's own move command; `map` is an index, the current map when omitted |
| `attack` (`target`, `times`, `interval`) | plain attacks on an id or character name |
| `skill` (`skill`, `target`) | cast a skill the character owns — with `target` it is aimed at that object, without one it is cast where the character stands |
| `pickup` (`item`) | walk to a drop and take it, by the id `nearby` reports for it |
| `use` (`slot`), `equip` (`slot`, `target_slot`) | inventory actions |
| `say` (`text`), `whisper` (`name`, `text`) | chat, including `/` commands |
| `party` (`action`, `target`) | `invite`, `accept`, `decline`, `leave` |
| `halt` | stop the walk or repeated attack in progress |

`state` reports the scene and account on every screen, and in the world adds:
character name, class, level, experience, zen, HP/mana/SD/AG with their
maxima, map number and name, position, alive flag, safe-zone flag, current
target, the skills the character owns, equipment, inventory, buffs, party and
`nearby`.

Each `nearby` object carries `id`, `kind`, `name`, `position`, and a player,
monster or NPC also `alive`, `level` and `hp_percent`. `hp_percent` is a
percentage of full health (`100` is untouched) and is `null` when the server
has not told the client that object's health — a threshold test has to allow
for the null rather than read it as zero.

### Synthetic input

`hotkey` and `click-ui` inject a key or a click *below* the game's own input
readers: the key counts as down for one rendered frame in the same key-state
scan a physical key goes through, and the click writes the same mouse
variables the event loop fills from real mouse events, at a window pixel. The
window system is never involved — no pointer movement, no focus change, no
synthetic OS events — so a scripted session can open the system menu, the
inventory or start the MU Helper from its HUD button while the human keeps
working elsewhere. Coordinates for `click-ui` are the pixels of the client
area, the same space a `screenshot` image is in, so a script can capture,
locate and click. What a click reaches is what reads those variables: the
current UI layer and the world. The older widget layer hit-tests through
`CInput`, which reads the operating system's cursor, and a click injected
below the readers never moves that — those windows are out of scope for
`click-ui`, and a key is the way to drive them. Key names are case-insensitive: the letters, the digits,
`esc`, `enter`, `tab`, `space`, `backspace`, `home`, `end`, `insert`,
`delete`, `pageup`, `pagedown`, `up`, `down`, `left`, `right`, `printscreen`
and `f1`–`f12`; anything else answers `bad_request`. Both answer once the
release frame has run; a second injection while one is in flight answers
`busy`, while a walk or an attack in flight is left alone — injecting a key is
an observation of the act slot, not a claim on it. The sequence follows
*rendered* frames, so an injection sent to a client that is not rendering (the
occluded-window case below) answers `timeout` and is dropped rather than
delivered late. Not covered: typing text (`say` sends chat), key chords, drags.

## Editor commands

A client built with the editor (`-mueditor` presets) answers these too; a player
build does not know them (`unknown_command`). They look at the map the client has
loaded and at the Map Editor's view of it, so an agent can see a map before it
edits one, and edit it with undoable scripts. All need the `world` scene; `map-open`
needs a map opened offline (`--editor --world N`). `tools/world_editor/mapctl.py` wraps them in
a command line (`mapctl.py launch --world 1`, `mapctl.py shot top.png --topdown ...`); agents
start at [`docs/agents/AI_MAP_EDITING.md`](agents/AI_MAP_EDITING.md).

Tiles are `[x, y]`, rectangles `[x0, y0, x1, y1]` (corners in any order, both
included), 0 to 255 on every map; one tile is 100 world units. `world` is the map's
folder number (`Data/World{N}`, Lorencia is 1), `map` the game's own map number
(`world - 1`, what `state`, `teleport` and OpenMU use).

| Command | What it does |
|---|---|
| `map-open` (`world`) | switch the offline session to `Data/World{world}`; the Map Editor drops the old map's selection and undo steps, the camera goes to the new map's start view |
| `map-info` (`models`) | the loaded map: `world`, `map`, `name`, `tiles`, `objects`, the asset `catalog`, the `gates` of `Gate.bmd` on this map, and which of its files hold `unsaved` edits; with `models: true` also every model it has, by type |
| `map-tab` (`tab`) | show the Map Editor on a tab (`Texture`, `Gates`, …): the Gates tab draws walkability and gate areas on the ground, also in clean screenshots |
| `map-camera` (`tile`, `yaw`, `pitch`, `distance` or `height`) | point the free-fly camera at a tile |
| `map-camera` (`topdown: {"rect": [...]}`) | look straight down on a rectangle, north up, as high as it takes to see all of it |
| `map-export` (`layers`, `rect`, `out`) | write the map's layers as images, one pixel per tile, with `objects.json` and `legend.json` |
| `map-query` (`rect` or `tile`) | numbers about an area: heights, attribute and texture counts, the objects standing in it |
| `screenshot` (`clean`, `region`, `out`) | `clean: true` leaves out the editor, the cursor, the HUD and the camera text; `region` crops to a rectangle of tiles (for top-down shots); written as PNG |
| `map-apply` (`script` or `path`, `dry_run`) | run an edit script (`mu-map-edit/1`: terrain, textures, walkability, light, objects) as one undo step; a dry run only reports what would change |
| `map-undo`, `map-redo` | one step of the Map Editor's undo history (scripted or the owner's own) |
| `map-history` | the steps that can be undone and redone |
| `map-save` (`layers`) | save the map's files (default: the ones with unsaved edits) into the game's `Data` and the repository, with backups |
| `map-revert` (`layers`) | read files back from disk, dropping unsaved edits and the undo history |
| `map-new` (`map` or `world`, `name`, `from`, `models_from`, `dry_run`) | make a new map (82 to 254) as a copy of another or flat, in `Data/World{map + 1}` and the repository |
| `gate-list` (`map` or `world`) | the gates on a map (the loaded one by default) and the gates that lead to it |
| `gate-add` (`from`, `to`, `level`, `allow_trap`, `dry_run`) | a one-way gate between two maps; saves `Gate.bmd` at once; refuses an arrival nobody could leave or one inside its own enter area unless `allow_trap` |
| `gate-remove` (`number`, `dry_run`) | remove a gate the editor added (345 and up) |
| `gate-show` (`number`, `look`) | open the Map Editor's Gates tab on a gate of the loaded map |
| `map-server-export` (`map` or `world`, `safezone_map`, `out`) | write what OpenMU needs for a map (walk map, gates, `HOWTO.md`, SQL); never applied |

`map-camera`'s `yaw` is the compass heading of the view in degrees: 0 looks north
(+y, up in a top-down shot), 90 east (+x). `pitch` is how far it looks down: 5 to
89.7 (straight down; the camera cannot look exactly straight down). `distance` is
how far the camera stands from the tile, `height` how far above it (100 to 60000).
Leaving them out gives the offline start view: yaw -45, pitch 45, distance 6000.
The answer is the camera's `pose` as it now stands, which the next frame (and so
the next `screenshot`) uses:

```json
{"cmd":"map-camera","tile":[122,232],"pitch":55,"distance":2500,"yaw":0}
{"ok":true,"result":{"tile":[122,232],"pose":{"target":[12250.0,23250.0,165.0],
 "position":[12250.0,21816.1,2212.9],"target_tile":[122,232],"yaw":0.0,"pitch":55.0,
 "distance":2500.0,"view_range":25000.0,"vertical_fov":30.5}}}
```

A top-down framing draws as far as the rectangle needs; the next `tile` framing
goes back to the camera's usual range. Frame and shoot the same rectangle:

```json
{"cmd":"map-camera","topdown":{"rect":[0,0,255,255]}}
{"cmd":"screenshot","clean":true,"region":[0,0,255,255],"out":"/tmp/lorencia.png"}
{"ok":true,"result":{"path":"/tmp/lorencia.png","width":1046,"height":1034,"frame":[1920,1080],
 "crop":[437,28,1046,1034],"region":[0,0,255,255],"clean":true}}
```

`region` crops the frame to the part the rectangle's ground covers in the drawn
view (`crop` is x, y, width, height in the frame); `not_in_view` when none of it is
on screen. Without `out` a capture goes to `<repository>/out/map-captures/`.

`map-export` writes every layer unless `layers` names some (`height`, `attribute`,
`texture1`, `texture2`, `alpha`, `light`, `objects`), for the whole map unless
`rect` limits it, into `out` (a folder; default `<repository>/out/map-exports/World{N}-<time>-<n>/`):

```json
{"cmd":"map-export","layers":["height","attribute","objects"],"rect":[100,90,170,160],"out":"/tmp/town"}
{"ok":true,"result":{"folder":"/tmp/town","area":[100,90,170,160],
 "layers":["height","attribute","objects"],"legend":"/tmp/town/legend.json",
 "files":["/tmp/town/height.png","/tmp/town/attribute.png","/tmp/town/objects.png",
          "/tmp/town/objects.json","/tmp/town/legend.json"]}}
```

How to read the files (`legend.json` says the same for each export):

- **Orientation:** pixel (column, row) is tile (x0 + column, y1 - row): +x to the
  right, +y (north) up, as the top-down camera shows the map.
- `height.png`: grey, the lowest height of the area black and the highest white;
  the legend gives `min`, `max` and `units_per_level` (height = min + pixel x units_per_level).
- `attribute.png`: one colour per walkability class, the strongest flag winning:
  no ground black, blocked dark red (dark green inside a safe zone: town walls and
  houses), water blue, special height magenta, action yellow, camera-up cyan, no
  attack orange, walkable safe zone green, walkable grey. The legend lists every
  attribute value in the area with its flags, colour and tile count.
- `texture1.png`, `texture2.png`: the texture slot of each tile layer in a colour of
  its own (grass green, water blue, rock grey, ExtTiles in bright hues; black on
  layer 2 where there is no overlay); the legend maps each slot to its texture name,
  the file the map loaded into it, its colour and tile count.
- `alpha.png`: layer 2's opacity (pixel / 255). `light.png`: the painted light map.
- `objects.png`: black where no object stands, brighter the more objects stand on
  the tile. `objects.json` lists them, one per line, each with `index` (its place in
  the next save of `EncTerrain{N}.obj`), `loaded_record` (its record in the loaded
  file, -1 for objects added since), `type`, `name` (the asset catalog's, else the
  model's), `tile`, `position`, `height`, `height_above_ground`, `angle`, `scale` and
  `block` (the 16 x 16-tile object-grid block the engine keeps it in).

`map-query` answers the same facts as numbers, for one tile or a rectangle, and
lists up to 200 of the objects in it (`objects_in_area` counts all):

```json
{"cmd":"map-query","tile":[135,123]}
{"ok":true,"result":{"area":[135,123,135,123],"tiles":1,"height":{"min":165.0,"max":165.0,"mean":165.0},
 "attributes":[{"value":5,"flags":["safezone","nomove"],"tiles":1}],
 "texture1":[{"slot":0,"name":"TileGrass01","tiles":1}],"texture2":[{"slot":255,"name":"none","tiles":1}],
 "alpha_mean":0.0,"light_mean":[0.42,0.40,0.40],"objects_in_area":1,"objects":[{"index":1701,"name":"Fence02",...}]}}
```

The edit commands (`map-apply` and the ones after it in the table) are described, with the
script format and a worked example, in [`docs/agents/AI_MAP_EDITING.md`](agents/AI_MAP_EDITING.md);
`map-new`, the `gate-*` commands and `map-server-export` in its section "Growing the world". In
those, a map is `{"map": N}` or `{"world": N}`, and a new map's folder is its number + 1.
`map-apply`, `map-undo`, `map-redo`, `map-save` and `map-revert` answer `busy` while the owner
holds a brush stroke or a drag in the Map Editor.

`map-info`'s `unsaved` has one flag per file a map saves to: `texture`
(`EncTerrain{N}.map`), `height` (`TerrainHeight.OZB`), `attribute`
(`EncTerrain{N}.att`), `light` (`TerrainLight.OZJ`) and `objects`
(`EncTerrain{N}.obj`). A flag is set while that part of the loaded map differs from
what was loaded or last saved; an edit that was undone again does not count.
`gates` lists each gate whose area lies on the map: `enter` gates (walking into
`area` warps to gate `target` on `target_map`, from `level`) and `arrival` gates.

## Events

The packet handlers record what a scenario asserts on. Each event carries a
strictly increasing `seq`, a UTC `time` and its own fields:

| Event | Fields |
|---|---|
| `hit` | `direction` (`dealt`/`received`), `attacker`/`target` `{id,name,kind}`, `damage`, `shield_damage`, `critical`, `missed` |
| `killed` | `victim`, `killer` |
| `stat` | `stat` (`life`, `mana`, `sd`, `ag`, `level`, `experience_gained` — the experience of one kill, and `damage_dealt` with it — the cumulative total is `state.experience`), `value`, `max` |
| `chat` | `sender`, `text`, `kind` (`public`, `whisper`, `party`, `guild`, `union`, `gens`, `gm`) |
| `drop` / `drop_gone` | `id` (the id `pickup` takes), `item`, `position` / `reason` |
| `map` | `map`, `map_name`, `position` |
| `scene` | `scene` |
| `view_enter` / `view_leave` | `object` |
| `party` | `change`, `name` |
| `disconnect` | `reason` |
| `error` | `command`, `error`, `message` |

The last 2,048 events are kept. Names and fields are plain lower snake case,
so a test script can parse them next to a server-side event stream.

## Security posture

**Build-time gate first.** A player build contains no control code at all:
`ENABLE_CONTROL_SOCKET` is off by default, the plain presets and CI keep it
off, none of `App/Control/` or the local-socket transport is compiled, and
the activation variable's name does not appear in the executable. What a
player can reach by setting an environment variable is therefore nothing.
What stays in every build is the mouse-free automation the in-game helper
already had (`GameLogic/Automation`) and the extracted login/character entry
points — refactorings of existing code, not new surface. The gate removes the
ready-made API and the flip-a-switch path; it does not defend against a
patched or rebuilt binary.

Within a developer build the socket is unauthenticated: any process of the
same user can drive the client, like any other local developer endpoint. It is
a filesystem socket with mode `0600` at the path the launcher names (keep it in a
folder only you can use, see above), never a network address, never enabled from
`config.ini`, and never on unless the launcher sets the variable. `mapctl.py`
talks only to a socket file owned by the user running it, so a socket another
user planted at the same path is refused rather than driven.

## Keeping the taps when `WSclient.cpp` changes

The event recorders are one-line calls named `App::Control::Events::Record…`,
sitting at the end of the packet receive functions in
`src/source/Network/Server/WSclient.cpp` (hits, deaths, experience, stats,
chat, whisper, drops appearing and vanishing, view enter/leave, party changes,
logout) plus the scene and map watcher in `App/Control/ControlServer.cpp`.
When one of those functions is rewritten:

1. `rg -c 'App::Control::Events::' src/source/Network/Server/WSclient.cpp` —
   the count is 23; a lower one means a tap was dropped. Compare it against
   `git show upstream/main:…` when the number itself is in doubt: the count
   is a smoke test, the list above is the contract.
2. Re-run the live checks that cover the dropped tap (a fight records `hit`,
   `killed` and `stat`; a pickup records `drop` and `drop_gone`).

## Notes from the field

- A window nobody can see does not stop a scripted client. While the socket
  serves, a frame whose window is minimized, hidden or fully covered (as SDL
  reports it) is drawn into an offscreen image of the window's size instead of
  the window: the client never waits for the window system, keeps its frame rate,
  answers the socket, and `screenshot` still captures the world. Those frames are
  paced as a visible window's would be: at most two frames queued on the GPU and
  at most 60 a second. Without that, nothing limited them, the loop ran flat out
  and the Metal driver piled up vertex buffers for frames it had not drawn yet
  (tens of GB within seconds on a scatter-heavy map); paced, a hidden client
  stays at the memory and CPU of a visible one. The log says
  `window not visible: drawing offscreen for the control socket` when this starts
  and `window visible again` when it ends. Checked on macOS 15 (Metal): minimized,
  hidden with Cmd+H and covered by another window, for minutes each, `ping`
  answers in 2 to 20 ms and clean screenshots are correct. On macOS the loop did
  not stall even without it. On Linux, where the socket was first used, the loop
  used to stop while the window was fully covered (the compositor stops the frame
  callbacks a waiting swapchain needs); a compositor that stops them without
  telling SDL the window is covered can still stall it. Two clients side by side
  answer `ping` in about 4 ms.
- The server's speed check bans an account after four warnings in an hour, so
  every command that walks paces its steps (300 ms between walk packets,
  250 ms between automation steps). Do not remove that pacing.
- Attacks are refused inside a safe zone; `state`'s `safe_zone` flag says when
  the character is in one, and `attack` answers `not_allowed` there.
