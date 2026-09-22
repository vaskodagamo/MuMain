"""Prove exact frozen block triangles and the measured piecewise-planar perimeter band."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from audit_exports import triangles

POSITION_TOLERANCE=.0003
UV_TOLERANCE=.000001
AREA_TOLERANCE=.000001


def area(points):
    return abs(sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(points,points[1:]+points[:1])))/2


def clip(points,axis,limit,keep_greater):
    out=[]
    for a,b in zip(points,points[1:]+points[:1]):
        ia=(a[axis]>=limit) if keep_greater else (a[axis]<=limit)
        ib=(b[axis]>=limit) if keep_greater else (b[axis]<=limit)
        if ia:out.append(a)
        if ia!=ib:
            t=(limit-a[axis])/(b[axis]-a[axis]);out.append([x+t*(y-x) for x,y in zip(a,b)])
    return out


def inside_area(points,rect):
    for axis,limit,greater in ((0,rect[0],True),(0,rect[1],False),(1,rect[2],True),(1,rect[3],False)):
        if not points:return 0
        points=clip(points,axis,limit,greater)
    return area(points)


def weights(uv,rows):
    a,b,c=[r[7:9] for r in rows];den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
    x=((b[1]-c[1])*(uv[0]-c[0])+(c[0]-b[0])*(uv[1]-c[1]))/den
    y=((c[1]-a[1])*(uv[0]-c[0])+(a[0]-c[0])*(uv[1]-c[1]))/den
    return x,y,1-x-y


def same(a,b):
    if a[0]!=b[0]:return False
    return any(all(x[0]==y[0] and math.dist(x[1:4],y[1:4])<POSITION_TOLERANCE and math.dist(x[7:9],y[7:9])<UV_TOLERANCE for x,y in zip(a[1],b[1][shift:]+b[1][:shift])) for shift in range(3))


def outline(faces):
    edges={}
    for material,rows in faces:
        points=[tuple(r[7:9]) for r in rows]
        for a,b in zip(points,points[1:]+points[:1]):
            key=tuple(sorted((a,b)));edges[key]=edges.get(key,0)+1
    assert all(count in (1,2) for count in edges.values())
    return {edge for edge,count in edges.items() if count==1}


def uv_triangle_distance(uv,rows):
    """Distance in UV units, not dimensionless barycentric coordinates."""
    if min(weights(uv,rows))>=0:return 0.0
    points=[r[7:9] for r in rows]
    distances=[]
    for a,b in zip(points,points[1:]+points[:1]):
        delta=[b[k]-a[k] for k in range(2)]
        t=max(0,min(1,sum((uv[k]-a[k])*delta[k] for k in range(2))/sum(v*v for v in delta)))
        distances.append(math.dist(uv,[a[k]+t*delta[k] for k in range(2)]))
    return min(distances)


def band_match(face,old_faces):
    matches=[]
    for previous in old_faces:
        ws=[weights(row[7:9],previous[1]) for row in face[1]]
        if max(uv_triangle_distance(row[7:9],previous[1]) for row in face[1])>UV_TOLERANCE:continue
        errors=[]
        for row,w in zip(face[1],ws):
            point=[sum(weight*r[k] for weight,r in zip(w,previous[1])) for k in range(1,4)]
            errors.append(math.dist(row[1:4],point))
            assert row[0]==previous[1][0][0]
        matches.append((max(errors),max(uv_triangle_distance(row[7:9],previous[1]) for row in face[1])))
    assert matches,('Border face crossed an original surface edge',face)
    result=min(matches);assert result[0]<POSITION_TOLERANCE,result
    return result


def check():
    folder=ROOT/'HouseEtc01';old=triangles(folder/'validation/baseline/HouseEtc01.smd');new=triangles(folder/'validation/new/HouseEtc01.smd')
    blocks_old=[f for f in old if f[0]=='c_wall04.jpg'];blocks_new=[f for f in new if f[0]=='c_wall04.jpg']
    assert len(blocks_old)==len(blocks_new)==24
    assert all(sum(same(a,b) for b in blocks_new)==1 for a in blocks_old)
    old_faces=[f for f in old if f[0]=='c_wall06.jpg'];new_faces=[f for f in new if f[0]=='c_wall06.jpg']
    assert outline(old_faces)==outline(new_faces),'Original perimeter edges changed'
    design=json.loads((folder/'validation/source.json').read_text())['design'];sx,sy=design['scale'];band=design['border_units']
    rect=(.0005+band/sx,.9995-band/sx,.0005+band/sy,.9995-band/sy)
    expected=actual=0;maximum=0;maximum_uv=0;count=0
    for material,rows in old_faces:
        points=[r[7:9] for r in rows];expected+=area(points)-inside_area(points,rect)
    for face in new_faces:
        points=[r[7:9] for r in face[1]];band_area=area(points)-inside_area(points,rect);actual+=band_area
        if band_area>1e-8:
            position_error,uv_error=band_match(face,old_faces)
            maximum=max(maximum,position_error);maximum_uv=max(maximum_uv,uv_error);count+=1
    assert abs(expected-actual)<AREA_TOLERANCE,(expected,actual)
    (folder/'validation/protected-border.json').write_text(json.dumps(dict(status='PASS',border_units=band,
        frozen_block_triangles=24,original_perimeter_edges='EXACT',border_triangles_checked=count,
        baseline_band_uv_area=expected,candidate_band_uv_area=actual,coverage_area_error=abs(expected-actual),
        maximum_baseline_plane_position_error=maximum,maximum_uv_edge_distance=maximum_uv,position_tolerance=POSITION_TOLERANCE,uv_tolerance=UV_TOLERANCE,
        criterion='Frozen block triangles match one-to-one in material/vertex bone/positions/UV/cyclic winding. Every perimeter-band triangle lies on one unchanged original front surface with the same UV registration; total band area and original four perimeter edges are preserved.'),indent=2))
if __name__=='__main__':check()
