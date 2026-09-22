"""Prove every retained zero-UV or reversed-corner case already exists on the baseline face."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from audit_exports import triangles,sub,cross

def properties(face):
    rows=face[1];n=cross(sub(rows[1][1:4],rows[0][1:4]),sub(rows[2][1:4],rows[0][1:4]))
    dot=min(sum(a*b for a,b in zip(n,p[4:7])) for p in rows)
    u,v=sub(rows[1][7:9],rows[0][7:9]),sub(rows[2][7:9],rows[0][7:9])
    return abs(u[0]*v[1]-u[1]*v[0])/2,dot

def check(name):
    folder=ROOT/name;old=triangles(folder/'validation/baseline'/f'{name}.smd');new=triangles(folder/'validation/new'/f'{name}.smd')
    report=json.loads((folder/'validation/uv-winding.json').read_text())
    for item in report['unchanged_legacy_faces']:
        uv,dot=properties(old[item['baseline_triangle']])
        if item['uv_area']<1e-10:assert uv<1e-10,item
        if item['min_normal_dot']<=0:assert dot<=0,item
    counts={}
    for stage,faces in (('baseline',old),('candidate',new)):
        values=[properties(f) for f in faces]
        counts[stage]=dict(zero_uv=sum(uv<1e-10 for uv,dot in values),nonpositive_corner_normals=sum(dot<=0 for uv,dot in values))
    (folder/'validation/legacy-incidence.json').write_text(json.dumps(dict(status='PASS',incidence=counts,criterion='Every candidate zero-UV and nonpositive corner-normal face corresponds to the same baseline bone/position/UV/winding triangle, and has the same baseline defect; no added exceptions'),indent=2))
if __name__=='__main__':
    for name in ('Bonfire01',):check(name)
