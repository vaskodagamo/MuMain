# Grass03 geometry candidate

Swept leaf crowns and cupped interior foliage boughs join the old isolated horizontal canopy levels; curved outer sprigs retain their original root and tip anchors.

Status: independently accepted offline; client verification pending. The user reported that the merged Lorencia baseline works in game. These new exports have no client verification.

- Placements: 119; placement files unchanged.
- Triangles: 800.
- Bounds before/after: [[-178.46, -159.31, -3.08], [136.39, 122.73, 131.26]] / [[-178.46, -159.31, -3.08], [136.39, 122.73, 131.26]].
- Original root order, independent parent bindings, bind transforms, actions, mesh/material order, and contact/extents retained.
- Frozen textures: tree_01.OZT, tree_02.OZT. No texture or alpha edits.
- Official Blender import/export and actual reimport pass converter validation. Every authored triangle matches exported bone, material, UV and position. Raw normals checked in world space across root rotations; per-root extents and ground contacts checked separately.
- `source.blend` packs diffuse images and retains excluded REF_ORIGINAL and REF_BASELINE geometry.
- `review/` contains matched offline diffuse views, reverse/light-background views and reduced-scale views. These are not client screenshots.
- Export SHA-256: `ba0e8c4d85ecaf6fa17288d53dd4f74d1f7d1ef77062ae2189fac31b554560ec`.

Experiments01 and02 remain in the batch `experiments/` directory. Experiment01 was rejected for sparse growth, exposed cap edges, and a cross-root vertex-dedup defect caught by complete authored triangle auditing. The accepted tree direction is experiment02; grass continues separately.
