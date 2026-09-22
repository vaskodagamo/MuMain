# Grass04 geometry candidate

Swept leaf crowns and cupped interior foliage boughs join the old isolated horizontal canopy levels; curved outer sprigs retain their original root and tip anchors.

Status: independently accepted offline; client verification pending. The user reported that the merged Lorencia baseline works in game. These new exports have no client verification.

- Placements: 119; placement files unchanged.
- Triangles: 800.
- Bounds before/after: [[-216.27, -78.82, -2.9], [165.65, 88.88, 115.98]] / [[-216.27, -78.82, -2.9], [165.65, 88.88, 115.98]].
- Original root order, independent parent bindings, bind transforms, actions, mesh/material order, and contact/extents retained.
- Frozen textures: tree_01.OZT, tree_02.OZT. No texture or alpha edits.
- Official Blender import/export and actual reimport pass converter validation. Every authored triangle matches exported bone, material, UV and position. Raw normals checked in world space across root rotations; per-root extents and ground contacts checked separately.
- `source.blend` packs diffuse images and retains excluded REF_ORIGINAL and REF_BASELINE geometry.
- `review/` contains matched offline diffuse views, reverse/light-background views and reduced-scale views. These are not client screenshots.
- Export SHA-256: `c4f68a1ff5bb6a5ac7228eb5d13232b9fd1db737f5428210600860b04dd86fe8`.

Experiments01 and02 remain in the batch `experiments/` directory. Experiment01 was rejected for sparse growth, exposed cap edges, and a cross-root vertex-dedup defect caught by complete authored triangle auditing. The accepted tree direction is experiment02; grass continues separately.
