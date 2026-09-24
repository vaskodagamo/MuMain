# Items

Everything the item editor and the art builder (Codex, [`ASTRA.md`](../../ASTRA.md)) need to know
about the game's items, and the requests to improve them. The plan behind it is
[`docs/agents/ITEM_EDITOR_PLAN.md`](../../docs/agents/ITEM_EDITOR_PLAN.md) (sections 2, 4 and 5).

```
assets-work/Items/
  README.md            this file
  catalog.json         generated: one entry per item that has a model ("mu-item-catalog/1")
  render-facts.json    generated: how the game draws every mesh of every item, and its effects ("mu-item-render-facts/1")
  openmu-items.json    generated: the OpenMU server's item definitions (optional input)
  tiers.json           the owner's tier overrides
  client-review.json   the owner's verdicts from the editor (created by the editor on first use)
  assignments.json     the coordinator's worker assignments for requests
  requests/            item requests: contract (README.md), request.schema.json, validate_request.py
  concepts/            the owner's picked concept images per item (see "Concept images")
```

The tools that build these files live in [`tools/item_editor/`](../../tools/item_editor/):

| Tool | Writes | What it does |
|------|--------|--------------|
| `gen_item_models.py` | `tools/item_editor/item_models.json` | Runs the client's C++ model-loading code (`OpenPlayers`, `OpenItems`, their texture functions and the member loaders they call) symbolically and records, per item, the BMD file, the folders its textures come from and the extra models the engine swaps in. |
| `item_table.py` | - | Decodes `Item_<lang>.bmd` (legacy 30-byte and current 50-byte names, BuxConvert XOR, checksum), `ItemSetType.bmd` and `ItemAddOption.bmd`. |
| `export_openmu_items.py` | `openmu-items.json` | Reads OpenMU's item definitions from the admin backup or the local database container. |
| `build_item_catalog.py` | `catalog.json` | Joins all of the above with `bmdconv info` of every model and the git history, and computes tiers (`tiers.py`). |
| `render_facts.py` | `render-facts.json` | Reads the client's item render code (`render_code.py`), applies `BMD::RenderMesh`'s rules (`render_rules.py`) to every mesh of every catalog model and merges the facts checked by hand (`render_notes.py`); see "How the game draws items". |
| `concepts.py` | `concepts/<key>/` (picks), `out/item-concepts/` | Concept images through the OpenAI Images API, with reference renders, cost caps and a contact sheet; see [`concepts/README.md`](concepts/README.md). |

## Rebuilding

Run on `main` after pulling, from the repository root. `bmdconv` comes from the tools build
(`cmake --build --preset macos-arm64 --target bmdconv`, then
`out/build/macos-arm64/tools/bmdconv/Release/bmdconv`); the catalog builder also finds it through
`$MU_BMDCONV` or under `out/build/*/tools/bmdconv/*/`.

```bash
# 1. only after a change to the C++ load code (ZzzOpenData.cpp and friends) or to the Data folders
python3 tools/item_editor/gen_item_models.py
# 2. optional: refresh the server facts (read-only; the OpenMU database container must be running)
python3 tools/item_editor/export_openmu_items.py --postgres database
#    or from an admin backup (Admin panel -> backup download, zip or extracted GameConfiguration_*.json)
python3 tools/item_editor/export_openmu_items.py --backup ~/Downloads/openmu-backup.zip
# 3. the catalog
python3 tools/item_editor/build_item_catalog.py --bmdconv out/build/macos-arm64/tools/bmdconv/Release/bmdconv
# 4. how the game draws the items (after the catalog; after a change to the item render code in
#    ZzzObject.cpp / ZzzCharacter.cpp, to render_notes.py or to a model)
python3 tools/item_editor/render_facts.py
# check only (exit 1 when a file is out of date)
python3 tools/item_editor/gen_item_models.py --check
python3 tools/item_editor/build_item_catalog.py --bmdconv <bmdconv> --check
python3 tools/item_editor/render_facts.py --check
# tests
python3 -m unittest discover -s tools/item_editor/tests
```

