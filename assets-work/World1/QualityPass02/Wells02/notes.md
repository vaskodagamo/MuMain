# Wells and pottery — focused second pass

Status: independently accepted at the exact hashes in `candidate-manifest.json`; the four owned game BMDs are installed in this isolated worker checkout. Integration remains the coordinator’s responsibility. All new views are offline renders, not client evidence.

## Scope and visual intent

- Well03 (two placements) and Well04 (two placements): replace the angular painted mouth with a continuous beveled lip, real inner wall and recessed throat. Preserve the existing lower-body triangles, handles, ground contacts, root bindings and footprint. This is a focused mouth/neck refinement, not a complete pottery silhouette remake.
- Well02 (one placement): dress the stone coping and separate the roof into thick boards aligned to its existing painted seams. Preserve the original roof underside contact planes and all existing posts, bucket and rope geometry.
- Well01 (unplaced composite): carry the matched well and shared new pottery mouth parts into the existing composite. Retain all tub.jpg and horse_drawn_01.jpg triangles exactly.

Textures remain byte-identical: jar_01.OZJ, well.OZJ, tub.OZJ and horse_drawn_01.OZJ. The packed sources use their extracted JPEG payloads. No painted artwork was generated.

## References and iterations

Each asset retains untouched `original/` evidence and the merged `baseline/` BMD/source. `baseline-sha256.json` identifies the current starting files. Production `source.blend` includes hidden REF_ORIGINAL references and packed textures. `exports/` contains actual BMD candidates and frozen dependencies; `validation/reimported.blend` is imported from those exports using the official importer.

`study-v1/` retains the first candidate: real mouths were useful, but stretched interior UVs created radial folded-looking streaks. `study-v2/` retains the cleaner interior UV study with independently fitted full bodies. The final mouth-only design preserves baseline bodies and uses one common mouth for matching Well03/Well01 parts; Well04 variants retain their own measured heights and tilt. The compact inner-clay UV region removes the streaks.

## Protected contracts

All assets retain filenames, origins, orientation, root names/order/parents, mesh texture order, one single-key action and lock metadata. Per-root bounds and exact ground contact preservation are recorded in each `validation/source.json`.

The composite audit compares every standalone well triangle, the 200 shared new lip/throat triangles in named bone frames, and the untouched cask/support materials. Lower pottery bodies remain their own historical geometry; small original per-instance differences are not falsely described as canonical equality.

The four baseline Well02 underside triangles 54–57 define the full protected support-contact planes. `roof-topology-inspection.json` records their exact geometry. They are distinguished from sloped edge fascias by their lack of shared vertices with either painted roof top plane. `roof-contact-proof.json` verifies preservation in both standalone and composite exports.

## Validation and review

`pipeline.py prepare` imports the current references. `pipeline.py build` rebuilds all four packed sources, uses the official exporter, runs converter validation/info/comparison, audits the rig, actions, texture ordering and frozen payloads, and officially reimports each export. Optional asset arguments permit a focused rebuild. The render stage first requires a full triangle-to-triangle authored/export audit of positions, named bones, material, UVs and cyclic winding, plus direct raw BMD world-normal comparisons.

Raw normal ownership aliases in the original composite and Well04 are rotation-equivalent: their actual world direction error is zero. They are not concealed by converter reimport, which uses vertex ownership when expanding normals. `audit_raw_normals.py` compares actual raw normal-node world directions against evaluated authored corner normals. `audit_legacy.py` proves every retained zero-UV or reversed corner-normal case matches the same defective baseline triangle, rather than exempting a material or mesh.

Review images show original, current and actual exported candidates using matching cameras, diffuse textures, reverse angles, wireframes and reduced previews. `assemblies.py` uses every unchanged actual placed transform; the unplaced composite is reviewed individually. Isolated pottery assembly views contain a gray 190-unit scale bar used only for review.

Independent final review is retained as `independent-final-review.json`. All four candidates passed artistic and offline technical review. Existing lower-body and handle coarseness remains visible; the roof refinement has restrained reduced-scale gain. These limits are recorded without claiming a whole-asset rebuild.

## Final candidate measurements

| Asset | Baseline → candidate triangles | Bind bounds (min → max) | Max raw normal world error |
|---|---:|---|---:|
| Well01 | 915 → 1149 | (-154.76, -125.851, -0.068) → (121.569, 93.463, 270.819) | 0.00008676 |
| Well02 | 123 → 317 | (-79.277, -93.457, 0.0) → (77.997, 93.463, 270.819) | 0.00006817 |
| Well03 | 704 → 744 | (-51.567, -48.729, -0.068) → (63.518, 53.384, 100.971) | 0.00011780 |
| Well04 | 1408 → 1488 | (-43.643, -134.583, -1.258) → (58.291, 126.725, 99.259) | 0.00017459 |

All counts stay within the 1500-triangle prop target. Each root retains its bounds and ground contacts; the recorded ground-contact error is zero. The full well and both protected roof contact planes match the composite exactly (zero local position/UV error). The shared pottery mouth maximum position error is 0.00003619 from interchange precision; UV error is exactly zero, checked separately against a strict 0.000001 budget. Frozen texture and source/export hashes are in `candidate-manifest.json`.
