# Object48: single-component rounded bone study

The candidate changes one detached long bone in this eight-piece Dungeon scatter. It does not remodel the hand/foot silhouettes or any of the other seven pieces. Independent review accepts this as a localized refinement: fuller rounded ends and a tapered shaft remain discernible in the isolated reduced view. Cluster-distance gain is modest. The acceptance does not authorize propagation or imply a whole-family remake. See `independent-final-review.json`. No game file is installed in this worker checkout; integration remains the coordinator’s responsibility.

The baseline is the original 284-triangle model at `7b808473`, also unchanged at this worktree's integration base `5577176b`. `baseline-sha256.json` records the exact BMD and frozen `Object2/bons.OZJ`. The latter has nine known consumers and is byte-identical in the candidate package and packed Blender image. Object45/46 and every other game asset remain untouched.

## Artistic change

Original component 0 contains 32 faces and 18 vertices. Its four diamond cross-sections and pointed caps produce a bar-like shaft and abrupt angular ends. The study uses eight-sided sections, a gently narrowed middle and monotone longitudinal profiles through measured end widths. The whole model is 428 triangles, including 176 on the edited component and the original 252 on other pieces.

All 18 original component points remain as support controls. New rings keep the same arrangement and projected bone-paint strip, preserving original UV anchors and interpolating additional surface coordinates. Full and reverse diffuse views show a fuller end and continuous shaft; the reduced cluster gain is more restrained because only one of eight pieces changes. This is not a claim that the whole remains family has been remade.

## Technical evidence

- Official Blender importer/exporter and Source Tools; Blender Python mesh API for authored changes. Original raw 32-byte model name is preserved through a metadata-only byte argument adapter. No engine, converter or official tool modifications.
- One `Box14` root, one one-key action, same frame/lock/pose, one `bons.jpg` mesh, 128×128 frozen diffuse. `source.blend` packs the exact JPEG and retains a hidden `REF_ORIGINAL` mesh.
- All 385 placements retain whole-model bounds exactly. Edited-component support bounds differ by at most 0.0000019074 world units; all 18 original anchors remain. Original hands, feet and seven other components are exact by position, UV, bone and cyclic winding.
- Every authored triangle matches the actual reimported export: maximum position error 0.000002568; UV error 0.000000634, below separate 0.0003-world/0.000001-UV limits. One full-weight bone per vertex checked.
- Authored corner normals independently match raw BMD normal-node world directions within 0.001. Untouched normals exactly match an unchanged official import/export control. Their maximum 0.000389484 difference from the raw baseline is inherited custom-normal codec quantization, not accepted new drift.
- Eight old nonpositive corner-normal incidences on untouched geometry are matched explicitly: baseline faces 124, 128, 137, 139, 266, 270, 279, 281. No new incidence or zero-UV face. New bone is a closed oriented manifold with Euler characteristic 2; all eight disconnected components remain.
- Converter `compare` correctly reports DIFFERENT for 32 replaced triangles, while bone distance and differing names are zero. Converter validation, exact raw header/action checks, texture checks and actual export reimport pass.

`Object48/validation` contains measured reports and converter logs. `Object48/review` contains original/current/candidate diffuse, reverse, reduced and wire views. `review-assemblies` compares the exact recorded six-model remains cluster and representatives of all six nonzero normalized pitch families, including inverted pitch. All 385 transforms are checked numerically; the renders are samples, not 385 individual pictures.

These are offline diffuse renders without terrain or client verification. No runtime installation or client observation occurred.

## Reproduction

Set `MU_BLENDER`, `MU_BMDCONV`, `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` to the verified Blender 5.2.2 / Source Tools 3.4.3 environment. From the repository root:

```sh
python3 assets-work/World2/Remains48/snapshot.py
python3 assets-work/World2/Remains48/pipeline.py prepare
python3 assets-work/World2/Remains48/pipeline.py build
"$MU_BLENDER" --factory-startup -b --python-exit-code 1 --python assets-work/World2/Remains48/assemblies.py
python3 assets-work/World2/Remains48/comparisons.py
```

The comparison script requires Pillow and only lays out rendered evidence; it does not paint textures. The pipeline uses existing repository review utilities and raw BMD readers without modifying them. Strict source/raw-normal/packed/contact audit helpers were adapted from the previously reviewed Pier01 and Pottery01 workflows. `official_io.py` changes only legacy metadata transport around the official importer/exporter.
