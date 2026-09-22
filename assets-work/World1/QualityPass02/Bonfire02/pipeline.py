"""Official Blender/BMD interchange and strict bonfire contract audits."""
from pathlib import Path
import importlib.util
import json
import math
import os
import subprocess
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPOSITORY=ROOT.parents[3]
WORLD=ROOT.parents[1]
NAMES=tuple(sys.argv[2:]) if len(sys.argv)>2 else ('Bonfire01',)
BLENDER=os.environ['MU_BLENDER']
CONVERTER=os.environ['MU_BMDCONV']


def helper(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def blender(log,*args):
    result=subprocess.run([BLENDER,'-b','--python-exit-code','1',*map(str,args)],
        capture_output=True,text=True,cwd=REPOSITORY,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
    log.write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(str(log))


def import_model(folder,stage):
    for texture in (folder/'baseline').glob('*.OZJ'):
        subprocess.run([sys.executable,str(REPOSITORY/'tools/mu_texture.py'),'unwrap',str(texture),'--out',str(folder/'textures'/(texture.stem+'.jpg'))],check=True,capture_output=True)
    out=folder/'baseline/source.blend' if stage=='baseline' else folder/'validation/reimported.blend'
    blender(folder/'validation'/f'import-{stage}.txt','--python',REPOSITORY/'tools/blender/mu_bmd_import.py',
        '--','--bmd',folder/stage/(folder.name+'.bmd'),'--textures',folder/'textures',
        '--out',out,'--bmdconv',CONVERTER)


def rotation(values):
    x,y,z=values
    cx,cy,cz=math.cos(x),math.cos(y),math.cos(z)
    sx,sy,sz=math.sin(x),math.sin(y),math.sin(z)
    return (cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx,
            sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx,-sy,cy*sx,cy*cx)


def motion(shared,old,new,name):
    records=[]
    for filename in (name+'.smd',name+'_a00.smd'):
        a_nodes,a_frames,a=shared.pose_rows(old/filename)
        b_nodes,b_frames,b=shared.pose_rows(new/filename)
        assert a_nodes==b_nodes and a_frames==b_frames and len(a)==len(b)
        position=max(abs(x-y) for aa,bb in zip(a,b) for x,y in zip(aa[1:4],bb[1:4]))
        angle=max(abs(x-y) for aa,bb in zip(a,b) for x,y in zip(rotation(aa[4:]),rotation(bb[4:])))
        assert position<.0001 and angle<.00001,(position,angle)
        records.append(dict(file=filename,position=position,rotation_matrix=angle))
    return records


def validate(name,shared,raw):
    folder=ROOT/name;report=folder/'validation'
    old,old_meta,old_logs=shared.extract(folder,'original')
    new,new_meta,new_logs=shared.extract(folder,'new')
    assert old_meta==new_meta
    motion_report=motion(shared,old,new,name)
    (report/'smd-validation.txt').write_text('\n'.join(old_logs+new_logs))
    (report/'local-motion.json').write_text(json.dumps(motion_report,indent=2))
    info=shared.run('info',folder/'exports'/(name+'.bmd')).stdout
    (report/'info-after.txt').write_text(info)
    import re
    expected=re.findall(r'texture=(\S+)',shared.run('info',folder/'baseline'/(name+'.bmd')).stdout)
    assert re.findall(r'texture=(\S+)',info)==expected
    before=shared.engine_bounds(shared.run('info',folder/'baseline'/(name+'.bmd')).stdout)
    after=shared.engine_bounds(info)
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.02
    comparison=shared.run('compare',folder/'baseline'/(name+'.bmd'),folder/'exports'/(name+'.bmd'),check=False)
    assert 'DIFFERENT' in comparison.stdout
    (report/'compare.txt').write_text(comparison.stdout)
    skeleton=shared.run('compare',old/'skeleton.bmd',new/'skeleton.bmd').stdout
    assert 'EQUIVALENT' in skeleton
    (report/'skeleton-compare.txt').write_text(skeleton)
    binding=raw.audit(folder/'exports'/(name+'.bmd'))
    world_normals=helper('world_normals',ROOT/'raw_audit.py').check(folder,'new')
    assert world_normals['status']=='PASS',world_normals
    (report/'raw-bindings.json').write_text(json.dumps(binding,indent=2))
    for texture in (folder/'exports').glob('*.OZJ'):
        assert texture.read_bytes()==(REPOSITORY/'src/bin/Data/Object1'/texture.name).read_bytes()
        result=subprocess.run([sys.executable,str(REPOSITORY/'tools/mu_texture.py'),'check',str(texture)],capture_output=True,text=True,check=True)
        (report/(texture.stem+'-check.txt')).write_text(result.stdout)
    (report/'summary.json').write_text(json.dumps(dict(status='PASS',bounds_before=before,bounds_after=after,
        skeleton_actions='EQUIVALENT',mesh_order=expected,client_verified=False),indent=2))
    import_model(folder,'exports')


def main():
    if sys.argv[1]=='prepare':
        for name in ('Bonfire01',):import_model(ROOT/name,'baseline')
        return
    shared=helper('shared',WORLD/'StaticBatch01/validate_export.py')
    shared.CONVERTER=Path(CONVERTER);shared.EXPECTED_KEYS={name:1 for name in NAMES}
    raw=helper('raw',WORLD/'Architecture03/raw_bindings.py')
    if sys.argv[1]=='build':blender(ROOT/'build-log.txt','--python',ROOT/'build_source.py','--',*NAMES)
    for name in NAMES:
        folder=ROOT/name
        blender(folder/'validation/export.txt',folder/'source.blend','--python',
            REPOSITORY/'tools/blender/mu_bmd_export.py','--','--out',folder/'exports'/(name+'.bmd'),'--bmdconv',CONVERTER)
        validate(name,shared,raw)
    helper('final_corner_audit',ROOT/'audit_exports.py').main(NAMES)
    legacy=helper('legacy_cases',ROOT/'audit_legacy.py')
    for name in NAMES:legacy.check(name)
    helper('protected_shell',ROOT/'audit_shell.py').check()
    blender(ROOT/'render-log.txt','--python',ROOT/'review.py','--',*NAMES)


if __name__=='__main__':main()
