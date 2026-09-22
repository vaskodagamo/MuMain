"""Inspect actual converter output corners and frozen texture provenance."""
import hashlib
import json
import math
from pathlib import Path
ROOT=Path(__file__).resolve().parent
REPOSITORY=ROOT.parents[3]
def sub(a,b):return [x-y for x,y in zip(a,b)]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
for name in ('Grass05','Grass06'):
    folder=ROOT/name
    lines=(folder/'validation/new'/f'{name}.smd').read_text().split('triangles\n')[1].splitlines()[:-1]
    min_area=min_uv=min_dot=float('inf')
    for i in range(0,len(lines),4):
        assert lines[i]=='tree_09.tga'
        rows=[list(map(float,line.split())) for line in lines[i+1:i+4]]
        assert all(len(row)==9 and row[0] in (0,1,2,3) and all(math.isfinite(x) for x in row) for row in rows)
        assert len(set(row[0] for row in rows))==1
        normal=cross(sub(rows[1][1:4],rows[0][1:4]),sub(rows[2][1:4],rows[0][1:4]))
        area=math.sqrt(sum(x*x for x in normal))/2
        a,b=sub(rows[1][7:9],rows[0][7:9]),sub(rows[2][7:9],rows[0][7:9])
        uv=abs(a[0]*b[1]-a[1]*b[0])/2
        dots=[sum(x*y for x,y in zip(normal,row[4:7]))/(2*area) for row in rows]
        min_area=min(min_area,area);min_uv=min(min_uv,uv);min_dot=min(min_dot,*dots)
    assert min_area>1e-8 and min_uv>1e-10 and min_dot>0
    textures=[folder/'baseline/tree_09.OZT',folder/'exports/tree_09.OZT',REPOSITORY/'src/bin/Data/Object1/tree_09.OZT']
    assert len(set(p.read_bytes() for p in textures))==1
    result=dict(status='PASS',triangles=len(lines)//4,minimum_area=min_area,minimum_uv_area=min_uv,
        minimum_corner_normal_dot_face=min_dot,texture_sha256=hashlib.sha256(textures[0].read_bytes()).hexdigest(),
        export_sha256=hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest())
    (folder/'validation/uv-normal-texture.json').write_text(json.dumps(result,indent=2)+'\n')
