"""Audit frozen faces, full placement supports, raw bindings and resource scope."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import struct
sys.dont_write_bytecode = True
import numpy as np
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
sys.path.insert(0,str(REPO/'assets-work/World1/Architecture03'))
from raw_bindings import payload, meshes, audit
CONVERTER='/Users/lukasmac/Documents/claude-test-mumain/MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv'
FOLDER=ROOT/'Object51'


def triangles(path):
    lines=path.read_text().split('triangles\n')[1].splitlines()[:-1]
    return [np.array([list(map(float,row.split())) for row in lines[i+1:i+4]]) for i in range(0,len(lines),4)]


def signature(t):
    corners=[tuple(np.round(row[[0,1,2,3,7,8]],5)) for row in t]
    return min(tuple(corners[s:]+corners[:s]) for s in range(3))


def rotation(angles):
    a,b,c=np.radians(angles)
    x=np.array([[1,0,0],[0,np.cos(a),-np.sin(a)],[0,np.sin(a),np.cos(a)]])
    y=np.array([[np.cos(b),0,np.sin(b)],[0,1,0],[-np.sin(b),0,np.cos(b)]])
    z=np.array([[np.cos(c),-np.sin(c),0],[np.sin(c),np.cos(c),0],[0,0,1]])
    return z@y@x


def main():
    old=triangles(FOLDER/'baseline/smd/Object51.smd')
    new=triangles(FOLDER/'validation/new/Object51.smd')
    signatures={signature(t) for t in new}
    unchanged=[i for i,t in enumerate(old) if signature(t) in signatures]
    expected=[i for i in range(56) if i not in [14,15,50,51,52,53,54,55]]
    assert unchanged==expected,(unchanged,expected)
    positions=[np.concatenate([t[:,1:4] for t in group]) for group in [old,new]]
    records=json.loads((ROOT/'placements.json').read_text())
    maximum=0
    for record in records:
        transformed=[(p@rotation(record['rotation']).T)*record['scale']+record['position'] for p in positions]
        bounds=[np.array([p.min(axis=0),p.max(axis=0)]) for p in transformed]
        maximum=max(maximum,float(np.max(abs(bounds[0]-bounds[1]))))
    assert maximum<.0003,maximum
    normal_min=1
    uv_min=1
    for t in new:
        cross=np.cross(t[1,1:4]-t[0,1:4],t[2,1:4]-t[0,1:4]);cross/=np.linalg.norm(cross)
        normal_min=min(normal_min,float(np.min(t[:,4:7]@cross)))
        uv_min=min(uv_min,abs(float(np.linalg.det(np.stack([t[1,7:9]-t[0,7:9],t[2,7:9]-t[0,7:9]]))))/2)
    assert normal_min>0 and uv_min>1e-10,(normal_min,uv_min)
    paths=[FOLDER/'baseline/Object51.bmd',FOLDER/'exports/Object51.bmd']
    data=[payload(p) for p in paths]
    assert data[0][:38]==data[1][:38]
    raw=[audit(p) for p in paths]
    assert all(not m['vertex_to_normal_node_mismatch_corner_counts'] for r in raw for m in r['mesh_bindings'])
    for filename in ['Object51.smd','Object51_a00.smd']:
        texts=[(FOLDER/path/filename).read_text().split('triangles\n')[0] for path in ['baseline/smd','validation/new']]
        assert texts[0].replace('-0.000000','0.000000')==texts[1].replace('-0.000000','0.000000'),filename
    actions=[(FOLDER/path/'Object51.actions.txt').read_bytes().splitlines() for path in ['baseline/smd','validation/new']]
    assert [[line for line in rows if line.startswith(b'action ')] for rows in actions][0]==[[line for line in rows if line.startswith(b'action ')] for rows in actions][1]
    texture=REPO/'src/bin/Data/Object2/deep_wall12.OZJ'
    assert hashlib.sha256(texture.read_bytes()).hexdigest()=='9d3540a7af29454af04eb1d10c657ef65ec9f3d670e2426d6ed51d0426a3b45f'
    for command,args in [('validate',[FOLDER/'validation/new/Object51.smd']),('info',[paths[1]]),('compare',paths)]:
        result=subprocess.run([CONVERTER,command,*map(str,args)],capture_output=True)
        (FOLDER/'validation'/f'{command}.txt').write_bytes(result.stdout+result.stderr)
        if command!='compare':assert result.returncode==0
    report=dict(status='PASS',counts=[len(old),len(new)],frozen_triangles=unchanged,
                placements_checked=len(records),maximum_support_bounds_error=maximum,
                minimum_corner_normal_dot=normal_min,minimum_uv_area=uv_min,
                raw_bindings=raw,raw32_name=data[0][:32].hex(),metadata_header_equal=True,
                exact_nodes_and_all_action_rows=True,texture_frozen=True)
    (FOLDER/'validation/contracts.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)


if __name__=='__main__':main()
