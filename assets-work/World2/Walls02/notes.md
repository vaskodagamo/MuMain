# Object51 controlled boss study — rejected; retain baseline

**Decision:** do not install this candidate. The artist, independent reviewer and coordinator agree that the broader dressed boss reads in closeup but provides negligible construction/readability gain in the actual joined run or reduced view. It also concentrates the old streaky ledge painting into a more conspicuous bright strip. The original projecting boss, deep recess, stepped crown and continuous joins already communicate this module's architectural role. No further bevel escalation is warranted.

This package is an offline study, not a completed replacement or client verification. No game BMD, texture, placement, terrain, engine or shared coordination file changed. Only `assets-work/World2/Walls02/` is committed.

## Evidence

- `joined-run-small-comparison.png`: decisive current/candidate reduced comparison.
- `joined-run-comparison.png`, `joined-run-reverse-comparison.png`: exact three-module run.
- `isolated-comparison.png`, `isolated-small-comparison.png`, `isolated-reverse-comparison.png`.
- `Object51/review/candidate-wire.png`: topology-only reverse view.
- `Object51/source.blend`: packed editable candidate and hidden REF_ORIGINAL.
- `Object51/exports/Object51.bmd`: rejected, non-installed official export.
- `independent-direction.json`: reviewer message transcribed, with review limits.

Actual adjacent records 3214/3215/3216 are at (16050,17950,169.50003), (16050,18250,169.50003), (16050,18550,169.49997); each has Euler degrees (0,0,450), scale1. Context scenes subtract only the first position as a common origin. Exact records, bounds, camera and lighting are retained; there is no terrain or client rendering. Both sides use diffuse-only Cycles with the same frozen JPEG. Comparison sheets only arrange/label the actual pixels.

## Scope and contracts

Starting revision c40d0b0d313b44b99a4fd84bcac0a81e476f3ed7, branch codex/dungeon-wall51. Original7b808473 and current Object51/texture bytes are identical; both BMD references and packed imports are retained. The baseline import uses the unchanged official importer plus an ASCII-only action-manifest comment adapter. The export adapter preserves all32 original model-name bytes; no converter or engine behavior changes.

Candidate56→66 triangles. A seven-unit single-segment bevel dresses three exposed edges of the central boss; no other masonry face receives arbitrary subdivisions. All48 other original triangle surfaces/material/UV/bindings/winding are retained, including end faces24–35, top faces0–5, and the foot. The twelve end triangles are the full joining interfaces at x=-150.056107 and149.943893 (300-unit module pitch). Ground/top boundaries and local bounds remain unchanged. The original open underside remains intentionally open.

Validation in `Object51/validation/`:

- All178 actual placements tested with scale and complete Euler rotation: maximum global support-box error0.
- Authored-to-actual export: all66 triangles matched one-to-one, exact material/bone/cyclic winding; maximum position error0, UV error5.6093e-7 (budget1e-6).
- Raw normal-node bindings clean. Actual raw corner normals compared to authored world normals after full triangle matching: maximum vector error0.00014393 (budget0.001).
- Every candidate face has positive corner-normal incidence (minimum0.9999733); minimum UV triangle area0.0003050.
- Raw32 name and mesh/bone/action header preserved: one mesh, rootBox06, one action, one key, lock0. Nodes and full action rows agree numerically; only textual negative zero changes. Action manifest entries agree exactly.
- Converter validation passes. Original/candidate comparison correctly reports DIFFERENT geometry. Candidate official import/re-export comparison reports EQUIVALENT with zero unmatched triangles and zero printed corner/bone distance.
- Packed JPEG is256×256 and byte-identical to the unwrapped frozen container. REF_ORIGINAL remains hidden and packed.

Independent review inspected the actual BMD and all image directions plus contracts/authored reports; final raw-normal/dependency reports were completed afterward and are worker checks, not separately claimed independent technical acceptance. Artistic rejection stands independently of those checks.

## Frozen dependency

Object2/deep_wall12.OZJ SHA256 `9d3540a7af29454af04eb1d10c657ef65ec9f3d670e2426d6ed51d0426a3b45f` remains untouched. `texture-consumers.json` scans all5300 available `.bmd` paths; Object51 is the only readable model referencing deep_wall12.jpg. All Object2 model files parsed successfully. Non-model BMD tables and four unsupported Object3 model versions are explicitly listed, so the scan does not falsely claim all suffix-matching files are decoded models. Other ObjectN texture resources resolve in their own folders. No artwork replacement is proposed.

Baseline Object51 SHA256 `81e200861fe90ba8c3616f28a61bdd1ae0b5025c749c8956f17a3193520ff2c0`.
Candidate SHA256 `b25043a1cbb2dd52df843aa61c77250932c72ba5c0a0e9fae9d3b5010dceb846`.

## Reproduction

Use the official Blender5.2.2 binary and enabled SourceTools3.4.3 with `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` pointing to the isolated astra-tools configuration. Set `BLENDER` to that binary and `MU_BMDCONV` to the built converter. From this worktree:

1. Blender `-b --python-exit-code 1 --python assets-work/World2/Walls02/build_source.py`.
2. Python `assets-work/World2/Walls02/pipeline.py export` (official export and actual reimport).
3. Bundled numpy Python `assets-work/World2/Walls02/audit.py`.
4. Blender batch scripts `audit_source.py`, `audit_raw_normals.py`, `audit_packed.py`, then `review.py` and `wire.py`, all under Walls02.
5. Bundled Pillow Python `assets-work/World2/Walls02/comparisons.py`.

Helper scripts reuse the reviewed Walls01 metadata adapter and Remains48 authored/raw-normal auditing methods. These are retained locally in this package. Current baseline imports and historical joined-run evidence were copied read-only from Readiness03; their provenance hashes remain intact. Scripts write only this package. Render/source files remain editable. The branch is frozen after its study-only commit pending coordinator review; no push or PR is authorized for this checkpoint.
