"""Refresh human-readable candidate records and immutable artifact hashes."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parent
NAMES=('Tree09','Tree10','Grass03','Grass04')
placements=dict(Tree09=42,Tree10=250,Grass03=119,Grass04=119)
manifest={}
for name in NAMES:
    folder=ROOT/name
    summary=json.loads((folder/'validation/summary.json').read_text())
    audit=json.loads((folder/'validation/authored-match.json').read_text())
    change=('Twelve individually bowed growth ribbons per rooted tuft remove the narrow tied waist and solid polygon skirt. Broad overlapping lower growth preserves tuft mass; upper tips separate organically.' if name.startswith('Tree') else 'Swept leaf crowns and cupped interior foliage boughs join the old isolated horizontal canopy levels; curved outer sprigs retain their original root and tip anchors.')
    textures=json.loads((folder/'validation/frozen-textures.json').read_text())
    (folder/'notes.md').write_text(f'''# {name} geometry candidate

{change}

Status: independently accepted offline; client verification pending. The user reported that the merged Lorencia baseline works in game. These new exports have no client verification.

- Placements: {placements[name]}; placement files unchanged.
- Triangles: {audit['triangles']}.
- Bounds before/after: {summary['bounds_before']} / {summary['bounds_after']}.
- Original root order, independent parent bindings, bind transforms, actions, mesh/material order, and contact/extents retained.
- Frozen textures: {', '.join(Path(p).name for p in textures)}. No texture or alpha edits.
- Official Blender import/export and actual reimport pass converter validation. Every authored triangle matches exported bone, material, UV and position. Raw normals checked in world space across root rotations; per-root extents and ground contacts checked separately.
- `source.blend` packs diffuse images and retains excluded REF_ORIGINAL and REF_BASELINE geometry.
- `review/` contains matched offline diffuse views, reverse/light-background views and reduced-scale views. These are not client screenshots.
- Export SHA-256: `{summary['sha256']}`.

Experiments01 and02 remain in the batch `experiments/` directory. Experiment01 was rejected for sparse growth, exposed cap edges, and a cross-root vertex-dedup defect caught by complete authored triangle auditing. The accepted tree direction is experiment02; grass continues separately.
''')
    manifest[name]={str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (folder/'source.blend',folder/'exports'/f'{name}.bmd',folder/'original'/f'{name}.bmd',folder/'baseline'/f'{name}.bmd')}
(ROOT/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
