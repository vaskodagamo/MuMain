"""Track inherited signed-normal incidents by exact protected face, not aggregate count."""
import json
import math
from pathlib import Path
ROOT=Path(__file__).resolve().parent;folder=ROOT/'Waterspout01'

def triangles(path):
 lines=path.read_text().split('triangles\n')[1].splitlines()
 return [(lines[i],[list(map(float,row.split())) for row in lines[i+1:i+4]]) for i in range(0,len(lines)-1,4)]

def dot(a,b):return sum(x*y for x,y in zip(a,b))
def unit(v):
 length=math.sqrt(dot(v,v));return [x/length for x in v]
def signs(rows):
 a,b,c=[r[1:4] for r in rows];u=[y-x for x,y in zip(a,b)];v=[y-x for x,y in zip(a,c)]
 n=unit((u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]))
 values=[dot(n,unit(r[4:7])) for r in rows]
 return dict(corners=values,mean=sum(values)/3)

old=triangles(folder/'baseline/smd/Waterspout01.smd');new=triangles(folder/'validation/new/Waterspout01.smd')
protected=json.loads((folder/'validation/authored.json').read_text())['protected_original_triangle_indices']
used=set();retained=[]
for index in protected:
 material,rows=old[index];matches=[]
 for j,(m,r) in enumerate(new):
  if j in used or m!=material:continue
  for shift in range(3):
   q=r[shift:]+r[:shift]
   if any(a[0]!=b[0] for a,b in zip(rows,q)):continue
   if max(abs(x-y) for a,b in zip(rows,q) for x,y in zip(a[7:9],b[7:9]))>1e-6:continue
   error=max(abs(x-y) for a,b in zip(rows,q) for x,y in zip(a[1:4],b[1:4]))
   if error<.0003:matches.append((error,j,q))
 assert matches,('missing protected face',index)
 error,j,q=min(matches,key=lambda m:m[:2]);used.add(j)
 normal_error=max(math.dist(unit(a[4:7]),unit(b[4:7])) for a,b in zip(rows,q));assert normal_error<.001,(index,normal_error)
 before,after=signs(rows),signs(q)
 if min(before['corners'])<-.00001:retained.append(dict(baseline_face=index,final_face=j,baseline=before,final=after,maximum_normal_delta=normal_error))
newly_authored=[dict(face=i,**signs(r)) for i,(m,r) in enumerate(new) if i not in used]
invalid=[r for r in newly_authored if min(r['corners'])<-.00001]
baseline=[dict(face=i,**signs(r)) for i,(m,r) in enumerate(old)]
report=dict(status='PASS' if not invalid else 'FAIL',baseline_mean_opposing=[r for r in baseline if r['mean']<-.00001],baseline_any_corner_opposing=[r for r in baseline if min(r['corners'])<-.00001],protected_faces=len(used),retained_matched_face_exceptions=retained,newly_authored_faces=len(newly_authored),new_opposing_corner_incidents=invalid,policy='Untouched old face normals retained and matched individually; remodeled/new faces require positive per-corner shading. Mean-normal12 and any-corner45 are distinct baseline measures.')
(folder/'validation/shading-contract.json').write_text(json.dumps(report,indent=2)+'\n');assert not invalid,invalid
print('Matched legacy shading and new per-corner normal proof PASS')
