# Bonfire01 — exposed split-log refinement

Status: candidate under technical and artistic review. No game file installed.

Nine unchanged World1 placements use this six-log bonfire. The prior FireProps01 pass changed the bark painting but kept the original square-section geometry. This bounded geometry pass gives the exposed lower log edges broader chamfers and shaped heels, transitioning into the original square hot ends beneath the additive shell. Each original lowest wood contact survives, and the full bind bounds remain exact. Candidate 362 triangles (324 wood + 38 protected effect), versus baseline 110.

The additive fire_02 mesh remains protected, including geometry, UVs, raw vertex and normal bindings, normals, material ordering and winding. Skeleton Bone01/Bone_fire/Cylinder03/Cylinder04 and the one single-key unlocked action remain unchanged. Both Object1/fire_01.OZJ and Object1/fire_02.OZJ stay byte-identical. No new texture painting, particles or engine edits.

The game uses BlendMesh1 with flickering brightness, plus procedural runtime fire. Offline review approximates only the fixed additive layer and omits procedural particles and runtime lighting. This approximation is applied identically to current and candidate; wood-only views expose the actual geometry changes. Neither is client evidence. New client verification is pending.

Each asset folder retains untouched originals and merged-baseline BMDs, an official baseline import, packed source.blend with REF_ORIGINAL, frozen texture payloads, actual exported BMD and official reimport. Production uses bpy/mathutils with the official tools/blender importer/exporter. Reproduction: `pipeline.py prepare`, then `pipeline.py build`, using MU_BLENDER, MU_BMDCONV and the isolated SourceTools environment configured by the coordinator. `pipeline.py export` skips source rebuilding.

The pipeline verifies complete rig/actions, bounds, mesh ordering, frozen texture containers, valid UVs and triangle geometry, exact inherited baseline exceptions only, every authored triangle's bone/material/position/UV/winding correspondence, and actual raw normal-node directions. `audit_shell.py` independently matches every protected fire_02 triangle with exact raw vertex/normal node IDs and strict separate position/UV/normal limits.

Review includes matched original/current/candidate views, reverse and reduced previews, wood-only variants and wireframe. Actual-placement assemblies use representative smallest/medium scales and the adjacent tilted pair, with unchanged world transforms. A gray 190-unit bar in isolated placement views is a review-only size reference.

Final hash-bound independent verdict and installation status will be added after review. A wood-only improvement that worsens the protected shell presentation is rejected.

Frozen payloads decode as fire_01 512×512 RGB and fire_02 32×32 RGB. Original engine behavior is recorded in `engine-reference.txt`. Initial extracted review JPEGs were invalid because the OZJ duplicated header was retained; official `mu_texture.py unwrap` now runs before every import, and the regenerated evidence uses valid payloads. `texture-decode.json` records decoded dimensions and payload hashes.

The protected normal audit caught double custom-normal quantization during reconstruction. Seeding retained corners directly from baseline normal vectors reduced the maximum raw normal difference to0.00015952, without changing the audit tolerance. Protected raw geometry differs by at most0.000000954 units from float serialization, UVs are exact, and both raw node IDs are exact. `normal-encoding-diagnostic.json` retains the initial diagnosis; `validation/protected-shell.json` records the final passing proof.
