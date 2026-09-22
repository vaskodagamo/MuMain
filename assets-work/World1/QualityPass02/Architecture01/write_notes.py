"""Write per-asset and batch handoffs from retained validation evidence."""
import hashlib
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parent
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')
BASELINE='7b808473'


def accepted():
    path=ROOT/'independent-review.json'
    if not path.exists(): return False
    report=json.loads(path.read_text())
    return all('ACCEPT' in record['verdict'] and not record.get('pending') and record['export_sha256']==hashlib.sha256((ROOT/record['asset']/'exports'/f"{record['asset']}.bmd").read_bytes()).hexdigest() for record in report['assets'])


def notes(name):
    folder=ROOT/name
    summary=json.loads((folder/'validation/summary.json').read_text())
    source=json.loads((folder/'validation/source-contract.json').read_text())
    before=(folder/'baseline/info.txt').read_text()
    after=(folder/'validation/info-after.txt').read_text()
    old=int(re.search(r'triangles: (\d+)',before)[1])
    new=int(re.search(r'triangles: (\d+)',after)[1])
    count=len(json.loads((folder/'placements.json').read_text()))
    rows=[f'# {name} — QualityPass02', '',f'{count} World1 placements; {old} → {new} triangles. Baseline `{BASELINE}`. Offline validation PASS; independent review required before integration. Client check pending.', '',
          '## Improvement', '', 'Chamfered timber uprights, grain-aligned corbels, stepped foot collars and a lower rail articulate the existing stone bay.' if name in NAMES[:2] else 'Actual shingle-course laps follow the existing painted rows. Relief is recessed within the original roof envelope and vanishes along the modular outer perimeter.', '',
          '## Contract', '',f'Current and candidate bind bounds: `{summary["bounds_before"]}` → `{summary["bounds_after"]}`. Original bounds are retained in `original/info.txt`. Mesh/material order, named bone and parent, one action with one frame and lock=0, filename, orientation and placements are preserved. Every baseline corner survives within 0.002 units. Roof HeroTile 4 object fade remains driven by the original model ID in `ZzzObject.cpp:3710`.', '',
          'Every triangle corner matches the packed authored source to actual re-extracted BMD by material, bone, position and UV. Raw BMD normals all bind to the intended single root bone. One UV layer, packed diffuse images and original/reference exclusions pass. Full converter comparison is intentionally DIFFERENT; skeleton/actions compare EQUIVALENT.', '',
          '## Frozen textures', '']
    for path,sha in json.loads((folder/'validation/frozen-textures.json').read_text()).items():
        rows.append(f'- `{path}` — SHA-256 `{sha}`')
    rows+=['',f'Packed image dimensions: `{[(i["name"],i["size"]) for i in source["packed_images"]]}`.', '',
           'Original BMD is byte-identical to the retained first-pass original; merged baseline is byte-identical to current main. Original preview uses current unchanged diffuse paintings to isolate geometry. Historical original paintings remain under `assets-work/World1/Architecture02/`. No artwork was generated or repainted in this batch.', '',
           f'Export SHA-256: `{summary["sha256"]}`.', '',
           '## Evidence', '', 'See `review/comparison.jpg`, `comparison-reverse.jpg`, `comparison-small.png`, `wireframe.png`, original references, and batch actual-placement assemblies. These are offline diffuse-only Blender renders, never client screenshots. Original source.blend, merged baseline source.blend, packed editable candidate source.blend and all converter outputs are retained.', '',
           'The user reported the merged Lorencia baseline working in game. This worker has not launched the client or installed shared runtime files. New candidate client acceptance is pending.']
    content='\n'.join(rows)+'\n'
    if accepted(): content=content.replace('independent review required before integration', 'independent artistic and technical review ACCEPTED offline')
    (folder/'notes.md').write_text(content)
    return f'| {name} | {count} | {old} → {new} |'


if __name__=='__main__':
    table=[notes(name) for name in NAMES]
    text='''# Architecture01 — structured wall bays and shingle laps

Production artist A. Branch `codex/lorencia-q02-architecture`, based on merged `7b808473`.

Four assets cover 23 World1 placements. All textures remain byte-identical. Current bounds, modular extremities, openings, bone/action metadata, material ordering and roof fade IDs remain unchanged. The ashlar wall family gains visible structural framing; roof caps gain modeled course lips aligned to the existing painting.

| Asset | Placements | Current → candidate triangles |
|---|---:|---:|
'''+ '\n'.join(table)+'''

All converter, source/export triangle, UV, raw normal-binding, single-bone, frozen texture and modular anchor checks pass. New client review remains pending; the user's baseline client report does not apply to these replacements. Independent artistic review is required before integration.

No terrain, shared texture, engine, runtime, placement, excluded model, or coordination file was modified. Game installation is restricted to the four owned Object1 BMDs. Actual World1 assemblies omit unrelated buildings, terrain and collision; inspect `review/*-placements.json` for exact records.

## Reproduction

Run explicitly from this worktree with environment variables `BLENDER`, `MU_BMDCONV`, `BLENDER_USER_SCRIPTS` and `BLENDER_USER_CONFIG` pointing to the coordinator's official Blender 5.2.2 / Source Tools 3.4.3 installation. Python commands use `PYTHONDONTWRITEBYTECODE=1`.

1. `python3 assets-work/World1/QualityPass02/Architecture01/pipeline.py prepare`
2. Same entrypoint with `build`, then `export`, then `validate`, then `review`.
3. Blender `-b --python-exit-code 1 --python assets-work/World1/QualityPass02/Architecture01/audit_source.py`.
4. Python `validate_anchors.py`; Pillow-enabled Python `assemble.py`; Python `write_notes.py`.
5. `install.py` installs only four hash-guarded owned BMDs into this isolated source checkout after review.

The first candidate exposed a UV-layer join mismatch and an eave bound violation; both were fixed in authored geometry and revalidated. No converter/engine changes or weakened validation thresholds were used. Final roofs use a 5-unit perimeter restraint and 3-unit maximum recessed lap depth; wall joinery remains inside the existing envelope. Sources retain REF_ORIGINAL and REF_BASELINE. No unnecessary high-poly source is used: the final bevels and laps are directly editable geometry.
'''
    if accepted():
        text=text.replace('Independent artistic review is required before integration.', 'Independent artistic and technical review ACCEPTED all four final hashes; see `independent-review.json`.')
    (ROOT/'notes.md').write_text(text)