Commit the regenerated files together with whatever changed them. The catalog is deterministic
for a commit (sorted keys, no timestamps); it depends on `openmu-items.json`, so rebuild it after
refreshing the export. A worker branch that installs new textures or models does not rebuild the
catalog; the coordinator does that when a request is accepted.

## How the game draws items (blending and effects)

Painting an item starts with knowing how the client draws each of its meshes. The Wing01 style
pilot was repainted as an opaque texture, but the client draws the Wings of Elf additively: black
turned see-through and the colours glowed, so both variants failed in the game.
[`render-facts.json`](render-facts.json) records it for every catalog item, model and mesh, in
three contexts: **worn** (in the hand, on the back, as armour), **dropped** (on the ground) and
**inventory**, with the code lines that decide it. Item requests copy it
([`requests/README.md`](requests/README.md), "How the game draws the item"); the Item Editor shows
it as the **Drawn** line of an item's details.

| Mode | What the engine does | How to paint it |
|------|----------------------|-----------------|
| `opaque` | draws the texture as it is (lit) | as usual |
| `alpha-test` | a `.tga`: texels at or below 25% alpha are cut out, the rest is blended by its alpha | the alpha channel is the silhouette; keep the colour under the edges close to the edge colour (no dark or white fringe) |
| `blended-additive` | adds the texture to the picture (GL_ONE, GL_ONE), unlit, scaled by a brightness the code often pulses | paint light on **pure black**: black is fully transparent, brightness is glow; no background, no dark outlines or shading (they vanish), no alpha (a `.tga`'s alpha is ignored); it washes out over bright backgrounds |
| `blended-alpha` | the code draws the whole mesh at partial opacity | keep it light and even; nothing that must read as solid |
| `blended-subtract` | subtracts it (RENDER_DARK; rare) | dark areas darken the scene |
| `hidden` | not drawn in this context (`hid*` texture, `_H`, the code's hidden mesh, a cape replaced by cloth, skin/hair in the inventory) | keep the mesh; spend no effort on its look there |

What makes a mesh blended, in the engine's order:

1. **The item's own code.** `ItemObjectAttribute` gives some types a blend mesh (`BlendMesh 0`:
   Wings of Elf, Wings of Spirits, Small Wings of Elf; `BlendMesh 1`: Lighting Sword, the Serpent,
   Bronze and Dragon shields, ...; `-2`: every mesh, e.g. the Staff of Resurrection, Chaos Nature
   Bow, Saint Crossbow). The per-type branches of `RenderPartObjectBody` draw single meshes with
   `RENDER_BRIGHT` (additive) or at partial alpha (the 3rd wings, Flameberge, Elemental Shield, ...).
2. **The texture name.** In the default draw (`RenderBody`) an `_R` mesh is its own blend mesh,
   i.e. additive (`NEWW_R.jpg` of the Wings of Soul). The flags are read after the **first**
   underscore, at most four capital letters, and all of them must be flags (`R` bright, `H`
   hidden, `S` stream, `N` no chrome/level passes, `DC`/`DT` shadow): `NEWW_R.jpg` is bright,
   `wing_3_R.jpg` and `sword_r.jpg` are not. `_S` scrolls only where the code passes a UV offset.
3. **The file kind.** A `.tga` (4 channels) is alpha-tested; a `.jpg` is opaque.

The wings, as checked in the client (the Item Editor preview, 2026-09-24; test textures proved
the modes of the Wings of Elf, Heaven, Soul and Curse):

| Wings | Blended (additive) | Cut-out (.tga) | Opaque | Engine extras |
|-------|--------------------|----------------|--------|---------------|
| Elf `12-0`, Spirits `12-3`, Small Wings of Elf `12-132` | mesh 0 | | | |
| Heaven `12-1`, Satan `12-2` (and their small ones) | | mesh 0 | | |
| Curse `12-41` (and `12-131`) | | | mesh 0 | |
| Soul `12-4`, Dragon `12-5` | mesh 1 (`_R`, pulsing) | | mesh 0 | |
| Darkness `12-6` | an additive chrome layer | mesh 0 | | flares, thunder beams |
| Despair `12-42`, Dimension `12-43` | a chrome layer on mesh 1 | Dimension mesh 2 | the rest | Dimension: flares |
| Storm `12-36` | meshes 0, 1 (1 is a 4-frame UV flip-book) | mesh 2 | | clouds, lights, thunder |
| Eternal `12-37` | | mesh 0 | | lights |
| Illusion `12-38` | mesh 0 (`_R`) | | | flares, particles |
| Ruin `12-39` | mesh 0, plus a layer on mesh 1 | | mesh 1 | particles; a cloth cape on a Magic Gladiator |

Capes are cloth when worn. The Cape of Lord (`13-30`), Small Cape of Lord (`12-130`), Cape of
Fighter (`12-49`) and Little Warrior's Cloak (`12-135`) are not drawn as a model on the
character at all; a cloth simulation with a texture is. The Cape of Lord's worn cloth uses
`Player/DarklordRobe.tga`, **not** the item's `Item/DarkLordRobe` texture, so repainting the item
changes only the inventory and ground look. The Cape of Emperor (`12-40`) and Cape of Overrule
(`12-50`) draw their collar mesh 0 on the character and cloth made from their mesh 1/2 textures;
their other meshes (and the whole Small Cape of Lord) show only under an exactly white light,
i.e. in the inventory.

**What the engine adds must not be painted in.** Every item lists it under `effects` (and a
request under `constraints.render.<key>.effects`):

- the +level look (+3..+6 tinted light, +7 and up chrome and metal passes over the whole model;
  the big wings, capes, jewels and some others are drawn at a fixed level: no glow at all on the
  big wings and capes, the small wings `12-130..135` do get it), the
  excellent shine (an additive pass of the own texture: bright texels glow; not on wings and
  capes) and the ancient shimmer;
- per-type extras: sprites (lights, flares), particles, joints (beams, trails), effect models
  (thunder), pulsing brightness of the blend mesh, UV scrolling or flip-books, extra mesh passes
  with engine textures (chrome, `msword01_r.jpg`), the tip light of a held weapon, the swing trail
  of an attack, cloth capes and the wing animation.

Sprites, particles and joints are world effects: the inventory does not show them (not checked in
the game client). `render_facts.py` reads these from `ItemObjectAttribute`,
`RenderPartObjectEffect`, `RenderPartObjectBody`, `RenderPartObjectBodyColor`, `RenderLinkObject`
and the held-weapon code of `RenderCharacter`; an item with none of them says "No item-specific
effect in the scanned code". Each item has a `status`: `verified-in-client` (checked in the Item
Editor preview, see `verified`), `from-code` (read from the code by the rules above) or
`unverified` (the reading hit a condition it cannot decide: the list under `unverified` says
where; pets and mounts, some armour and event items). Where facts and the game disagree, the game
wins: fix `render_notes.py` and rebuild.

