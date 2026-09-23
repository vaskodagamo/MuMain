"""Measure the first official output before deciding whether motion restoration is needed."""
import hashlib
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'Houses01'))
from raw_motion import motion,world,raw,bounds
folder=ROOT/'Waterspout01'
a,old,old_tail=motion(folder/'baseline/Waterspout01.bmd');b,new,new_tail=motion(folder/'exports/Waterspout01.bmd')
assert a==b and [(x['name'],x['parent']) for x in old]==[(x['name'],x['parent']) for x in new]
deltas=[abs(x-y) for ba,bb in zip(old,new) for ta,tb in zip(ba['tracks'],bb['tracks']) for fa,fb in zip(ta,tb) for va,vb in zip(fa,fb) for x,y in zip(va,vb)]
old_mesh,new_mesh=raw.meshes(folder/'baseline/Waterspout01.bmd'),raw.meshes(folder/'exports/Waterspout01.bmd');frames=[]
for frame in range(21):
 x,y=world(old,frame),world(new,frame)
 delta=max(abs(p-q) for bone in x for ra,rb in zip(x[bone],y[bone]) for p,q in zip(ra,rb))
 ba,bb=bounds(old_mesh,x),bounds(new_mesh,y);bound=max(abs(p-q) for ra,rb in zip(ba,bb) for p,q in zip(ra,rb))
 frames.append(dict(frame=frame,maximum_world_matrix_delta=delta,maximum_posed_bounds_delta=bound))
report=dict(status='DIAGNOSTIC_NOT_ACCEPTANCE',raw_tail_identical=old_tail==new_tail,maximum_raw_float_delta=max(deltas),raw_float_components=len(deltas),maximum_world_matrix_delta=max(r['maximum_world_matrix_delta'] for r in frames),maximum_posed_bounds_delta=max(r['maximum_posed_bounds_delta'] for r in frames),frames=frames)
(folder/'validation/official-motion-diagnostic.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
