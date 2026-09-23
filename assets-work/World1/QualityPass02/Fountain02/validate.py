"""Validate fountain geometry, protected motion, textures and raw normal bindings."""
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
NAMES=tuple(os.environ.get('FOUNTAIN_NAMES','Waterspout01').split(','))
CONVERTER=os.environ['MU_BMDCONV']


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


validation=module('static_validator',REPO/'assets-work/World1/StaticBatch01/validate_export.py')
validation.EXPECTED_KEYS={name:21 for name in NAMES}
raw=module('raw_bindings',REPO/'assets-work/World1/Cannons01/raw_bindings.py')


def rotate(vector,angles):
    x,y,z=vector
    a,b,c=angles
    y,z=y*math.cos(a)-z*math.sin(a),y*math.sin(a)+z*math.cos(a)
    x,z=x*math.cos(b)+z*math.sin(b),-x*math.sin(b)+z*math.cos(b)
    x,y=x*math.cos(c)-y*math.sin(c),x*math.sin(c)+y*math.cos(c)
    return (x,y,z)


def audit_normals(folder):
    # Actual hierarchical world directions are proved by authored_normals.py across all21frames.
    corners=0
    for mesh in raw.meshes(folder/'exports/Waterspout01.bmd'):
        for vertices,normals in mesh['triangles']:
            for vertex,normal in zip(vertices,normals):
                assert mesh['vertices'][vertex][0]==mesh['normals'][normal][0],('raw normal ownership',mesh['index'],vertex,normal)
                corners+=1
    (folder/'validation/raw-normal-bindings.json').write_text(json.dumps(dict(status='PASS',corners=corners,ownership='Every raw vertex/normal corner uses the same bone; full21frame authored directions separately proved'),indent=2)+'\n')


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
    for path,value in hashes.items():
        assert hashlib.sha256((REPO/path).read_bytes()).hexdigest()==value
        assert hashlib.sha256((folder/'exports'/Path(path).name).read_bytes()).hexdigest()==value
    paint=folder/'textures'/os.environ.get('FOUNTAIN_PAINT','paint02')
    proof=json.loads((paint/'packaging-proof.json').read_text())
    actual=(folder/'exports/reagon_waterspout.OZJ').read_bytes()
    assert hashlib.sha256(actual).hexdigest()==proof['output_ozj_sha256']
    assert actual[24:]==(paint/'patched.jpg').read_bytes()
    assert proof['max_protected_rgb_error']==proof['max_all_unselected_rgb_error']==0
    art_python=os.environ.get('MU_ART_PYTHON','/Users/lukasmac/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3')
    subprocess.run([art_python,str(ROOT/'engine_decode_proof.py'),str(paint)],check=True)
    shutil.copy2(paint/'engine-decode-proof.json',folder/'validation/engine-decoded-texture-protection.json')
    textures=list((folder/'exports').glob('*.OZ*'))
    result=subprocess.run([sys.executable,str(REPO/'tools/mu_texture.py'),'check',*map(str,textures)],capture_output=True,text=True,check=True)
    (folder/'validation/texture-check.txt').write_text(result.stdout)


for name in NAMES:
    folder=ROOT/name
    bounds=check_model(name)
    check_textures(folder)
    audit_normals(folder)
    summary=dict(status='PASS',bounds_before=bounds[0],bounds_after=bounds[1],skeleton_actions='EQUIVALENT',full_comparison='DIFFERENT',sha256=hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest(),textures_frozen=3,edited_atlas='reagon_waterspout.OZJ; protected basin/filter-margin decoded pixels exact',client_verified=False)
    (folder/'validation/summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(name,'converter/motion/texture/raw-normal audit PASS')