## Concept images

Before a `redesign` request, the owner can have concept images made for an item and pick one:
`tools/item_editor/concepts.py plan` (dry run with the cost estimate) -> `refs` (renders the
current look offline in Blender) -> `run --yes` (OpenAI Images API, hard caps on images and
dollars) -> `sheet` (contact sheet: current look next to the variants) -> `pick <batch> <key> <v>`.
The pick lands in `concepts/<key>/concept.jpg` (committed, small) and goes into the request as
`captures/ref-concept.jpg`. The API key stays in the owner's shell (`OPENAI_API_KEY`); batches
stay in the git-ignored `out/item-concepts/`. Workflow, options and prices:
[`concepts/README.md`](concepts/README.md).

## `catalog.json`

About 2.5 MB. `items` is keyed `"<group>-<index>"` (the item type is `group * 512 + index`). An
entry exists for every item whose model the client loads, including 13 models whose table row
has no name (`in_table: false`); `table_items_without_model` lists the 70 named rows the client
loads no model for (38 of them are the unused `-J` duplicates). Each entry has:

- `key`, `group`, `index`, `name`, `in_table`, `family` (see Tiers).
- `table`: from the client item table: `width`, `height` (inventory slots), `two_hand`,
  `drop_level` (`ITEM_ATTRIBUTE::Level`), `require_level`, `item_slot` and `classes`, the minimum
  class stage per class (`dw`, `dk`, `elf`, `mg`, `dl`, `sum`, `rf`; 0 = cannot use, 1 = base
  class, 2 = second class, 3 = third). Whether a character can equip an item is decided by the
  engine's `IsRequireEquipItem`; the editor calls it instead of re-reading these numbers.
