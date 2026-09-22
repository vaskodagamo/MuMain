# StoneStatue03

Independently accepted offline; new client verification pending.

Three broad closed feather lobes replace each angular wing slab, with coherent shoulder attachment and curved overlapping growth. Rounded arm interiors and broad robe folds retain the original pose. The complete plinth and head anchors remain unchanged.

- World1 placements: 4; transforms unchanged.
- Triangles: 248 → 1353, below the 1,500 prop target.
- Baseline/final bounds: [[-72.77, -53.36, -0.08], [74.61, 39.88, 264.77]] / [[-72.77, -53.36, -0.08], [74.61, 39.88, 264.77]].
- Static rig: original single root name/order/parent, one action, one key and lock metadata pass converter equivalence.
- Every authored triangle matches material, bone, position, UV and winding. Maximum position error 4.99999999e-07; tolerance .0003, UV tolerance .000001. No zero-area geometry or UV triangles.
- Authored-to-raw engine normal direction maximum error 0.000479095784, tolerance .001.
- Exact contact/anchor maximum error 0; explicit protected set in validation/contact-contract.json.
- Frozen texture containers match current game and exported bytes. Complete hashes in validation/frozen-textures.json and final-proof.json.
- Packed source.blend includes REF_ORIGINAL with original paintings and REF_BASELINE with merged paintings. Official exporter output is reimported for the main/reverse/light/reduced and wireframe reviews. Actual placement groups include tilted instances and frozen current neighbors; PoseBox01 is an engine-hidden operation marker and omitted explicitly.
- Final export SHA256: `ddfa5d87568039df181dcf24831bd64b4c573ebccfe04c1c9973dba785bac8f3`.

The user reported the prior merged Lorencia baseline working in game. No new client verification, collision test or runtime performance benchmark is claimed here.
