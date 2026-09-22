# Lorencia statues — second quality pass

Independently accepted offline; new client verification pending. Only StoneStatue01, StoneStatue03 and SteelStatue01 are in this batch. Waterspout01, StoneStatue02, tombs and every texture are read-only dependencies. The merged baseline is the exact `7b808473` game blob, retained beside each original model and original painting.

The pillar is a carved human relief on a tall stone shaft. The angel is a stone sculpture on a square plinth. The memorial is dark carved stone with a bronze inscription plate; its filename does not justify a generic metallic treatment.

The final candidates passed independent visual and technical review at the exact hashes recorded in independent-review.json. Earlier user-reported client verification applies only to the prior baseline.

## Studies

- Angel study 1: technically valid, rejected for wedge-like wings and angular robe shading.
- Angel study 2: improved continuous robe and rounded arms, but dense wing tessellation still failed to establish broad feather masses. Retained under `experiments/02-angel`.
- Angel study 3: broad closed feather lobes established the shape, but detached shoulder roots and a pale atlas sliver required correction. Retained under `experiments/03-angel`.
- Angel study 4: rooted, overlapping lobes with explicit mapping into the original dark feather island; rounded arm interiors and broad continuous robe planes. Accepted offline after final placement review.
- Pillar study 1: deeper relief and capital undercut too subtle at reduced scale. Retained under `experiments/01-pillar`.
- Pillar study 2: adds a carved arch around the original figure while retaining its painting and pose. Accepted offline after final placement review.
- Memorial study 1: stepped undercut capital and projected bronze plate approved directionally; exact cap endpoint precision is validated; final placement review passed.

## Contracts and evidence

All three use one root bone and one static action/key. Material order, names, UV set count and frozen texture bytes remain unchanged. The angel plinth and head anchors, pillar outer shell/cap/base anchors and memorial base/capital perimeter anchors are checked separately from overall bounds. Candidates go through the official Blender importer/exporter and BMD converter, then are reimported for every visual review. A cyclic triangle audit checks every authored corner's material, root, position, UV and winding; raw engine normal directions are compared to the authored split normals.

Matching main/reverse/light/reduced images live under each model's `review` directory. Actual placement context uses frozen neighbor snapshots under `context`. These are offline diffuse previews without terrain, collision, runtime effects or client lighting.

The prop target is at most 1,500 triangles (`docs/agents/ASSET_REGENERATION_PLAN.md`); the 15,000 converter ceiling is not a production target. Final counts and reproducible accepted entrypoints will be recorded after review.

## Reproduction

Use Blender 5.2.2 with Blender Source Tools 3.4.3 enabled. Set `BLENDER` to its executable, `MU_BMDCONV` to the built repository converter and the isolated `BLENDER_USER_SCRIPTS` / `BLENDER_USER_CONFIG` paths used for the project. Run from the repository root:

```sh
python3 assets-work/World1/QualityPass02/Statues02/pipeline.py experiment
python3 assets-work/World1/QualityPass02/Statues02/prepare_context.py
"$BLENDER" -b --python-exit-code 1 --python assets-work/World1/QualityPass02/Statues02/final_evidence.py
"$BLENDER" -b --python-exit-code 1 --python assets-work/World1/QualityPass02/Statues02/context_review.py
python3 assets-work/World1/QualityPass02/Statues02/final_checks.py
```

Run the two final rendering entrypoints in fresh Blender processes. Set `STATUE_NAMES` to a comma-separated subset for focused work. `CONTEXT_GROUPS` similarly selects individual placement groups. The final build dispatch is pillar2 / angel4 / piers (SteelStatue01); earlier studies are retained as evidence, not reproduction targets. This batch's helpers import read-only mesh and validation utilities from earlier accepted batches.

Final geometry counts are 212 / 1353 / 274 triangles for pillar / angel / memorial. Texture containers remain byte-identical. Independent review reports and current hashes belong beside this README; `document.py --accepted` requires a retained review containing each exact export hash.