- `openmu`: the server's facts when the export has the item, else `null`: `name`, `drop_level`,
  `maximum_drop_level`, `drops_from_monsters`, `maximum_item_level`, `maximum_sockets`,
  `is_ammunition`, `qualified_classes`, `ancient_sets`, `set_bonus_groups`, `options`,
  `excellent`, `width`, `height`. The server only sends group and number; size, class and
  requirements must match the client table, names and stats on the client are display only.
- `client`: what the client itself knows: `socket_item` (the hard-coded list of
  `CSocketItemMgr::IsSocketItem`), `set_types` (the two set numbers of `ItemSetType.bmd`; the file
  stores `0, 0` for "no set") and `option380` (`ItemAddOption.bmd`, or `null`).
- `badges`: `socket`, `set` (can be ancient), `option380`, `excellent` (can carry excellent
  options) and `max_item_level` (see Tiers for where each comes from).
- `tier`: `value` 1..7, `score`, `score_source` (`openmu-drop-level` or `client-level`),
  `drops_from_monsters`, `family_rank` and `family_size` (position basic -> rare in the family),
  `source` (`computed` or `owner`), `note`, and `computed_value` when the owner overrode it.
- `models`: the item's own model first (`role` `item`), then the extra models the engine uses for
  it: `class-variant` (with `class`: the Summoner, Rage Fighter and Dark Lord replacements of
  armour and helms, only for classes that can wear the item), `left-hand`/`right-hand` (Rage
  Fighter gloves) and `inventory` (a different model drawn in the inventory, with the engine's
  `condition`, often an item level). Each model has `bmd` (on-disk spelling), `folder`,
  `texture_dirs` (the folders the engine opens its textures from, in call order; a texture found
  in several of them comes from the last), `loaded_by` (the C++ function), `textures` (name in the
  BMD -> container path; `null` for `hide*` meshes and missing files), `texture_status` (only the
  names that are `hidden` or `missing`), `structure` (`version`, `meshes`, `bones`, `actions`,
  `action_keys`, `triangles`, `mesh_textures` in mesh order, bind-pose `bounds`) and `original`
  (`revision`: the newest commit of `HEAD` that changed the file; `sha256` of the file).
- `armour_set`: for helm/armour/pants/gloves/boots, `{index, name, parts}`: the parts with the same
  index, named after the armour part; `null` otherwise.
- `shared_with`: texture container -> every other consumer: item keys, and `other:<MODEL_...>`
  for models that are not items (default class bodies, event models, ...).
- `original`: the `original` of the item's own model (what the A/B compare restores).
- `requests`: the item requests that name it (`id`, `status`, `assigned_to`); `client_review`:
  the owner's verdict from `client-review.json` or `null`.

Top level: `counts` (items, per family, per tier, ...), `item_table` (file, layout, named items),
`openmu` (export source and count or `null`), `missing_models`, `missing_textures` (texture names
that no texture folder of the model has; the engine then falls back to an already loaded texture
of the same name, or draws the mesh untextured), `table_items_without_model` and `requests`.

### How Codex uses it

Before touching an item, a worker reads its entry: which files are the item (`models[].bmd`),
which textures they use and who else uses each one (`shared_with`: a shared texture is frozen
unless every consumer is a target), the skeleton and action facts to preserve (`structure`), the
inventory size (`table.width`/`height`) and the `original` to compare against. Requests copy these
facts; `requests/validate_request.py` checks that they still match. Never edit `catalog.json`
by hand.

## Tiers: basic to rare

