# Item Editor (MuEditor)

A table of every item the client knows (`Data/Local/<lang>/Item_<lang>.bmd`): name, size, damage,
defence, requirements, class stages, resistances and prices, edited in place and saved into the
game's own file. It exists only in editor builds (`ENABLE_EDITOR`), so the normal game is unaffected.
The quick start is for the Mac; everything after it is reference. The Map Editor's guide,
[MAP_EDITOR.md](../MapEditor/MAP_EDITOR.md), covers building, logs and the editor in general.

### Quick start (macOS)

1. **Build** (repository root; the first build takes several minutes):
   ```sh
   PATH=/opt/homebrew/bin:$PATH cmake --preset macos-arm64-mueditor -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
   PATH=/opt/homebrew/bin:$PATH cmake --build --preset macos-arm64-mueditor-release --target Main
   ```
2. **Open the Item Editor without a server:**
   ```sh
   cd out/build/macos-arm64-mueditor/src/Release/Main.app/Contents/MacOS && ./Main --editor --items
   ```
   The client opens Lorencia offline (add `--world N` for another map) with the Item Editor open and
   the Map Editor closed. In a game session, **F12** opens the editor and the toolbar's **Item Editor**
   button shows the window.
3. **Find an item:** type in **Search** (part of the name, any letter case). **Columns** picks which
   fields are shown; the choice is kept in `MuEditor/MuEditor.ini`. **Freeze Index/Name** keeps those
   two columns in view while you scroll sideways.
4. **Edit:** click a cell and type. The change is live at once (the running game uses the same
   table) and is logged in the Editor Console. A name holds at most 29 bytes (letters outside
   A-Z take two or more); the cell stops there.
5. **Save:** **Save Items** writes the game's file and your checkout's
   `src/bin/Data/Local/Eng/item_eng.bmd`, and first copies the file it replaces to
   `out/editor-backups/<time>/`. The lines under the buttons list the paths; the Editor Console lists
   every changed field. Nothing edited means nothing is written.
6. **Commit or undo** like code: `git status` shows `src/bin/Data/Local/Eng/item_eng.bmd` modified;
   `git add` + `git commit` keeps it, `git checkout -- src/bin/Data/Local/Eng/item_eng.bmd` throws the
   edit away. The game's copy keeps the edit until the next build copies `src/bin/Data` over it
   again (step 1's build command), so rebuild before you start the client again.
7. **Copy a row:** select it (click any of its cells), then **Cmd+C** (Ctrl+C on Windows) copies it as
   `Field = value` text plus a CSV line, for pasting into a note or a request.

**Stats and names are only half of it:** the server (OpenMU) sends just group and number for each
item. Size, class and requirements must match the server's item definitions, or the game shows items
it cannot place or equip; names, damage and prices on the client are display only. A change you want
to keep in play must be made in OpenMU too (admin panel, Items).

### Files

| What | Where | Notes |
|------|-------|-------|
| Item table the game loads | `Data/Local/<lang>/Item_<lang>.bmd` next to the executable | `<lang>` is `LanguageSelection` in `config.ini` (`Eng`). On disk the file is `item_eng.bmd`; the editor saves under that spelling. |
| Repository copy | `src/bin/Data/Local/Eng/item_eng.bmd` | Written on every save; this is the file git tracks. |
| Backup of the repo file | `out/editor-backups/<YYYYMMDD-HHMMSS>/Data/Local/Eng/item_eng.bmd` | Taken before the repo file is replaced; `out/` is not in git. |
| Backups next to the game's file | `Data/Local/Eng/item_eng.bmd_N01_Y....bak` | The data layer's own last-ten backups, inside the build output only. |
| **Export as S6E3** | `Data/Local/<lang>/Item_S6E3.bmd`, copy in `out/editor-exports/` | The table in the legacy S6E3 layout for other tools; the game does not read it. |
| **Export as CSV** | `Data/Local/<lang>/Item.csv`, copy in `out/editor-exports/` | UTF-8 with BOM, one row per named item, field names as in the code. |
| Column choice | `MuEditor/MuEditor.ini`, section `[ColumnVisibility]` | Next to the executable. |
| Logs | `MuError.log`, `MuEditor/MuEditor_YYYYMMDD.log` | The save's paths and the changed fields. |

Without a checkout above the game folder (a copied `Main.app`), a save keeps a copy next to the
executable instead (`Local/Eng/item_eng.bmd`); `MU_EDITOR_REPO_ROOT=/path/to/repo` points the editor
at a checkout. See [Where saves go](../MapEditor/MAP_EDITOR.md#where-saves-go).

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
- **Offline there is no hero or inventory,** so an edited item cannot be seen in the game yet;
  the preview comes in a later milestone (docs/agents/ITEM_EDITOR_PLAN.md).
