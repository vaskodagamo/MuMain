# Short Sword — clean retopology

Built from the owner's external `item-sources/0-1/0-1-short-sword.glb`, following the selected concept. The owner's September 24 correction overrides the frozen brief's outdated Decimate instruction: no decimation was used. Five simple closed components reconstruct the blade, curved bronze guard, leather grip, pommel and raised diamond. The source mesh was imported only for measurement and colour projection, then removed before saving. Neither its geometry nor its 4096 px textures are in the delivery.

## Delivery

- 380 triangles; one mesh/material, `sword03.jpg`; original `knife_a` bone and one-frame unlocked action.
- One 1024×1024 diffuse atlas, colour baked from the generator reference. No PBR/normal-map dependency or painted engine effects.
- Original bounds: size 4.61 × 115.14 × 32.02. Replacement: 4.07 × 114.82 × 30.79. The source silhouette is fitted to the original weapon length and attachment region.
- Only `Sword02.bmd` and `sword03.OZJ` are installed, byte-identical to `exports/`. Originals are unchanged copies from the request's base commit.

## Three required checks

1. **Attachment/orientation:** the original was imported and overlaid in Blender (`review/original-overlay.png`, cyan original). The blade extends toward negative Y. Original grip Y range is −6.005 to +12.966; replacement leather runs −6.012 to +10.498, ending in its pommel. Its cross-section surrounds (0,0,0); its axis preserves the original X/Z offset (−0.028/+0.607). `hand-fit-schematic.png` shows a schematic glove around that grip. This is an offline fit illustration, not an actual equipped client capture.
2. **Closed geometry:** zero non-manifold edges, zero degenerate triangles, five closed components with positive signed volume/outward normals. The final mesh is explicitly triangulated. Each vertex has exactly one bone assignment; one UV layer.
3. **Texture:** seam unwrap with large blade panels and separate part islands; diffuse selected-to-active bake with a 32 px EXTEND margin. All unused texels are propagated from neighbouring baked colour, with zero black background pixels. JPEG saved at quality 98. The final JPEG itself and both sword sides were visually inspected.

## Validation and review

`validation/summary.json` includes raw `bmdconv validate`, `bmdconv compare` and `mu_texture.py check` output. Engine validation and texture checks pass. Compare returns 2 (DIFFERENT), expected for a redesign: 44 original versus 380 replacement triangles. Mesh/bone/action counts match, differing bone names = 0, maximum bone motion distance = 0. Final export has no errors or warnings.

Review images include matched before/after views, front/back diffuse views, wireframe, original overlay, schematic hand fit, and a 40×120 pixel 1×3 inventory footprint. The review uses diffuse-only shading so no Blender metal shader conceals texture quality. Every check is offline; client appearance, level effects and actual character animation still require owner review.

## Reproduction

Use the repository's Blender Source Tools installation (`BLENDER_USER_RESOURCES=../astra-tools/blender-user`) and `../astra-tools/Blender.app/Contents/MacOS/Blender`. First run `tools/blender/mu_bmd_import.py` on this delivery's `original/Sword02.bmd`, writing `/tmp/short-sword-original.blend`, with `--bmdconv ../MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv`. Then run the delivery scripts in order: `build.py`, `review.py`, `finalize.py`, `inventory.py`.

Export `source.blend` with `tools/blender/mu_bmd_export.py`, preserving animations; wrap the JPEG with `tools/mu_texture.py wrap`. Remove intermediate `exports/bake_raw.png` and `.blend1` backups. `build.py` requires the reference GLB at its external sibling location; never copy it into the repository. The packed final `source.blend` contains only the reduced item and original rig, and can be exported independently of the GLB.

No push or PR: `handoff.push_allowed` is false. Never merged.

## Owner correction — diagonal blade tip

The owner marked the intended straight diagonal from the blade corner to its point in a client comparison. The first retopology recessed its intermediate tip vertices, leaving a notch in that view. Replaced that ring with a planar diagonal cap, rebaked from the external reference, and regenerated all reviews and exports. Triangle count remains 380; attachment, bounds, guard and grip are unchanged. Rechecked zero non-manifold edges, zero degenerate triangles, outward component volumes, padded atlas and engine compatibility. The corrected candidate was inspected offline; the owner's screenshot documents the previous candidate, not a client test of this correction.
