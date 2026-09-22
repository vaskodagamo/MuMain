# Bonfire01 — exposed split-log refinement

Status: independently accepted at the source and BMD hashes in `candidate-manifest.json`. The owned Bonfire01.bmd is installed in the isolated worker checkout; coordinator integration remains separate.

Nine unchanged World1 placements use this six-log bonfire. The prior FireProps01 pass changed the bark painting but kept the original square-section geometry. This bounded geometry pass rebuilds the exposed logs as quarter-split firewood: two flat split faces meeting at the original heel and a broad rounded bark arc, transitioning into the original square hot ends beneath the additive shell. Each original lowest wood contact survives, and the full bind bounds remain exact. The baseline has 110 triangles; the final candidate count is recorded in `Bonfire01/validation/source.json`. The 38 effect triangles remain protected.

The additive fire_02 mesh remains protected, including geometry, UVs, raw vertex and normal bindings, normals, material ordering and winding. Skeleton Bone01/Bone_fire/Cylinder03/Cylinder04 and the one single-key unlocked action remain unchanged. Both Object1/fire_01.OZJ and Object1/fire_02.OZJ stay byte-identical. No new texture painting, particles or engine edits.

The game uses BlendMesh1 with flickering brightness, plus procedural runtime fire. Offline review approximates only the fixed additive layer and omits procedural particles and runtime lighting. This approximation is applied identically to current and candidate; wood-only views expose the actual geometry changes. Neither is client evidence. New client verification is pending.

Each asset folder retains untouched originals and merged-baseline BMDs, an official baseline import, packed source.blend with REF_ORIGINAL, frozen texture payloads, actual exported BMD and official reimport. Production uses bpy/mathutils with the official tools/blender importer/exporter. Reproduction: `pipeline.py prepare`, then `pipeline.py build`, using MU_BLENDER, MU_BMDCONV and the isolated SourceTools environment configured by the coordinator. `pipeline.py export` skips source rebuilding.

The pipeline verifies complete rig/actions, bounds, mesh ordering, frozen texture containers, valid UVs and triangle geometry, exact inherited baseline exceptions only, every authored triangle's bone/material/position/UV/winding correspondence, and actual raw normal-node directions. `audit_shell.py` independently matches every protected fire_02 triangle with exact raw vertex/normal node IDs and strict separate position/UV/normal limits.

Review includes matched original/current/candidate views, reverse and reduced previews, wood-only variants and wireframe. Actual-placement assemblies use representative smallest/medium scales and the adjacent tilted pair, with unchanged world transforms. A gray 190-unit bar in isolated placement views is a review-only size reference.

The first 362-triangle chamfer candidate was rejected for insufficient silhouette gain and is retained in `study-v1/`. Final hash-bound acceptance is retained in `independent-final-review.json`. A wood-only improvement that worsens the protected shell presentation is rejected.

Frozen payloads decode as fire_01 512×512 RGB and fire_02 32×32 RGB. Original engine behavior is recorded in `engine-reference.txt`. Initial extracted review JPEGs were invalid because the OZJ duplicated header was retained; official `mu_texture.py unwrap` now runs before every import, and the regenerated evidence uses valid payloads. `texture-decode.json` records decoded dimensions and payload hashes.

The protected normal audit caught double custom-normal quantization during reconstruction. Seeding retained corners directly from baseline normal vectors reduced the maximum raw normal difference to 0.00015952, without changing the audit tolerance. Protected raw geometry differs by at most 0.000000954 units from float serialization, UVs are exact, and both raw node IDs are exact. `normal-encoding-diagnostic.json` retains the initial diagnosis; `validation/protected-shell.json` records the final passing proof.

Final candidate: 518 triangles (480 wood, 38 protected additive shell). Full bind bounds: min (-57.606201, -54.829102, -6.945099), max (62.040100, 55.015900, 55.954498), unchanged. The six measured lowest wood contacts and original hot-end corners remain. `candidate-manifest.json` binds the editable source, BMD and frozen texture hashes to final evidence. `Bonfire01/validation/packed-textures.json` proves packed image bytes equal the decoded frozen payloads and REF_ORIGINAL is retained.

The square glowing hot ends remain a deliberate compatibility limit. The accepted improvement is exposed split-log silhouette and coherent longitudinal grain, not a redesign of the fire effect. Procedural runtime flames and new client verification remain unavailable.
