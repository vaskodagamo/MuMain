"""Validate multi-root scrub geometry, immutable motion and raw normal world directions."""
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
NAMES=tuple(os.environ.get('HOUSE_NAMES','House01,House03').split(','))
CONVERTER=os.environ['MU_BMDCONV']


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


validation=module('static_validator',REPO/'assets-work/World1/StaticBatch01/validate_export.py')
validation.EXPECTED_KEYS={name:40 if name=='House04' else 1 for name in NAMES}
raw=module('raw_bindings',REPO/'assets-work/World1/Cannons01/raw_bindings.py')


def rotate(vector,angles):
    x,y,z=vector
    a,b,c=angles
    y,z=y*math.cos(a)-z*math.sin(a),y*math.sin(a)+z*math.cos(a)
    x,z=x*math.cos(b)+z*math.sin(b),-x*math.sin(b)+z*math.cos(b)
    x,y=x*math.cos(c)-y*math.sin(c),x*math.sin(c)+y*math.cos(c)
    return (x,y,z)


def audit_normals(folder):
    name=folder.name
    reports=[]
    if name=='House04':
        for mesh in raw.meshes(folder/'exports'/f'{name}.bmd'):
            for vertices,normals in mesh['triangles']:
                for vertex,normal in zip(vertices,normals):
                    assert mesh['vertices'][vertex][0]==mesh['normals'][normal][0],('raw bone mismatch',mesh['index'],vertex,normal)
        return
    for filename in (f'{name}.smd',f'{name}_a00.smd'):
        nodes,frames,rows=validation.pose_rows(folder/'validation/new'/filename)
        assert all(line.rstrip().endswith('-1') for line in nodes.splitlines())
        angles={int(row[0]):row[4:7] for row in rows}
        mismatch=0; maximum=0
        for mesh in raw.meshes(folder/'exports'/f'{name}.bmd'):
            for vertices,normals in mesh['triangles']:
                for vertex,normal in zip(vertices,normals):
                    vbone=mesh['vertices'][vertex][0]
                    record=mesh['normals'][normal]; nbone=record[0]
                    if vbone==nbone: continue
                    mismatch+=1
                    intended=rotate(record[1:4],angles[vbone])
                    actual=rotate(record[1:4],angles[nbone])
                    delta=sum((a-b)**2 for a,b in zip(intended,actual))**.5
                    maximum=max(maximum,delta)
                    assert delta<.0001,(name,mesh['index'],vbone,nbone,delta)
        reports.append(dict(pose_file=filename,shared_normal_corners=mismatch,maximum_world_direction_delta=maximum))
    (folder/'validation/raw-normal-bindings.json').write_text(json.dumps(dict(status='PASS',poses=reports,threshold=.0001,method='Raw BMD vertex/normal ownership plus world-rotated direction against intended root, bind and actual action'),indent=2)+'\n')


def check_model(name):
    folder=ROOT/name
    old=folder/'validation/original'; old.mkdir(exist_ok=True)
    for path in (folder/'baseline/smd').iterdir(): shutil.copy2(path,old/path.name)
    new,meta,messages=validation.extract(folder,'new')
    validation.check_local_motion(old,new,name,folder/'validation')
    before=(folder/'baseline/info.txt').read_text()
    after=validation.run('info',folder/'exports'/f'{name}.bmd').stdout
    assert re.findall(r'texture=(.*)',before)==re.findall(r'texture=(.*)',after)
    bounds=[validation.engine_bounds(info) for info in (before,after)]
    assert max(abs(a-b) for aa,bb in zip(*bounds) for a,b in zip(aa,bb))<.021
    old_meta=validation.manifest_meta((old/f'{name}.actions.txt').read_text())
    assert meta==old_meta
    header=(old/f'{name}.smd').read_text().split('triangles\n')[0]
    (old/'skeleton.smd').write_text(header+'triangles\nend\n')
    validation.run('smd2bmd',old/'skeleton.smd',old/'skeleton.bmd','--manifest',old/f'{name}.actions.txt')
    skeleton=validation.run('compare',old/'skeleton.bmd',new/'skeleton.bmd').stdout
    (folder/'validation/skeleton-compare.txt').write_text(skeleton)
    comparison=validation.run('compare',folder/'baseline'/f'{name}.bmd',folder/'exports'/f'{name}.bmd',check=False).stdout
    (folder/'validation/compare.txt').write_text(comparison)
    (folder/'validation/info-after.txt').write_text(after)
    (folder/'validation/smd-validation.txt').write_text('\n'.join(messages))
    assert 'DIFFERENT' in comparison and 'EQUIVALENT' in skeleton
    return bounds


def check_textures(folder):
    hashes=json.loads((folder/'validation/frozen-textures.json').read_text())
    for path,value in hashes.items(): assert hashlib.sha256((REPO/path).read_bytes()).hexdigest()==value
    textures=list((folder/'exports').glob('*.OZ*'))
    result=subprocess.run([sys.executable,str(REPO/'tools/mu_texture.py'),'check',*map(str,textures)],capture_output=True,text=True,check=True)
    (folder/'validation/texture-check.txt').write_text(result.stdout)


for name in NAMES:
    folder=ROOT/name
    bounds=check_model(name)
    check_textures(folder)
    audit_normals(folder)
    summary=dict(status='PASS',bounds_before=bounds[0],bounds_after=bounds[1],skeleton_actions='EQUIVALENT',full_comparison='DIFFERENT',sha256=hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest(),textures_frozen=True,client_verified=False)
    (folder/'validation/summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(name,'converter/motion/texture/raw-normal audit PASS')
