"""Baseline retention and explicitly non-installable official roundtrip control."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
sys.dont_write_bytecode=True
import numpy as np
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
sys.path.insert(0,str(ROOT))
import raw_bindings as raw
sys.path.insert(0,str(REPO/'assets-work/World1'))
from inventory import decode_map
CONVERTER='/Users/lukasmac/Documents/claude-test-mumain/MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv'
ANCHORS={'Object42':[0,-30,240],'Object43':[0,0,190]}


def sha(data):return hashlib.sha256(data).hexdigest()


def rotate(angles):
    a,b,c=np.radians(angles)
    x=np.array([[1,0,0],[0,np.cos(a),-np.sin(a)],[0,np.sin(a),np.cos(a)]])
    y=np.array([[np.cos(b),0,np.sin(b)],[0,1,0],[-np.sin(b),0,np.cos(b)]])
    z=np.array([[np.cos(c),-np.sin(c),0],[np.sin(c),np.cos(c),0],[0,0,1]])
    return z@y@x


def shell_bytes(path):
    data=raw.payload(path)
    cursor=38
    for index in range(2):
        start=cursor
        nv,nn,nu,nt,_=struct.unpack_from('<5h',data,cursor)
        cursor+=10+nv*16+nn*20+nu*8+nt*64+32
        if index==1:return data[start:cursor]


def triangles(path):
    lines=path.read_text().split('triangles\n')[1].splitlines()[:-1]
    return [(lines[i],np.array([list(map(float,row.split())) for row in lines[i+1:i+4]])) for i in range(0,len(lines),4)]


def incidence(group):
    bad=[]
    collapsed=[]
    for index,(_,triangle) in enumerate(group):
        cross=np.cross(triangle[1,1:4]-triangle[0,1:4],triangle[2,1:4]-triangle[0,1:4])
        length=np.linalg.norm(cross)
        if length==0 or min(triangle[:,4:7]@(cross/length))<=0:bad.append(index)
        uv=np.stack([triangle[1,7:9]-triangle[0,7:9],triangle[2,7:9]-triangle[0,7:9]])
        if abs(np.linalg.det(uv))/2<1e-10:collapsed.append(index)
    return dict(nonpositive_corner_faces=bad,zero_uv_faces=collapsed)


def inspect_model(name,placement_data):
    folder=ROOT/name
    baseline=folder/'baseline'/f'{name}.bmd'
    control=folder/'exports'/f'{name}.bmd'
    live=REPO/'src/bin/Data/Object2'/f'{name}.bmd'
    assert live.read_bytes()==baseline.read_bytes()==(folder/'original'/live.name).read_bytes()
    assert raw.payload(baseline)[:38]==raw.payload(control)[:38]
    meshsets=[raw.meshes(path) for path in [baseline,control]]
    assert all([m['material'] for m in meshes]==['wood01.jpg','fire0a.jpg'] for meshes in meshsets)
    assert all(not m['vertex_to_normal_node_mismatch_corner_counts'] for path in [baseline,control] for m in raw.audit(path)['mesh_bindings'])
    groups=[triangles(folder/path/f'{name}.smd') for path in ['baseline/smd','validation/new']]
    (folder/'validation/baseline-incidence.json').write_text(json.dumps({stage:incidence(group) for stage,group in zip(['baseline/smd','validation/new'],groups)},indent=2)+'\n')
    points=[np.concatenate([t[:,1:4] for _,t in group]) for group in groups]
    records=json.loads((folder/'provenance.json').read_text())['placements']
    support=[]
    for record in records:
        index=record['index'];kind,*values=struct.unpack_from('<h7f',placement_data,4+index*30)
        assert kind==int(name[-2:])-1
        assert values==record['position']+record['rotation']+[record['scale']]
        rotation=rotate(record['rotation'])
        transformed=[p@rotation.T*record['scale']+record['position'] for p in points]
        bounds=[np.array([p.min(axis=0),p.max(axis=0)]) for p in transformed]
        support.append(dict(index=index,position=record['position'],rotation=record['rotation'],scale=record['scale'],retained_bounds=bounds[0].tolist(),roundtrip_max_bound_error=float(np.max(abs(bounds[0]-bounds[1]))),flame_anchor_before_random_jitter=(rotation@ANCHORS[name]+record['position']).tolist()))
    assert max(p['roundtrip_max_bound_error'] for p in support)<.0003
    actions=[]
    for stage in ['baseline/smd','validation/new']:
        text=(folder/stage/f'{name}_a00.smd').read_text()
        actions.append(text.replace('-0.000000','0.000000'))
    rows=[np.array([list(map(float,line.split())) for line in text.split('skeleton\n')[1].split('end')[0].splitlines() if line and not line.startswith('time')]) for text in actions]
    motion_error=float(np.max(abs(rows[0]-rows[1])))
    assert motion_error<.00003
    assert actions[0].split('skeleton')[0]==actions[1].split('skeleton')[0]
    manifests=[(folder/stage/f'{name}.actions.txt').read_bytes().splitlines() for stage in ['baseline/smd','validation/new']]
    assert [x for x in manifests[0] if x.startswith(b'action ')]==[x for x in manifests[1] if x.startswith(b'action ')]
    for command,args in [('info',[control]),('compare',[baseline,control]),('validate',[folder/'validation/new'/f'{name}.smd'])]:
        result=subprocess.run([CONVERTER,command,*map(str,args)],capture_output=True)
        (folder/'validation'/f'{command}.txt').write_bytes(result.stdout+result.stderr)
        assert result.returncode==0
    result=dict(status='BASELINE_RETAINED_BYTE_EXACT',triangles=[len(g) for g in groups],materials=['wood01.jpg','fire0a.jpg'],source_sha256=sha(live.read_bytes()),raw_model_name=raw.payload(live)[:32].hex(),bone_action_count=struct.unpack_from('<2h',raw.payload(live),34),raw_bindings_clean=True,retained_actions_byte_equal=True,roundtrip_motion_max_error=motion_error,retained_shell_sha256=sha(shell_bytes(live)),control_shell_sha256=sha(shell_bytes(control)),control_shell_raw_bytes_equal=shell_bytes(live)==shell_bytes(control),control_is_not_an_install_candidate=True,placements=support)
    (folder/'validation/contracts.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    objpath='src/bin/Data/World2/EncTerrain2.obj'
    encoded=subprocess.check_output(['git','show','HEAD:'+objpath],cwd=REPO)
    data=decode_map(encoded)
    assert data[:2]==bytes([0,2])
    assert len(data)==4+30*struct.unpack_from('<h',data,2)[0]
    summaries={name:inspect_model(name,data) for name in ANCHORS}
    sourcefiles=['src/source/Engine/Object/ZzzObject.cpp','src/source/Render/Effects/ZzzEffectFireLeave.cpp']
    code=[(REPO/p).read_text() for p in sourcefiles]
    assert 'CreateFire(0, o, 0.f, -30.f, 240.f);' in code[0]
    assert 'CreateFire(0, o, 0.f, 0.f, 190.f);' in code[0]
    start=code[1].index('void CreateFire(');end=code[1].index('\nvoid CheckSkull',start)
    (ROOT/'flame-source.txt').write_text(code[0][code[0].index('    case WD_1DUNGEON:'):code[0].index('    case WD_1DUNGEON:')+900]+'\n'+code[1][start:end])
    consumers={t:[] for t in ['wood01.jpg','fire0a.jpg']}
    scanned=0
    for p in sorted((REPO/'src/bin/Data/Object2').glob('*.bmd')):
        for mesh in raw.meshes(p):
            if mesh['material'] in consumers:consumers[mesh['material']].append(dict(path=str(p.relative_to(REPO)),mesh=mesh['index'],sha256=sha(p.read_bytes())))
        scanned+=1
    (ROOT/'dependencies.json').write_text(json.dumps(dict(scanned_object2_models=scanned,consumers=consumers),indent=2)+'\n')
    flame=dict(source_hashes={p:sha((REPO/p).read_bytes()) for p in sourcefiles},placement_file=dict(path=objpath,sha256=sha(encoded),revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO).decode().strip()),local_anchors=ANCHORS,transform='Euler XYZ rotation plus placement position; CreateFire does NOT apply object scale',jitter='Independent random [-8,7] per axis after rotation/translation',placements_checked=sum(len(s['placements']) for s in summaries.values()),status='All original records and 120 retained anchors preserved exactly')
    assert flame['placements_checked']==120
    (ROOT/'flame-contract.json').write_text(json.dumps(flame,indent=2)+'\n')
    print('PASS all120 placements; baseline shell byte identity; roundtrip controls logged separately')


if __name__=='__main__':main()
