"""Reproduce sculpted fountain dragon and masked painting candidates with official Blender import/export."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
NAMES=tuple(os.environ.get('FOUNTAIN_NAMES','Waterspout01').split(','))
BLENDER=os.environ['BLENDER']
CONVERTER=os.environ['MU_BMDCONV']
PAINT=os.environ.get('FOUNTAIN_PAINT','paint02')
ENV={**os.environ,'PYTHONDONTWRITEBYTECODE':'1','FOUNTAIN_NAMES':','.join(NAMES),'FOUNTAIN_PAINT':PAINT}
EVIDENCE=json.loads((ROOT/'baseline-evidence.json').read_text())
TEXTURES={name:[Path(p).name for p in item['game_files'] if not p.endswith('.bmd')] for name,item in EVIDENCE.items()}


def blender(arguments,log,names=None):
    command=[BLENDER,'-b','--python-exit-code','1','--python-expr','import sys; sys.dont_write_bytecode=True',*map(str,arguments)]
    result=subprocess.run(command,capture_output=True,text=True,env=ENV if names is None else {**ENV,'FOUNTAIN_NAMES':','.join(names)})
    log.write_text(result.stdout+result.stderr)
    if result.returncode: raise RuntimeError(f'Blender failed: {log}')


def converter(*arguments,check=True):
    result=subprocess.run([CONVERTER,*map(str,arguments)],capture_output=True,text=True)
    if check and result.returncode: raise RuntimeError(result.stdout+result.stderr)
    return result.stdout


def prepare(name):
    import hashlib
    folder=ROOT/name
    for part in ('original','baseline','exports','textures','validation','review'):
        (folder/part).mkdir(parents=True,exist_ok=True)
    old=REPO/'assets-work/World1/Statues01'/name
    for original in (old/'original').iterdir():
        if original.suffix.lower() in ('.ozj','.ozt','.jpg','.tga'):shutil.copy2(original,folder/'original'/original.name)
    # Baseline painting remains reproducible after the candidate is installed.
    for filename in TEXTURES[name]:
        source=ROOT/'artwork/baseline-reagon_waterspout.OZJ' if filename=='reagon_waterspout.OZJ' else REPO/'src/bin/Data/Object1'/filename
        expected=EVIDENCE[name]['game_files']['src/bin/Data/Object1/'+filename]
        assert hashlib.sha256(source.read_bytes()).hexdigest()==expected
        shutil.copy2(source,folder/'baseline'/filename)
    for stage,path in [('original',old/'original'/f'{name}.bmd'),('baseline',REPO/'src/bin/Data/Object1'/f'{name}.bmd')]:
        target=folder/stage/path.name
        if target.exists():
            expected=EVIDENCE[name]['sha256'] if stage=='baseline' else hashlib.sha256(path.read_bytes()).hexdigest()
            assert hashlib.sha256(target.read_bytes()).hexdigest()==expected
        elif stage=='baseline':
            revision='7b808473'
            blob=subprocess.run(['git','show',f'{revision}:src/bin/Data/Object1/{name}.bmd'],cwd=REPO,capture_output=True,check=True).stdout
            assert hashlib.sha256(blob).hexdigest()==EVIDENCE[name]['sha256']
            target.write_bytes(blob)
        else: shutil.copy2(path,target)
        (folder/stage/'info.txt').write_text(converter('info',target))
        converter('bmd2smd',target,folder/stage/'smd')
        if (folder/stage/'source.blend').exists(): continue
        blender(['--python',REPO/'tools/blender/mu_bmd_import.py','--','--bmd',target,'--data',folder/stage,'--out',folder/stage/'source.blend','--bmdconv',CONVERTER],folder/stage/'import.txt')
    shutil.copy2(old/'original/placements.json',folder/'placements.json')
    hashes={}
    for filename in TEXTURES[name]:
        if filename=='reagon_waterspout.OZJ':
            source=folder/'textures'/PAINT/filename
        else:
            source=REPO/'src/bin/Data/Object1'/filename
            key=str(source.relative_to(REPO));hashes[key]=EVIDENCE[name]['game_files'][key]
            assert hashlib.sha256(source.read_bytes()).hexdigest()==hashes[key]
        shutil.copy2(source,folder/'exports'/filename)
    (folder/'validation/frozen-textures.json').write_text(json.dumps(hashes,indent=2)+'\n')


def export(name):
    folder=ROOT/name
    blender([folder/'source.blend','--python',REPO/'tools/blender/mu_bmd_export.py','--','--out',folder/'exports'/f'{name}.bmd','--bmdconv',CONVERTER],folder/'validation/export.txt')
    converter('bmd2smd',folder/'exports'/f'{name}.bmd',folder/'validation/new')
    blender(['--python',REPO/'tools/blender/mu_bmd_import.py','--','--bmd',folder/'exports'/f'{name}.bmd','--out',folder/'validation/reimported.blend','--bmdconv',CONVERTER],folder/'validation/reimport.txt')


if __name__=='__main__':
    stage=sys.argv[1]
    if stage in ('prepare','experiment'):
        for name in NAMES: prepare(name)
    if stage in ('build','experiment'):
        scripts={'Waterspout01':'build_curve.py'}
        for name in NAMES:
            blender(['--python',ROOT/scripts[name]],ROOT/name/'validation/build.txt',[name])
            blender(['--python',ROOT/'apply_paint.py'],ROOT/name/'validation/apply-paint.txt',[name])
    if stage in ('export','experiment'):
        for name in NAMES: export(name)
    if stage=='experiment':
        result=subprocess.run([sys.executable,str(ROOT/'validate.py')],env=ENV,check=True)
        blender(['--python',ROOT/'audit_source.py'],ROOT/'source-audit.txt')
        blender(['--python',ROOT/'authored_normals.py'],ROOT/'authored-normal-audit.txt')
        blender(['--python',ROOT/'motion_proof.py'],ROOT/'motion-proof.txt')
        blender(['--python',ROOT/'posed_source.py'],ROOT/'posed-source-audit.txt')
        subprocess.run([sys.executable,str(ROOT/'shading_proof.py')],env=ENV,check=True)
        blender(['--python',ROOT/'attachment_proof.py'],ROOT/'attachment-proof.txt')
        blender(['--python',ROOT/'audit_paint_source.py'],ROOT/'paint-source-audit.txt')
    if stage in ('review','experiment'):
        blender(['--python',ROOT/'review.py'],ROOT/'review.txt')
