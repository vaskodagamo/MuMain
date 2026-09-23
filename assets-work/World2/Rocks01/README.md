# Dungeon rock-spire pair — retain baseline

Decision: retain Object49 and Object50 unchanged. This is a baseline retention checkpoint, not a replacement or accepted new model. The coordinator relayed the independent review's retention decision before any geometry mutation. No candidate source or export was created, and no game file or texture was changed.

Object49 already has a strong asymmetric three-peak silhouette, a tall taper, and clearly separated saddle heights. Object50 supplies a lower, broader counterpart with a distinctive bent central spire. Their varied rotations and sizes form a coherent five-instance cluster; both forms remain readable in its reduced view. A possible broad fracture-plane treatment was considered, then stopped because there was no demonstrated contextual defect justifying replacement. Added facets could make the coherent cave formations more angular without a clear gain, while small detail would disappear at the observed scale.

The close views show stretched stone texture along some tapered surfaces and a soft rounded crest on Object50. These limitations are acknowledged. They do not undermine the recognizable cluster silhouette, and the shared frozen texture is not changed. Reverse evidence reveals the existing open undersides; this is an offline assembly without terrain, not a new hole or a claim of in-client ground verification.

| Asset | Triangles | Placements | Scale range | Rig |
|---|---:|---:|---:|---|
| Object49 | 398 | 235 | 1.00–3.16 | Cone01, one bone/action |
| Object50 | 398 | 41 | 1.00–5.14 | Cone02, one bone/action |

Both official imports have one TileRock01.jpg material, one UV layer, 209 imported vertices, and exactly one bone influence per vertex. Their identity transforms and all original geometry, UVs, normals, names, action data and contact edges remain byte-preserved in the game BMDs. No roundtrip was installed.

## Evidence and reproduction

Base commit: `c40d0b0d313b44b99a4fd84bcac0a81e476f3ed7`. Worktree: `/Users/lukasmac/Documents/claude-test-mumain/MuMain-dungeon-rocks`, branch `codex/dungeon-rocks49-50`.

`baseline/Object49` and `baseline/Object50` contain unchanged BMDs, packed official Blender imports, import logs, provenance and individual front/reverse/reduced images. `baseline/rock-cluster` contains the exact actual-placement assembly and matched source hashes for indices 435, 436, 437, 450, 453. `placements.json` retains all 276 records and the originating readiness-file hash. `baseline/TileRock01.OZJ` is the exact frozen shared texture.

Run from the worktree:

```sh
python3 assets-work/World2/Rocks01/verify_retention.py
BLENDER_USER_SCRIPTS=/Users/lukasmac/Documents/claude-test-mumain/astra-tools/blender-user/scripts BLENDER_USER_CONFIG=/Users/lukasmac/Documents/claude-test-mumain/astra-tools/blender-user/config /Users/lukasmac/Documents/claude-test-mumain/astra-tools/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 --python assets-work/World2/Rocks01/inspect.py
```

The first command checks all supplied image/import/dependency hashes, exact game bytes against the base commit, exact cluster source records, and membership of the five actual transforms in the placement data. Results are in `validation/retention-hashes.json`. The second reads the imports via Blender Python API without saving or mutating them and writes `validation/blender-inspection.json`.

Original baseline rendering was performed through the unchanged official importer and the coordinator's Readiness03 `rock_assemblies.py` / `render_evidence_views.py`; evidence provenance identifies those inputs. This package retains their already rendered, hash-bound outputs. There is no new runtime verification, performance claim, candidate export or texture painting in this checkpoint.
