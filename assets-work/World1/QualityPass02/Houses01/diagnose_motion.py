"""Measure official-export roundtrip pose drift before exact baseline motion restoration."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from audit_dome import matrix
folder=ROOT/'House04';old=folder/'validation/original/House04.smd';new=folder/'validation/new/House04.smd'
parents,old_bind=matrix.parse_smd(old);_,new_bind=matrix.parse_smd(new)
a=matrix.world_matrices(parents,old_bind[0]);b=matrix.world_matrices(parents,new_bind[0])
_,old_frames=matrix.parse_smd(old.with_name('House04_a00.smd'));_,new_frames=matrix.parse_smd(new.with_name('House04_a00.smd'))
old_points,new_points=matrix.mesh_points(old),matrix.mesh_points(new)
records=[]
for frame in old_frames:
    before=matrix.world_matrices(parents,old_frames[frame]);after=matrix.world_matrices(parents,new_frames[frame])
    world_delta=max(abs(x-y) for bone in parents for ra,rb in zip(before[bone],after[bone]) for x,y in zip(ra,rb))
    bounds_a=matrix.posed_bounds(old_points,a,before);bounds_b=matrix.posed_bounds(new_points,b,after)
    bound_delta=max(abs(x-y) for ra,rb in zip(bounds_a,bounds_b) for x,y in zip(ra,rb))
    records.append(dict(frame=frame,maximum_world_matrix_delta=world_delta,maximum_posed_bounds_delta=bound_delta))
report=dict(status='DIAGNOSTIC_NOT_ACCEPTANCE',maximum_world_matrix_delta=max(r['maximum_world_matrix_delta'] for r in records),maximum_posed_bounds_delta=max(r['maximum_posed_bounds_delta'] for r in records),frames=records,observation='Euler noise concentrated in bones7/8 near Y1.570451 radians; no tolerances changed')
(folder/'validation/official-motion-diagnostic.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
