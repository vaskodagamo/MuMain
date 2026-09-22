# Masonry dressed-cap refinement — QualityPass02

Production artist B; branch `codex/lorencia-q02-furniture` retains the name assigned before the coordinator changed the batch to masonry. Baseline: merged main `7b808473`. Final coordinator decision: retain all three baseline game BMDs and checkpoint studies only. No game changes remain; no asset is counted completed. HouseEtc01 received limited visual acceptance from the independent reviewer, but its side-edge notch violates the strict modular edge requirement and is rejected for integration.

The final candidate retains the original carved-face geometry and cuts broad five-unit chamfers into selected upper masonry edges. The beveled corner profile is deliberately simple and reads through real geometry and diffuse shading. Upper pier edges are split at the measured cap region before beveling. Global bind bounds, central support surfaces, skeleton/actions and mesh order remain intact. The rejected House cap adds a shallow V-shaped side-edge recess at the stacked joint: central support and front alignment remain, with no open separation. This dressed-joint notch is a real seam-profile change and caused coordinator rejection. The frozen 512px diffuse paintings are unchanged; new bevel faces receive coherent planar UV projections from the corresponding original stone faces.

| Model | World1 instances | Original / merged / candidate triangles | Exact texture dependencies under Object1 |
|---|---:|---:|---|
| HouseEtc01 | 52 | 34 / 50 / 54 | c_wall04.OZJ, c_wall06.OZJ |
| StoneMuWall02 | 32 | 44 / 76 / 132 | c_wall06.OZJ, c_wall05.OZJ, c_wall04.OZJ |
| StoneMuWall03 | 45 | 52 / 84 / 194 | c_wall04.OZJ, c_wall06.OZJ |

Only these three BMDs are owned; all textures are frozen. No terrain, placement, collision, engine, shared runtime or client operation occurred. The pre-existing in-game success was reported by the user for the merged baseline, not personally observed by this worker. New candidate client checks remain pending.

## Retained evidence

Each asset has untouched `original/`, untouched merged `baseline/` plus a fresh official import, packed `source.blend` with excluded `REF_ORIGINAL` and editable `REF_HIGH_POLY`, frozen diffuse JPEGs/containers, official BMD exports, and actual export reimports. Original/current/candidate converter info and comparisons are retained under `validation/`.

All converter/model/texture checks pass. Skeleton-only comparisons are EQUIVALENT; full models are DIFFERENT because the cap geometry is intentionally changed. Each model retains its original named root, original one-frame action and lock=0. Bounds and exact material ordering match. Source-to-export audits match every triangle's material, UV corners, winding, positions and bone assignments; both directions of vertex correspondence pass. Raw BMD vertex and normal nodes are checked directly.

UV area and triangle areas are nonzero. Every newly authored corner normal agrees with face winding. StoneMuWall02 retains exactly two pre-existing c_wall05 custom-normal corner anomalies: incidence is 2 in original, merged and candidate. Their material, positions, UVs and winding correspond to original triangles 17/23; normal component differences are below 0.0002 from official normalization. This inherited condition is explicit, not described as universally positive winding. Two zero-area faces produced by beveling StoneMuWall03 were removed before export; no tolerance was weakened.

Start with `comparison-sheet.jpg`, then the per-asset matching current/candidate, reverse, wireframe and quarter-resolution images. `review-assemblies/` uses actual unchanged World1 positions, rotations and scales with unchanged neighboring modular exports. All images are OFFLINE BLENDER, never client evidence. `candidate-sha256.json` records the exact exported files and evidence after the last successful build.

## Rejected studies

`rejected-study/` preserves the validated earlier motif-carving/stepped-panel experiment, exported BMDs, sources, comparison sheet and independent rejection. Its high triangle count did not improve the main silhouettes at normal camera scale. It was never installed and is not counted complete. The earlier tray-like inset experiment was also rejected; `rejected-v1/` warns that cached intermediate image provenance is uncertain and must not support acceptance.

## Reproduce

Set `MU_BLENDER`, `MU_BMDCONV`, `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` to Blender 5.2.2, the existing converter and isolated Source Tools 3.4.3 installation. Execute from this isolated worktree:

1. `pipeline.py prepare` imports the merged baseline through the official importer.
2. `pipeline.py build` runs `cap_source.py` using bpy/bmesh, the official exporter, converter and raw-binding checks, official reimports, complete source/material/UV matching, matching-camera renders, actual placement assemblies and strict UV/winding checks.
3. `finalize_evidence.py` refreshes current-versus-candidate comparisons and hashes.
4. `assemble_review.py` creates the labeled sheet using Pillow.

The older `build_source.py` and `relief.py` reproduce the rejected study only. Production reuses legacy repository utility modules read-only; no engine/converter behavior changed. No image generation or repaint occurs in this geometry-only batch.

## Final outcome

All candidates are rejected for integration. HouseEtc01 was briefly copied only into the isolated source checkout after the reviewer’s incremental acceptance, then immediately restored when the coordinator rejected the seam change. No runtime/client use occurred. StoneMuWall02/03 were never installed. All three source game files exactly match merged baseline. Continue future masonry work with unchanged modular edge profiles; pursue independent high-impact assets meanwhile.
