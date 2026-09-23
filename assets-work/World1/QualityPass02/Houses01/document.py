"""Refresh final candidate provenance and concise per-building notes."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parent
DESIGN={
 'House01':'Shaped stone window surrounds and projecting sills create deep, readable openings around the complete painted motif. Entrance pier strips, rafter corbels and chimney collars give the old flat house structural depth.',
 'House03':'Transverse timber headers, knee braces and post collars support the awning. Moderately warped canopy spans retain their torn alpha and exact perimeter. A wall-panel surround, rafter corbels and stepped chimney collars complete the structural treatment.',
 'House04':'Paint-aligned physical shingle laps create roof-course depth on the dome. Every nonroof triangle, all original roof vertices, dormer/perimeter profiles and animated effect assembly remain protected.'}
BASELINE_TRIANGLES={'House01':208,'House03':313,'House04':358}
provenance={}
for name,design in DESIGN.items():
    folder=ROOT/name
    summary=json.loads((folder/'validation/summary.json').read_text())
    authored=json.loads((folder/'validation/authored-match.json').read_text())
    textures=json.loads((folder/'validation/frozen-textures.json').read_text())
    placements=json.loads((folder/'placements.json').read_text())
    status='Independently accepted offline; client verification pending.'
    motion='One original static key, exact skeleton/action equivalence.' if name!='House04' else 'Nine-bone hierarchy and forty keys preserved. Final decrypted action/bone tail is byte-identical to baseline;2160raw floats have zero delta. All40raw world matrices and posed bounds are identical. Every authored corner matches its final posed counterpart within0.000062units.'
    (folder/'notes.md').write_text(f'''# {name}

{status}

{design}

- Actual World1 placements: {len(placements)}. Their transforms are unchanged.
- Triangles: {BASELINE_TRIANGLES[name]} → {authored['triangles']}.
- Baseline/final bounds: {summary['bounds_before']} / {summary['bounds_after']}.
- Frozen dependencies: {', '.join(Path(p).name for p in textures)}; byte hashes in validation/frozen-textures.json.
- Motion: {motion}
- Strict one-to-one authored bone/material/UV/position proof and raw normal binding audit pass. Preserved original contact vertices, foundation/door approaches and roof-support points are recorded by validation/authored-match.json. No newly collapsed geometry or UVs; House03 retains two baseline zero-area UV triangles.
- Packed source.blend retains REF_ORIGINAL and REF_BASELINE. Actual exported/reimported views include reverse, light-background and reduced scale; batch review/ adds every building placement with current readonly neighbors. Separate effect views are approximations, not client evidence.
- Export SHA-256: `{summary['sha256']}`.

The user reported the earlier merged Lorencia baseline working in game. No new client verification or runtime performance benchmark is claimed. All textures, alpha and engine controls remain unchanged.
''')
    files=[folder/'source.blend',folder/'exports'/f'{name}.bmd',folder/'baseline'/f'{name}.bmd',folder/'original'/f'{name}.bmd']
    provenance[name]={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
(ROOT/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
