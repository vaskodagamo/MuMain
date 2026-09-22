"""Verify baseline corners and the modular roof perimeter in actual exported SMDs."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parent
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')
TOLERANCE=.002


def triangles(path):
    lines=path.read_text().split('triangles\n')[1].splitlines()
    return [(lines[i],[list(map(float,row.split())) for row in lines[i+1:i+4]]) for i in range(0,len(lines)-1,4)]


def check_asset(name):
    folder=ROOT/name
    before=triangles(folder/'baseline/smd'/f'{name}.smd')
    after=triangles(folder/'validation/new'/f'{name}.smd')
    vertices=[row[1:4] for _,rows in after for row in rows]
    for texture,rows in after:
        a,b,c=[row[1:4] for row in rows]
        u=[b[i]-a[i] for i in range(3)]
        v=[c[i]-a[i] for i in range(3)]
        cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        assert sum(value*value for value in cross)>1e-10,(name,texture,'degenerate triangle')
    maximum=0
    for _,rows in before:
        for row in rows:
            distance=min(max(abs(a-b) for a,b in zip(row[1:4],point)) for point in vertices)
            assert distance<TOLERANCE,(name,row[1:4],distance)
            maximum=max(maximum,distance)
    materials={texture for texture,_ in after}
    uv_spans={}
    for texture in materials:
        coordinates=[row[7:9] for material,rows in after if material==texture for row in rows]
        span=[max(uv[a] for uv in coordinates)-min(uv[a] for uv in coordinates) for a in range(2)]
        assert min(span)>.1,(name,texture,span)
        uv_spans[texture]=span
    report=dict(status='PASS',every_baseline_corner_retained=True,maximum_position_delta=maximum,tolerance=TOLERANCE,uv_spans=uv_spans,collapsed_material_uvs=False,degenerate_triangles=0)
    if name in NAMES[2:]:
        old_roof=[row[1:4] for texture,rows in before if texture=='tile_wood03.jpg' for row in rows]
        extrema=[[min(v[a] for v in old_roof),max(v[a] for v in old_roof)] for a in (0,1)]
        # A candidate perimeter vertex must lie on at least one original triangle plane.
        errors=[]
        for texture,rows in after:
            if texture!='tile_wood03.jpg': continue
            for row in rows:
                p=row[1:4]
                if not any(abs(p[a]-edge)<TOLERANCE for a in (0,1) for edge in extrema[a]): continue
                distances=[]
                for material,original in before:
                    if material!='tile_wood03.jpg': continue
                    a,b,c=[r[1:4] for r in original]
                    u=[b[i]-a[i] for i in range(3)]; v=[c[i]-a[i] for i in range(3)]
                    normal=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
                    norm=sum(n*n for n in normal)**.5
                    distances.append(abs(sum(normal[i]*(p[i]-a[i]) for i in range(3)))/norm)
                assert min(distances)<TOLERANCE,(name,p,min(distances))
                errors.append(min(distances))
        report['roof_perimeter_vertices_checked']=len(errors)
        report['maximum_perimeter_plane_error']=max(errors)
    (folder/'validation/modular-anchors.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,'modular corners and UV span PASS')


if __name__=='__main__':
    for name in NAMES: check_asset(name)
