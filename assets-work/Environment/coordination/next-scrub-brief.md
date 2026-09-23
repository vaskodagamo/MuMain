# QualityPass02 scrub production brief

Independent reviewer, 2026-09-22. Baseline `7b808473`; no game or shared-document writes. Read current final images, Scrub01 source/validation SMD geometry, retained notes, alpha sheet, dependency map, raw-normal audit and engine references. Current BMDs byte-match retained Scrub01 exports; current final-render provenance matches BMDs and textures.

## Dispatch and ownership

One artist can own this four-model batch with **geometry-only initial scope**:

- `src/bin/Data/Object1/Tree09.bmd`: five tall-grass tufts, 42 placements, 240 triangles, one mesh `tree_07.tga`; five independent roots `Cylinder04` through `Cylinder08`, all parent -1.
- `src/bin/Data/Object1/Tree10.bmd`: ten tufts, 250 placements, 480 triangles, one mesh `tree_07.tga`; ten independent roots `Cylinder04` through `Cylinder13`, all parent -1.
- `src/bin/Data/Object1/Grass03.bmd`: four scrub clumps, 119 placements, 128 triangles; mesh0 `tree_01.tga` 96 triangles, mesh1 `tree_02.tga` 32 triangles. Four roots `Sphere04` through `Sphere07`, all parent -1.
- `src/bin/Data/Object1/Grass04.bmd`: spread four-clump variant, same slots/counts/roots, 119 placements.

All four have **one action, one key, lock=0**. These are multi-root rigid clusters, not one-root props. Preserve exact original bone names/order/parents and bind/action transforms. Each tuft/clump stays on its own bone.

Freeze complete resolved textures initially:

| Container | Consumers in Object1 | Current SHA-256 |
| --- | --- | --- |
| `src/bin/Data/Object1/tree_07.OZT` | Tree09, Tree10 | `2d39a05c692b757ab88eb2f1b0fe4af483c2b86ba75e206354a8c010bb9c6a9f` |
| `src/bin/Data/Object1/tree_01.OZT` | Grass03, Grass04 | `8cf7925f6ded30b83a2ecf6856617b5c823e5abb7aaccea7acb73fc7403ea0d6` |
| `src/bin/Data/Object1/tree_02.OZT` | Grass03, Grass04 | `472a7d0305c6af309c431cd62c8adc8301a16ef8d97499ef058dd87282b7a4be` |

All three are 512×512, 32-bit uncompressed bottom-left TGA in OZT. Original 32×32 graded alpha was bilinearly enlarged. Do not confuse `tree_01.OZJ`/`tree_02.OZJ`, which serve other trees, with these alpha containers. Freeze every other BMD/texture and World1 terrain/placements/collision. Use a unique `assets-work/World1/QualityPass02/Scrub01/` deliverable directory in assigned isolated worktree.

## Observed defect and useful remedy

Current Tree09/10 upper green blades flare above a pronounced mid-height cinch, while the lower straw shell flares again into a solid polygonal base. The result reads as tied brooms more than growing tussocks. This is geometry/UV silhouette structure; repainting the already coherent olive/dry-straw colors alone cannot fix it.

Each tall tuft is 48 triangles with 36 distinct SMD positions on its own root (verified root0; repeated family structure). Existing UV rows at approximately V=0.0005, 0.4887/0.5212, 0.9995 establish lower/upper strip roles. Do not apply generic subdivision: reshape the radial profile so growth springs naturally from each preserved ground-contact footprint through gently bowed stems into asymmetric splayed tips. Reduce the obvious waist with purposeful intermediate arcs. Break the large lower polygon faces into several overlapping, gently curved strips where this removes the solid skirt, using current alpha artwork; do not merely add dozens of flat crossing cards. Keep each cluster's measured bounds/contact footprint and preserve total spatial distribution. Use modest variation between tufts, never random displacement/noise. Avoid introducing a new horizontal alpha seam at the current mid UV row.

Grass03/04 have layered shell/leaf cards with only seven primary UV positions for mesh0 and four for mesh1 in original info. Each root0 has 44 distinct SMD positions across two slots. Their current outer outline is an angular dense conical clump. Reshape selected outer lobe/card profiles into bent leafy boughs with clear gaps, preserving low contact points, height and clump centers. Keep a coherent dense core rather than making floating sprigs. At least one side/reverse view is necessary to catch flat-card disappearance.

