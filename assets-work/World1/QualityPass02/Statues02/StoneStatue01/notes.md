# StoneStatue01

Independently accepted offline; new client verification pending.

A carved arched niche frames the existing human relief. Measured figure depth and an inward capital profile support the painted identity while the original outer cap, shaft anchors and foot remain fixed.

- World1 placements: 7; transforms unchanged.
- Triangles: 100 → 212, below the 1,500 prop target.
- Baseline/final bounds: [[-54.56, -54.34, 0.0], [54.62, 56.47, 399.99]] / [[-54.56, -54.34, 0.0], [54.62, 56.47, 399.99]].
- Static rig: original single root name/order/parent, one action, one key and lock metadata pass converter equivalence.
- Every authored triangle matches material, bone, position, UV and winding. Maximum position error 9.99999999e-07; tolerance .0003, UV tolerance .000001. No zero-area geometry or UV triangles.
- Authored-to-raw engine normal direction maximum error 7.11247581e-07, tolerance .001.
- Exact contact/anchor maximum error 0; explicit protected set in validation/contact-contract.json.
- Frozen texture containers match current game and exported bytes. Complete hashes in validation/frozen-textures.json and final-proof.json.
- Packed source.blend includes REF_ORIGINAL with original paintings and REF_BASELINE with merged paintings. Official exporter output is reimported for the main/reverse/light/reduced and wireframe reviews. Actual placement groups include tilted instances and frozen current neighbors; PoseBox01 is an engine-hidden operation marker and omitted explicitly.
- Final export SHA256: `bd36218ec97576af5fbbe1e7aa425130dda42663e7d66c82f20a8e695f1507d1`.

The user reported the prior merged Lorencia baseline working in game. No new client verification, collision test or runtime performance benchmark is claimed here.
