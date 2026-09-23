# Dungeon Object42 / Object43 — baseline retention

**Artist recommendation: retain both originals byte-for-byte. No replacement geometry is proposed.** Independent confirmation can follow through the coordinator. This is an offline visual assessment, not client verification or a new remake.

Object42 is a compact angled wood support with an existing fire-textured shell. Its shaped head, narrow downward handle and horizontal attachment arm remain distinct in close views. Actual wall placement297 shows a small, recognizable fixture against masonry; the wood occupies very few pixels at ordinary scale. The first architectural-head cluster81/82 largely conceals it and is retained as evidence of that limitation, not used alone to justify retention. No silhouette defect warrants a conservative geometry change in the unobscured wall context.

Object43 has a tall narrow shaft, cupped square top, and four splayed feet. Those three features already give it a coherent, readable construction beside the actual coffin, pottery and wall placements. The squared timber character is consistent with the frozen painting; indiscriminate rounding would not establish a useful improvement. Neither object is described as a hearth or brazier based merely on its filename.

## Read first

- `placed-retention-small.png`: both principal actual contexts at reduced scale.
- `Object42-full-and-wood.png`, `Object43-full-and-wood.png`: identical-camera full static shell versus wood-only diagnostic.
- `context/Object42-wall/`: exact records297,280,281, normal/reverse/reduced views and packed scene.
- `context/Object43/`: exact records295,298,296,285,299,286,291.
- `context/Object42/`: first, largely occluded architectural-head context81/82.
- Each model's `review/`: original and unchanged roundtrip full/wood/reverse/reduced/wire views. The `*-control-*` comparison sheets compare original versus export precision control, not before/after artwork.

The offline shader uses frozen diffuse textures, neutral world background and consistent lighting. Mesh1 is shown literally as a static textured surface. No live flame particles, jitter, flicker, terrain light, terrain, client camera or client frame is simulated. These omissions must not be mistaken for defects introduced by the models. Contexts use exact recorded rotations/scales/positions, with only a common scene origin subtracted; no per-object adjustment or compensating placement changes.

## Frozen sources and runtime contract

Started from current origin/main `7c25cce6911a0e3f8737e3214dcd12c982c18728`, verified with `git pull --ff-only origin main` before work and again before packaging. The original7b808473 and current BMDs match exactly.

| Model | Placements | Mesh0 / mesh1 triangles | Root | BMD SHA256 |
|---|---:|---:|---|---|
| Object42 |64|32 /12|Box03|63a03874ca7f76b8152362530b7e114365b322919849609f819e04d12f90f4da|
| Object43 |56|66 /4|Box05|d21ee62c9bc56b26848c5d21dd11ba728dc73582a70421d86f63f97e42048ce0|

Both have one action, one key, lock0; mesh order is wood01.jpg then fire0a.jpg. Raw CP94932-byte names are recorded in the per-model contract reports and preserved by the metadata-only wrapper around the official importer/exporter.

Frozen full-path textures:

- Object2/wood01.OZJ,256²: `e54a1ed261bc161b055d3127c97b300faa0a93cdbac72fec84d95c26322842fa`. Consumers19/20/21/22/35/42/43/44, proven by reading every available Object2 model.
- Object2/fire0a.OZJ,32²: `33a99dc310a146e62f6b04d439a46c2c7baee37606725a22bbcca1470dd993ac`. Consumers42/43 only.

`dependencies.json` records all consumer BMD hashes. No texture or other consumer is modified.

Dungeon type41/Object42 calls CreateFire at local(0,-30,240); type42/Object43 at(0,0,190). The engine rotates that offset and adds placement position **without applying placement scale**, then adds independent random[-8,7] jitter on each axis. `flame-source.txt` retains the exact code; `flame-contract.json` binds its hashes and the actual EncTerrain2.obj Git blob. All120 records were decoded independently and matched exactly to the recorded transforms. Per-record support boxes and pre-jitter flame anchors are in each `validation/contracts.json`.

Because the original BMDs remain byte-identical, every wood/fire vertex, UV, raw normal, bone binding, material, action, root, bound and contact plane is unchanged. Complete raw mesh1 chunk hashes (including tables/indices) are retained in those reports.

## Roundtrip controls are not candidates

`exports/Object42.bmd` and `exports/Object43.bmd` are **unchanged-source official pipeline controls only**. Do not install them. Blender/import/export reserializes the mesh1 tables, so their raw shell chunks are not byte-identical to the frozen originals. Object42 also exhibits up to0.000016 root-position component change from float precision. Baseline retention avoids all those changes. Converter EQUIVALENT is not a claim of byte identity.

For these diagnostic controls:

- Official converter validation passes; original/control comparison reports EQUIVALENT, with no unmatched triangles or bone names. Counts remain44 and70.
- Full one-to-one authored/reimport triangle proofs enforce material, named bone, cyclic winding, positions and UVs. Max position error:42=0.000180907,43=0; UV error0 for both.
- Direct raw-normal-node world-direction comparison passes: max vector distance42=0.00007241,43=0.00006239. Raw binding alias count0.
- UVs and normals preserve legacy properties:42 has four nonpositive corner-normal faces21/22/29/30;43 has sixteen zero-area UV faces on legacy timber sides. The exact incidence is in `baseline-incidence.json`, unchanged in controls. These are disclosed original properties, not blanket exemptions for new geometry.
- All120 transformed support-box comparisons are recorded within0.0003 model units for the controls. Retained original support boxes and contacts remain exactly unchanged by construction and hash.
- Packed source images decode at their original dimensions and match frozen JPEG payload bytes. `source.blend` contains the unchanged current mesh and hidden `REF_ORIGINAL`; independent packed original/current imports are retained.

No art candidate was generated or installed, and no independent acceptance is claimed. All game/source/coordination files remain unchanged.

## Reproduction and verification

Run `python3 assets-work/World2/FireProps01/verify.py` from this worktree for read-only evidence/source verification.

Blender5.2.2 with enabled SourceTools3.4.3 is required. Set `BLENDER` and `MU_BMDCONV` to the official tools, plus the isolated `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` paths. Run `prepare.py`, then Blender batch scripts `source_reference.py` and `render_individual.py`. `pipeline.py export` creates controls and actual reimports; `FIRE_VIEW_STAGE=roundtrip` selects control images. `prepare_context.py` imports the exact read-only neighboring models; Blender `render_context.py` produces their scenes. Existing context proofs are preserved rather than overwritten.

Run `audit_contracts.py` using the bundled numpy Python, then Blender `audit_source.py`, `audit_raw_normals.py`, `audit_packed.py`. Pillow `sheets.py` arranges the rendered images with labels. All outputs stay in this package. Pipeline and audit helpers reuse the established Walls01/Remains48 methods; official tool code is unchanged. A final hash manifest binds the package. The coordinator alone owns integration, runtime and shared work logs.
