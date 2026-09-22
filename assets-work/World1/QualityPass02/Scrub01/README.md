# Lorencia scrub geometry pass

Tree09 and Tree10 replace tied, narrow-waisted polygon skirts with continuous bowed growth ribbons. Grass03 and Grass04 use swept crowns, cupped central foliage and bowed peripheral sprigs to form connected leafy masses. All three OZT dependencies are frozen byte-for-byte. Existing roots, ground contacts, root bounds, rig transforms and actions remain intact.

The batch covers 530 stored World1 placements. All four final exports are independently accepted offline; see `independent-review.json` for exact hashes and limitations. The baseline was reported working in game by the user; these candidates remain client pending. Offline renders use only diffuse/alpha materials, and do not represent a client session.

## Reproduce

Use Blender 5.2.2 with official Blender Source Tools 3.4.3 enabled and the repository bmdconv. Set `BLENDER`, `MU_BMDCONV`, `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` to the installed tools. Run commands from the repository root:

```sh
python3 assets-work/World1/QualityPass02/Scrub01/pipeline.py experiment
```

The entrypoint imports untouched originals and merged baseline, rebuilds packed sources with excluded REF_ORIGINAL and REF_BASELINE collections, exports through the official pipeline, reimports, validates every authored triangle against actual bone/material/UV/position, checks independent root extents and contacts, and renders matched views. `SCRUB_NAMES=Grass03,Grass04` limits rebuild/export/render to grass while auditing the complete batch.

Run these scripts through Blender background mode with `--python-exit-code 1 --python` after the pipeline:

- `audit_source.py`: one-to-one triangle matching, collapsed triangle/UV checks, packed images and root contracts.
- `final_evidence.py`: matching original reference and actual-export wireframes.
- `assemblies.py`: exact stored World1 mixed plant placements plus observed minimum/maximum grass scales. Terrain and unrelated objects are omitted explicitly.

Run `assemble.py` with Pillow-enabled Python for comparison sheets, then `document.py` to refresh per-asset notes and SHA-256 provenance. Original and baseline BMDs are retained separately. The source scene is editable geometry with packed images; there is no new raster artwork or image-generation prompt because no texture was authored.

## Review history

Experiment01 was rejected: sparse tall forks, exposed grass cap edges and one cross-root converter position-dedup defect. Complete authored triangle matching exposed the latter; a controlled difference in interior bend amplitude prevents coincident local vertices without changing root or converter contracts. Experiment02 gives the current accepted-direction trees; its grass crown-only treatment remained layered. Experiment03 adds continuous cupped inner foliage to the grass. Rejected source/export/images remain under `experiments/`.

Alpha overdraw and final client appearance remain a practical client check for the new overlapping boughs. No engine, threshold, texture, placement, terrain, collision or runtime changes are part of this batch.

The final evidence run completed successfully. Blender SourceTools emitted stale scene-property callback warnings during factory resets in assembly rendering; the actual loaded models, saved transforms, bounds, rendered images and final exit were verified. These warnings do not alter exports or validation.
