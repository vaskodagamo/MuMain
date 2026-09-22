"""Record exact final source/export/evidence hashes and concise per-statue handoff."""
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parent
DESIGN={
 'StoneStatue01':'A carved arched niche frames the existing human relief. Measured figure depth and an inward capital profile support the painted identity while the original outer cap, shaft anchors and foot remain fixed.',
 'StoneStatue03':'Three broad closed feather lobes replace each angular wing slab, with coherent shoulder attachment and curved overlapping growth. Rounded arm interiors and broad robe folds retain the original pose. The complete plinth and head anchors remain unchanged.',
 'SteelStatue01':'An undercut capital, curved neck profile and raised bronze inscription panel add readable structural depth. The original square foot, gate-adjacent shaft and capital perimeter anchors remain fixed.'}
BEFORE={'StoneStatue01':100,'StoneStatue03':248,'SteelStatue01':98}
accepted='--accepted' in sys.argv
reviews='\n'.join(p.read_text() for p in ROOT.glob('independent-review*.json'))
provenance={}
for name,design in DESIGN.items():
 folder=ROOT/name;proof=json.loads((folder/'validation/final-proof.json').read_text());sha=proof['bmd_sha256']
 if accepted:assert sha in reviews and 'ACCEPT' in reviews
 a=proof['proofs']['authored_triangles'];summary=proof['proofs']['converter'];normal=proof['proofs']['raw_normals'];contact=proof['proofs']['contacts']
 placements=json.loads((folder/'placements.json').read_text())
 status='Independently accepted offline; new client verification pending.' if accepted else 'Direction approved; final independent placement review pending. No game files installed.'
 (folder/'notes.md').write_text(f'''# {name}

{status}

{design}

- World1 placements: {len(placements)}; transforms unchanged.
- Triangles: {BEFORE[name]} → {proof['triangles']}, below the 1,500 prop target.
- Baseline/final bounds: {summary['bounds_before']} / {summary['bounds_after']}.
- Static rig: original single root name/order/parent, one action, one key and lock metadata pass converter equivalence.
- Every authored triangle matches material, bone, position, UV and winding. Maximum position error {a['maximum_position_delta']:.9g}; tolerance .0003, UV tolerance .000001. No zero-area geometry or UV triangles.
- Authored-to-raw engine normal direction maximum error {normal['maximum_direction_delta']:.9g}, tolerance .001.
- Exact contact/anchor maximum error {contact['maximum_position_delta']:.9g}; explicit protected set in validation/contact-contract.json.
- Frozen texture containers match current game and exported bytes. Complete hashes in validation/frozen-textures.json and final-proof.json.
- Packed source.blend includes REF_ORIGINAL with original paintings and REF_BASELINE with merged paintings. Official exporter output is reimported for the main/reverse/light/reduced and wireframe reviews. Actual placement groups include tilted instances and frozen current neighbors; PoseBox01 is an engine-hidden operation marker and omitted explicitly.
- Final export SHA256: `{sha}`.

The user reported the prior merged Lorencia baseline working in game. No new client verification, collision test or runtime performance benchmark is claimed here.
''')
 paths=[folder/'source.blend',folder/'exports'/f'{name}.bmd',folder/'baseline'/f'{name}.bmd',folder/'original'/f'{name}.bmd']+list((folder/'review').glob('*.png'))+list((folder/'validation').glob('*.json'))
 provenance[name]={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
provenance['placement_context']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'review').iterdir() if p.suffix in ('.png','.json')}
(ROOT/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