There is no rarity field in the game data (rarity belongs to one dropped item: level, excellent,
ancient). The catalog therefore ranks item *types* per family. The rule lives in
[`tools/item_editor/tiers.py`](../../tools/item_editor/tiers.py); the editor reads the result
(`tier.value`, `tier.family_rank`) and does not compute it again.

1. **Family**: by item group (0 swords, 1 axes, 2 maces, 3 spears, 4 bows, 5 staffs, 6 shields,
   7 helms, 8 armours, 9 pants, 10 gloves, 11 boots, 12 misc, 13 helpers, 14 potions,
   15 skill-books), with these exceptions: `ammunition` (4-7, 4-15), `wings-1` (12-0..2, 12-41),
   `wings-2` (12-3..6, 12-42, 12-49, 13-30 Cape of Lord), `wings-3` (12-36..40, 12-43, 12-50),
   `wings-mini` (12-130..135), `jewels` (jewels and their bundles in groups 12 and 14),
   `skill-books` (also the orbs, scrolls and crystals of group 12), `socket-seeds` (12-60..129),
   `pets` (mounts and pets of group 13), `jewelry` (rings and pendants of group 13). The exact
   key lists are `FAMILY_KEYS` in `tiers.py`.
2. **Score**: OpenMU `DropLevel` when the export has the item, else the client table's `Level`.
3. **Drops**: OpenMU `DropsFromMonsters`; without the export every item counts as dropping.
4. **Badges**, in this order as tie-breakers: socket item (client list or OpenMU
   `MaximumSockets > 0`), set-capable (OpenMU ancient set groups, else the client
   `ItemSetType.bmd`), 380 option (client `ItemAddOption.bmd`), excellent-capable (an OpenMU
   option named "Excellent ...", else weapons, shields and armour), maximum item level (OpenMU,
   else none).
5. **Order in the family** (`family_rank`): dropping items first, then the ones that never drop
   (shop, event, quest, crafted); within each part by score, then badges (an item with a badge
   after one without, a higher maximum level later), then group and index.
6. **Tier** `T1..T7`: the percentile rank of the score among the family's dropping items (all of
   the family's items when none drops), cut into seven equal bands:
   `tier = min(7, 1 + floor(7 * (below + equal / 2) / n))`, where `n` is the number of reference
   scores, `below` how many are lower than the item's score and `equal` how many are equal to it.
   Equal scores share a tier; a family whose items all share one score (the third wings all drop
   at 150) sits at T4 instead of being called basic. An item that never drops gets the tier its
   score would have among the dropping items, but still sorts after them.
7. **Owner override**: `tiers.json` replaces the tier of any item; the order of rule 5 stays.

### `tiers.json`

Owned by the owner (the editor may write it; agents never do). An object keyed by item key; each
value has `tier` (1..7) and an optional `note`:

```json
{
  "0-19": { "tier": 7, "note": "Divine Sword: quest reward, feels top tier" },
  "14-13": { "tier": 5 }
}
```

The catalog builder refuses unknown keys, fields and tiers outside 1..7. An empty file (`{}`)
means no overrides. After a change, rebuild and commit the catalog with it.

## `client-review.json`

The owner's verdict after looking at an item in the client, written by the editor (**Looks good**
/ **Needs work**), same format as World1's: `{"<key>": {"verdict": "looks-good" | "needs-work",
"note": "...", "date": "YYYY-MM-DD"}}`. It does not exist until the first verdict. A verdict is not
a request.

## `assignments.json`

The coordinator's worker assignments for item requests:
`{"<worker label>": {"request": "<id>", "branch": "codex/item-req-...", "worktree": "MuMain-item-req-..."}}`.
Only the coordinator edits it; see [`requests/README.md`](requests/README.md).

## The OpenMU export

`openmu-items.json` (`mu-openmu-items/1`) was produced with `--postgres database` from the local
OpenMU server (Season 6 data, 676 item definitions). The `--postgres` source runs one read-only
`SELECT` through `docker exec <container> psql -U postgres -w`, which works only when the
container accepts local connections without a password; it never logs in to the admin panel.
Without the file the catalog falls back to client facts (score from the client `Level`, every
item counts as dropping, no maximum item level).
