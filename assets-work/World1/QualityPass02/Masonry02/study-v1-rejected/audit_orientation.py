"""Ensure the front surface is an oriented manifold UV disk without folds or hidden overlaps."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from audit_exports import triangles


def check():
    folder=ROOT/'HouseEtc01';reports={}
    for stage in ('baseline','new'):
        faces=[f for f in triangles(folder/'validation'/stage/'HouseEtc01.smd') if f[0]=='c_wall06.jpg']
        edges={}
        for material,rows in faces:
            points=[tuple(r[7:9]) for r in rows];a,b,c=points
            assert (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])>0
            for a,b in zip(points,points[1:]+points[:1]):
                edge=tuple(sorted((a,b)));edges.setdefault(edge,[]).append(1 if a<b else -1)
        assert all(len(v) in (1,2) for v in edges.values())
        assert all(sum(v)==0 for v in edges.values() if len(v)==2)
        boundary={str(k):v[0] for k,v in edges.items() if len(v)==1}
        assert len(boundary)==4
        reports[stage]=dict(triangles=len(faces),oriented_boundary=boundary)
    assert reports['baseline']['oriented_boundary']==reports['new']['oriented_boundary']
    (folder/'validation/oriented-surface.json').write_text(json.dumps(dict(status='PASS',stages=reports,
        criterion='Every front triangle has positive oriented UV area, all internal edges have exactly two opposite uses, and the same four oriented perimeter edges form the only boundary. Combined with band plane/coverage proof, this excludes missing or overlapping band patches.'),indent=2))
if __name__=='__main__':check()
