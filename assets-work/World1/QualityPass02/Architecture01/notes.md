# Architecture01 — structured wall bays and shingle laps

Production artist A. Branch `codex/lorencia-q02-architecture`, based on merged `7b808473`.

Four assets cover 23 World1 placements. All textures remain byte-identical. Current bounds, modular extremities, openings, bone/action metadata, material ordering and roof fade IDs remain unchanged. The ashlar wall family gains visible structural framing; roof caps gain modeled course lips aligned to the existing painting.

| Asset | Placements | Current → candidate triangles |
|---|---:|---:|
| HouseWall01 | 3 | 52 → 444 |
| HouseWall04 | 8 | 88 → 872 |
| HouseWall05 | 8 | 54 → 478 |
| HouseWall06 | 4 | 52 → 262 |

All converter, source/export triangle, UV, raw normal-binding, single-bone, frozen texture and modular anchor checks pass. New client review remains pending; the user's baseline client report does not apply to these replacements. Independent artistic and technical review ACCEPTED all four final hashes; see `independent-review.json`.

No terrain, shared texture, engine, runtime, placement, excluded model, or coordination file was modified. Game installation is restricted to the four owned Object1 BMDs. Actual World1 assemblies omit unrelated buildings, terrain and collision; inspect `review/*-placements.json` for exact records.

## Reproduction

Run explicitly from this worktree with environment variables `BLENDER`, `MU_BMDCONV`, `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` pointing to the coordinator's official Blender 5.2.2 / Source Tools 3.4.3 installation. Python commands use `PYTHONDONTWRITEBYTECODE=1`.

1. `python3 assets-work/World1/QualityPass02/Architecture01/pipeline.py prepare`
2. Same entrypoint with `build`, then `export`, then `validate`, then `review`.
3. Blender `-b --python-exit-code 1 --python assets-work/World1/QualityPass02/Architecture01/audit_source.py`.
4. Python `validate_anchors.py`; Pillow-enabled Python `assemble.py`; Python `write_notes.py`.
5. `install.py` installs only four hash-guarded owned BMDs into this isolated source checkout after review.

The first candidate exposed a UV-layer join mismatch and an eave bound violation; both were fixed in authored geometry and revalidated. No converter/engine changes or weakened validation thresholds were used. Final roofs use a 5-unit perimeter restraint and 3-unit maximum recessed lap depth; wall joinery remains inside the existing envelope. Sources retain REF_ORIGINAL and REF_BASELINE. No unnecessary high-poly source is used: the final bevels and laps are directly editable geometry.
