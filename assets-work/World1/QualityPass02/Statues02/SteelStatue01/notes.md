# SteelStatue01

Independently accepted offline; new client verification pending.

An undercut capital, curved neck profile and raised bronze inscription panel add readable structural depth. The original square foot, gate-adjacent shaft and capital perimeter anchors remain fixed.

- World1 placements: 4; transforms unchanged.
- Triangles: 98 → 274, below the 1,500 prop target.
- Baseline/final bounds: [[-65.92, -65.01, -0.04], [65.87, 66.77, 321.58]] / [[-65.92, -65.01, -0.04], [65.87, 66.77, 321.58]].
- Static rig: original single root name/order/parent, one action, one key and lock metadata pass converter equivalence.
- Every authored triangle matches material, bone, position, UV and winding. Maximum position error 1.66455078e-06; tolerance .0003, UV tolerance .000001. No zero-area geometry or UV triangles.
- Authored-to-raw engine normal direction maximum error 6.13667377e-07, tolerance .001.
- Exact contact/anchor maximum error 0; explicit protected set in validation/contact-contract.json.
- Frozen texture containers match current game and exported bytes. Complete hashes in validation/frozen-textures.json and final-proof.json.
- Packed source.blend includes REF_ORIGINAL with original paintings and REF_BASELINE with merged paintings. Official exporter output is reimported for the main/reverse/light/reduced and wireframe reviews. Actual placement groups include tilted instances and frozen current neighbors; PoseBox01 is an engine-hidden operation marker and omitted explicitly.
- Final export SHA256: `c118a1bd217e809a858e5b6eccef5ef83041d7d136499c3a7acca314415dcbf1`.

The user reported the prior merged Lorencia baseline working in game. No new client verification, collision test or runtime performance benchmark is claimed here.
