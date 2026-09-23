"""Freeze current source, game exports and final evidence hashes for review."""
from pathlib import Path
import json,hashlib,sys
ROOT=Path(__file__).resolve().parent;folder=ROOT/'Waterspout01'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
files=[folder/'source.blend',folder/'exports/Waterspout01.bmd',folder/'exports/reagon_waterspout.OZJ']
evidence=sorted(set([*folder.glob('validation/*.json'),*folder.glob('validation/*.txt'),*folder.glob('review/*.png'),*folder.glob('review/*context.json'),*ROOT.glob('review/*.png'),*ROOT.glob('review/*contacts.json'),ROOT/'context/groups.json',ROOT/'context/frozen-neighbor-hashes.json',folder/'textures/paint02/packaging-proof.json',folder/'textures/paint02/engine-decode-proof.json']))
report=dict(status='AWAITING_FINAL_REVIEW',client_verified=False,asset='Waterspout01',triangles_before=639,triangles_after=921,bones=11,actions=1,keys=21,placements=1,files={str(p.relative_to(ROOT)):sha(p) for p in files},evidence={str(p.relative_to(ROOT)):sha(p) for p in evidence},changes='Fuller dragon chest/neck, bowed membrane and rounded proximal wing chord, broad charcoal painted planes; protected basin/rock/water, original motion and3 shared textures preserved')
if '--accepted' in sys.argv:
 review=(ROOT/'independent-review.json').read_text()
 assert all(value in review for value in report['files'].values()),'Final review must record all exact source/BMD/texture hashes'
 report['status']='ACCEPTED_OFFLINE_CLIENT_PENDING';report['independent_review_sha256']=sha(ROOT/'independent-review.json')
(ROOT/'provenance.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='evidence'},indent=2));print(len(evidence),'evidence files recorded')