**Painter decision:** start geometry-only; current leaf and stalk artwork has appropriate color/material roles. Its old soft alpha limits crisp silhouette potential but is not proven to require immediate repainting. If geometry cannot remove opaque lower skirts or card halos while maintaining density, propose a focused alpha refinement within the same two complete dependency groups. That would need new explicit writable texture ownership from coordinator, retained editable mask/color layers, full consumer review and light/dark composites. Do not silently threshold alpha. No new raster painting is needed for the first geometry experiment.

## Spatial contract and placement scale

Baseline bind bounds (min -> max):

- Tree09: `[-123.82,-129.51,0.56] -> [156.32,111.42,192.24]`.
- Tree10: `[-199.56,-175.26,0.44] -> [134.86,178.79,159.08]`.
- Grass03: `[-178.46,-159.31,-3.08] -> [136.39,122.73,131.26]`.
- Grass04: `[-216.27,-78.82,-2.90] -> [165.65,88.88,115.98]`.

Tree09/10 placements all scale 1. Grass03 scales 0.42–1; Grass04 0.64–1.40. Review small and large real placed scales, not just normalized beauty views. Placements include nonzero X rotations and many rotations beyond one turn; preserve stored transforms verbatim. Exact complete 530 records are in `assets-work/World1/coordination/dependency-map.json` and retained Scrub01 per-asset `original/placements.json`.

Useful first placements: Tree09 tile (24.92,132.20), Tree10 (14.5,166.0), Grass03 (5.0,28.5), Grass04 (14.87,27.52). Select actual adjacent records for mixed-clump assembly instead of moving objects to attractive arbitrary locations.

## Engine and raw binding checks

`MapManager.cpp:1029–1031` loads the Tree/Grass series in Object1. Enum IDs: Tree09=8, Tree10=9, Grass03=22, Grass04=23. Lorencia setup at `ZzzObject.cpp:4611–4674` has no targeted special case for these four; special sway/collision applies only Tree01/02, seating applies Tree07. Keep original mesh order and standard TGA alpha behavior; no additive, scrolling, hidden flag or glow should be introduced.

A broad `MODEL_TREE01`/`MODEL_GRASS01` search across source finds loader, Tree01/02 motion, Tree07 seating, and enum declarations only. Numeric type8/9/22/23 cases in other map contexts do not imply Lorencia behavior. `ActionObject` has generic type9 handling behind an event-world gate; its callers are Blood/Chaos Castle event flows, not normal Lorencia vegetation. Do not change type IDs to avoid any control.

Current retained raw-normal audit: Tree09, Grass03/04 have zero shared-normal ownership cases. Tree10 has **two** shared-normal cases with zero world-direction delta over bind and its single key. This is safe legacy sharing, not proof a new export is safe. Reaudit final BMD normal-node ownership and world normal direction against the owning vertex bone. Differently rotated Cylinder roots mean deduplicating a normal across roots can change shading even with a static action. Prefer exact intended normal bone where possible; otherwise prove the direction is invariant. Do not rely on SMD compare alone (SMD expansion uses vertex bone for normals).

## Acceptance and evidence

Below 1500 triangles each; target economical shell/strip design closer to baseline than ceiling. No triangle increase is itself a quality gain. Retain untouched original plus merged baseline, packed source with REF_ORIGINAL, reproduction script, actual official export/reimport, texture hashes/checks, full converter info/validate/compare, exact rig/action checks, per-bone binding proof and raw normals audit. Verify every source/export triangle by material, bone, position and UV, not only vertex proximity.

Supply current-versus-candidate matching diffuse views at full and reduced scale, side/reverse view, wireframe and actual-placement assembly. On Tree09/10 the reduced preview must clearly lose the tied-broom waist without becoming fluffy blobs or a wall of cards. On Grass03/04 it must gain organic lobe shape without losing ground density/readability. Keep opaque/transparent parts correct over light and dark backgrounds. Client baseline is user-reported working; all new client checks remain pending.

Current BMD hashes: Tree09 `148d3f262f26d320339e0ab809b0db35c77d93f12518e919af3a6c9d2ccef11a`; Tree10 `efc5e8c23a602b7150324d0c7122fb15bcebf416e0cff35a84ebfb7ba7cb99fd`; Grass03 `7d8c7dad65f330d39687e43b35623eaec91d06ec6afb5bf117fef45d34c6794b`; Grass04 `48cbd9087f3197a701bb5e8c42698d2b9bb48bb1d68d7ddcc04e1335eb2c9c5b`.
