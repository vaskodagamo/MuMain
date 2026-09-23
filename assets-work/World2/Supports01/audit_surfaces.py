"""Audit face winding, UV areas and the continuous unchanged collar boundaries."""
from pathlib import Path
import json
import sys
from mathutils import Vector

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent


def triangles(path):
    lines=path.read_text().split('triangles\n')[1].splitlines()[:-1]
    return [(lines[i],[list(map(float,row.split())) for row in lines[i+1:i+4]]) for i in range(0,len(lines),4)]


def quality(rows):
    output=[]
    for index,(_,corners) in enumerate(rows):
        a,b,c=[Vector(v[1:4]) for v in corners]
        n=(b-a).cross(c-a)
        uv=[Vector(v[7:9]) for v in corners]
        u,v=uv[1]-uv[0],uv[2]-uv[0]
        area=abs(u.x*v.y-u.y*v.x)/2
        output.append(dict(index=index,area=n.length/2,uv_area=area,
                           dot=min(n.normalized().dot(Vector(v[4:7]).normalized()) for v in corners)))
    return output


def near(a,b):
    return max(abs(x-y) for x,y in zip(a,b))<.0003


def boundary(rows,level):
    edges=[]
    for _,corners in rows:
        for a,b in zip(corners,corners[1:]+corners[:1]):
            if abs(a[3]-level)<.001 and abs(b[3]-level)<.001:
                edge=(a[1:4],b[1:4])
                if not any((near(a[1:4],x) and near(b[1:4],y)) or (near(a[1:4],y) and near(b[1:4],x)) for x,y in edges):
                    edges.append(edge)
    return edges


def main():
    results={}
    for name in ('Object06','Object13','Object15'):
        folder=ROOT/name
        old=triangles(folder/'validation/baseline'/(name+'.smd'))
        new=triangles(folder/'validation/new'/(name+'.smd'))
        a,b=quality(old),quality(new)
        original_bad=[row for row in a if row['dot']<=0 or row['area']<=1e-8 or row['uv_area']<=1e-10]
        new_bad=[row for row in b if row['dot']<=0 or row['area']<=1e-8 or row['uv_area']<=1e-10]
        for bad in new_bad:
            candidates=[item for item in original_bad if old[item['index']][0]==new[bad['index']][0]]
            target=new[bad['index']][1]
            assert any(any(all(near(old[item['index']][1][j],target[(j+s)%3]) for j in range(3)) for s in range(3)) for item in candidates),bad
        report=dict(original_exceptional_faces=original_bad,matched_unchanged_exceptions=new_bad,
                    minimum_new_area=min(row['area'] for row in b),minimum_new_uv_area=min(row['uv_area'] for row in b))
        if name=='Object15':
            low=min(v[3] for _,tri in old for v in tri);high=max(v[3] for _,tri in old for v in tri)
            counts=[]
            for level in (low,high):
                before,after=boundary(old,level),boundary(new,level)
                for x,y in before:
                    assert any((near(x,a) and near(y,b)) or (near(x,b) and near(y,a)) for a,b in after)
                counts.append(len(before))
            report['continuous_original_upper_lower_edges_retained']=counts
        results[name]=report
    a=triangles(ROOT/'Object06/validation/new/Object06.smd')
    b=[row for row in triangles(ROOT/'Object13/validation/new/Object13.smd') if row[0]=='deep_wall04.jpg']
    assert a==b,'Shared support profile or UV/normals diverged'
    results['shared_06_13_profile']='EXACT_ALL_CORNERS_UV_NORMALS'
    (ROOT/'surface-contract.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Surface, UV, winding, collar boundaries and shared profile PASS')


if __name__=='__main__':
    main()
