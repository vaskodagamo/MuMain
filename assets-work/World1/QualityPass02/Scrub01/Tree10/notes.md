# Tree10 geometry candidate

Twelve individually bowed growth ribbons per rooted tuft remove the narrow tied waist and solid polygon skirt. Broad overlapping lower growth preserves tuft mass; upper tips separate organically.

Status: independently accepted offline; client verification pending. The user reported that the merged Lorencia baseline works in game. These new exports have no client verification.

- Placements: 250; placement files unchanged.
- Triangles: 960.
- Bounds before/after: [[-199.56, -175.26, 0.44], [134.86, 178.79, 159.08]] / [[-199.56, -175.26, 0.44], [134.86, 178.79, 159.08]].
- Original root order, independent parent bindings, bind transforms, actions, mesh/material order, and contact/extents retained.
- Frozen textures: tree_07.OZT. No texture or alpha edits.
- Official Blender import/export and actual reimport pass converter validation. Every authored triangle matches exported bone, material, UV and position. Raw normals checked in world space across root rotations; per-root extents and ground contacts checked separately.
- `source.blend` packs diffuse images and retains excluded REF_ORIGINAL and REF_BASELINE geometry.
- `review/` contains matched offline diffuse views, reverse/light-background views and reduced-scale views. These are not client screenshots.
- Export SHA-256: `471f2226cf526d3f1f4b81964ab9c97cb355eb37aa2822ed225f8761ed6b8679`.

Experiments01 and02 remain in the batch `experiments/` directory. Experiment01 was rejected for sparse growth, exposed cap edges, and a cross-root vertex-dedup defect caught by complete authored triangle auditing. The accepted tree direction is experiment02; grass continues separately.
