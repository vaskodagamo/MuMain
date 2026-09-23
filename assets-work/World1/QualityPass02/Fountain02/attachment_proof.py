"""All21 mouth-water and engine particle-anchor checks against exact baseline transforms."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from motion_proof import motion,world,raw
folder=ROOT/'Waterspout01'
old_meta,old,_=motion(folder/'baseline/Waterspout01.bmd');new_meta,new,_=motion(folder/'exports/Waterspout01.bmd')
a,b=world(old,0),world(new,0)
old_mesh=raw.meshes(folder/'baseline/Waterspout01.bmd');new_mesh=raw.meshes(folder/'exports/Waterspout01.bmd')
water=[a[v[0]]@Vector(v[1:4]) for v in old_mesh[3]['vertices']]
old_points=[(v[0],Vector(v[1:4]),a[v[0]]@Vector(v[1:4])) for v in old_mesh[1]['vertices']]
new_points=[(v[0],Vector(v[1:4]),b[v[0]]@Vector(v[1:4])) for v in new_mesh[1]['vertices']]
contacts=[(bone,p,q) for bone,p,q in old_points if q.z>250 and min((q-v).length for v in water)<14]
assert len(contacts)>=20
pairs=[]
for bone,local,p in contacts:
 match=min((q for q in new_points if q[0]==bone),key=lambda q:(q[2]-p).length)
 assert (match[2]-p).length<.0003
 pairs.append((bone,local,match[1]))
records=[]
for frame in range(21):
 a,b=world(old,frame),world(new,frame)
 particles=[]
 for bone,y in ((1,-20),(4,-80)):
  for x in (-16,16):
   for z in (-16,16):
    p=Vector((x,y,z));particles.append((a[bone]@p-b[bone]@p).length)
 mouth=max((a[bone]@p-b[bone]@q).length for bone,p,q in pairs)
 relative=max(((a[bone]@p-a[1].translation)-(b[bone]@q-b[1].translation)).length for bone,p,q in pairs)
 assert max(particles+[mouth,relative])<.0003,(frame,particles,mouth,relative)
 records.append(dict(frame=frame,particle_envelope_maximum_delta=max(particles),mouth_maximum_delta=mouth,mouth_relative_to_water_root_delta=relative))
report=dict(status='PASS',mouth_contact_vertices=len(pairs),tolerance=.0003,particle_bones=[1,4],all21_frames=records,policy='Original dragon points within14units of the water outlet retained; actual engine particle offsets sampled at extrema; all21 intended-v-final raw transforms checked')
(folder/'validation/attachments.json').write_text(json.dumps(report,indent=2)+'\n');print('Mouth/water/particle21-frame contact proof PASS')
