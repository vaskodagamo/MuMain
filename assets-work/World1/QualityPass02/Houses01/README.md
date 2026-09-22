# Lorencia houses: structural and roof study

Geometry-only candidates for House01, House03 and House04, evaluated independently. All thirteen resolved Object1 textures are frozen. No placement, terrain, collision, engine, effect setting or runtime changes are included.

House01 develops actual masonry window surrounds and projecting sills, recessed panels retaining their complete painted motif, entrance pier strips, rafter corbels and chimney collars. House03 develops supported awning headers with knee braces and post collars, moderately warped board spans, a framed wall panel, rafter corbels and chimney collars. House04 is restricted to paint-aligned physical shingle laps. Its dome boundary, dormer openings, all original vertices and every nonroof triangle retain their original contracts.

Current status: all three exports independently accepted offline at the hashes in the retained independent-review reports; client verification pending. The user reported the earlier merged Lorencia baseline working in game; these exports have not been verified in a client.

## Reproduction

Run from the repository root using Blender5.2.2 with official SourceTools3.4.3 enabled. Set `BLENDER`, `MU_BMDCONV`, `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` to installed tools.

```sh
HOUSE_NAMES=House01,House03 python3 assets-work/World1/QualityPass02/Houses01/pipeline.py experiment
HOUSE_NAMES=House04 python3 assets-work/World1/QualityPass02/Houses01/pipeline.py experiment
python3 assets-work/World1/QualityPass02/Houses01/prepare_context.py
```

The pipeline imports retained original and merged-baseline BMDs, rebuilds packed source scenes with excluded REF_ORIGINAL/REF_BASELINE, exports through official tooling, reimports, validates, and renders matched diffuse views. Complete authored triangles are matched one-to-one by material, bone, position and UV. Separate audits preserve root extents, contacts, original anchor points, controlled effect geometry and UVs. House04 additionally checks all forty hierarchical pose matrices, posed bounds and raw normal ownership/directions.

After the candidate passes, use Blender background mode with `--python-exit-code 1 --python` for `final_evidence.py`, `context_review.py` and `effect_review.py`. The same `HOUSE_NAMES` selection limits their asset scope. Context preparation imports immutable neighbor snapshots from the integration worktree and records exact hashes and actual World1 transforms. It never installs or modifies those neighbors. Light03 hidden engine-marker transforms are recorded without rendering their opaque boxes.

The effect views separately approximate House03's light_02 brightness range and House04's scrolling effect at sampled action frames. These previews are explicitly offline approximations, not engine observations. Original raster paintings remain packed unchanged; no texture generation or painting occurred.

First-study source/export/images are retained under `experiments/01`: that direction was rejected for weak game-scale improvement and excessive invisible awning tessellation. The second structural study spends geometry on visible load-bearing headers and shaped masonry surrounds.

House04 official Blender export exposed near-gimbal Euler drift in child bones7/8: maximum measured world-matrix component difference0.0003836, despite unchanged posed bounds. The final packaging step preserves the official roof geometry, restores protected nonroof triangles and uses the exact baseline SMD bind/action inputs through the supported converter manifest path. Strict validators remain unchanged. Separate raw-runtime float and byte-identity reports distinguish exact SMD input preservation from measured binary equivalence.

Untouched original BMDs and merged baseline BMDs are retained separately. The new original-geometry preview uses frozen current materials to isolate geometry changes; the earlier `assets-work/World1/Architecture03/<asset>/original/` retains the untouched original texture bundle and original inspection. All new source scenes explicitly preserve original nonroof corner normals on House04, and the final all40-frame authored-to-raw normal comparison passes with maximum unit-direction difference0.00029052. Final raw bone/action data is byte-identical to baseline.
