# Dungeon linked supports and open collar — production study

Artist disposition: **reject these candidate replacements and retain the baseline**. Coordinator review is pending. This package contains an actual Blender geometry attempt, official exports, reimports, strict numeric contracts and matched placed comparisons. No game model, texture, placement, shared board or runtime was changed; nothing was installed or verified in the client.

The 06/13 capital setback is visible close up, but chiefly deepens dark recesses beside the existing fins. It does not convincingly improve the joined support at reduced scale and weakens the visual sense of solid support. The collar's two-plane profile reads as a dressed edge at close range, yet largely disappears in the reduced three-instance run. Extra triangles are not justified by that payoff. The original tapered capitals, coherent painted ornament and intentionally open collar remain the recommended game assets. Further arbitrary stone filling or ornament is not warranted.

| Model | Baseline → study triangles | Actual placements | Treatment |
|---|---:|---:|---|
| Object06 | 54 → 132 | 179 | Stepped inward capital below exact original lip |
| Object13 | 56 → 134 | 50 | Identical shared profile; original two-face cap retained |
| Object15 | 9 → 27 | 217 | Shallow arris in existing collar faces; opening retained |

## Boundaries and inputs

Base and fetched `origin/main`: `7c25cce6911a0e3f8737e3214dcd12c982c18728`. Worktree `/Users/lukasmac/Documents/claude-test-mumain/MuMain-dungeon-supports`, branch `codex/dungeon-supports-pr02`. These are supports with roots at the top and shafts extending down about 300 units, not braziers.

Every study retains the model's raw CP949 name bytes, material/mesh order, Box13 root, one action and one key. The full original upper lip, 13 cap, original vertex/contact anchors, shaft surfaces and local/placed extrema are protected. The 15 upper opening and lower boundary edges are checked continuously as full edges, not merely extrema. The 06/13 generated deep_wall04 triangles match exactly including UVs and normals. Nothing fills the existing hollows.

`texture-consumers.json` discovers and hashes every Object2 consumer of the two frozen atlases, including DungeonStone01 as well as the supplied consumer lists. `data-hashes.json` records every Object2 game file. Both frozen atlases are byte-identical; the neighboring Object04 additionally uses unchanged deep_wall03. Current Object04 and all current textures were freshly officially imported for context. `context-input-audit.json` compares the previous Readiness01 dependencies to current input hashes. `placement-provenance.json` proves the encrypted map's bytes are unchanged from the readiness base; individual placement files preserve all records.

## Authoring and validation

`build.py` authors through Blender Python API. It cuts only selected original capital faces into a purposeful radial setback profile; UVs interpolate their existing original painted patches. All other faces are copied. A separate single profile break changes the open collar. Each packed `source.blend` keeps the baseline in a hidden, export-disabled `REF_ORIGINAL` collection. `original/source.blend` and `baseline/source.blend` are identical immutable imports of the current starting asset; “original” here means the original for this study, not a reconstructed historic texture version. Original/current images intentionally have identical bytes.

Official SourceTools and `mu_bmd_import.py` / `mu_bmd_export.py` handle the conversion. `official_io.py` is the documented existing raw-name byte transport adapter. It also selects the original action manifest at the supported converter boundary: direct SourceTools export changed the equivalent static rotation from +pi to -pi. Retaining the original one-key input restores exact numeric motion components. Raw action-tail bytes still differ because original unused bone-name padding is 0xCD and the exporter zero-pads it; no byte-identical raw-tail claim is made. Root names, parents, action count/key/lock and all six motion values match. Model-name raw 32-byte fields match exactly.

`audit.py` passes one-to-one authored triangle position/UV/material/bone/winding and normal matching, protected face correspondence, converter validation/info/compare, raw vertex/normal owner correspondence, raw world-normal checks, original vertex contacts, and bounds for every actual placement. Position tolerance is 0.0003 world units; UV tolerance 0.000001; normal tolerance 0.001. Maximum authored position error is 0.000001431, UV error 0.000000634, authored normal error 0.000143922. Protected exported normal values incur up to 0.000297109 quantization drift; they are numerically matched, not claimed byte-identical. The unchanged game parts/files are byte-identical.

`audit_surfaces.py` confirms positive face/UV area and corner normals, no new shading exception, retained complete collar boundary edges, and exact shared 06/13 profile. No tolerance was increased to obtain a pass. Per-model `validation/contract.json` contains counts, bounds and measured errors. Converter `compare` reports DIFFERENT geometry as expected, with unchanged bone/action identity; it is not claimed EQUIVALENT geometry.

An initial fresh Object04 context import lacked deep_wall03, visible as a white panel. That context was corrected and rerendered; its diagnostic import log is retained. The orchestrator now rejects missing-texture warnings as well as nonzero exits or logged SourceTools errors. Current evidence has the actual painted panel.

## Reproduce and review

Run from the worktree using the configured official Blender 5.2.2 / SourceTools 3.4.3 and converter paths in `pipeline.py`:

```sh
python3 assets-work/World2/Supports01/prepare.py
python3 assets-work/World2/Supports01/pipeline.py prepare
python3 assets-work/World2/Supports01/pipeline.py build
python3 assets-work/World2/Supports01/pipeline.py export
python3 assets-work/World2/Supports01/pipeline.py audit
python3 assets-work/World2/Supports01/pipeline.py audit_surfaces
python3 assets-work/World2/Supports01/pipeline.py review
python3 assets-work/World2/Supports01/run_context.py
python3 assets-work/World2/Supports01/finalize.py
```

Baseline acquisition asserts the fixed base's model and texture bytes before copying. `raw_bindings.py` is an unchanged copy of the repository's Architecture03 reader. No converter or engine source changed. Script stages write only inside Supports01.

Each model's `review/` has original/current/candidate, reverse, reduced and wire views of the actual exported/reimported geometry. `context/` contains matched current/candidate/reverse/reduced views for the actual pier support, stepped pair and open trim run. Current and candidate use identical transforms, lighting and camera bounds; the pier neighbor is the same current main asset in both. `previous-context/` retains historical Readiness01 evidence separately. `provenance.json` binds source, exports, scripts, reports and images to hashes.

All exports are **rejected study candidates**, retained for inspection and reproduction, not ready-to-install replacements. Offline diffuse review does not establish client appearance, terrain intersections or runtime performance.
