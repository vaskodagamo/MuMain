# Items

Everything the item editor and the art builder (Codex, [`ASTRA.md`](../../ASTRA.md)) need to know
about the game's items, and the requests to improve them. The plan behind it is
[`docs/agents/ITEM_EDITOR_PLAN.md`](../../docs/agents/ITEM_EDITOR_PLAN.md) (sections 2, 4 and 5).

```
assets-work/Items/
  README.md            this file
  catalog.json         generated: one entry per item that has a model ("mu-item-catalog/1")
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
# check only (exit 1 when a file is out of date)
python3 tools/item_editor/gen_item_models.py --check
python3 tools/item_editor/build_item_catalog.py --bmdconv <bmdconv> --check
# tests
python3 -m unittest discover -s tools/item_editor/tests
```

Commit the regenerated files together with whatever changed them. The catalog is deterministic
for a commit (sorted keys, no timestamps); it depends on `openmu-items.json`, so rebuild it after
refreshing the export. A worker branch that installs new textures or models does not rebuild the
catalog; the coordinator does that when a request is accepted.

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
