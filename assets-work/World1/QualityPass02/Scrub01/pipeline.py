"""Reproduce geometry-only scrub candidates with official Blender import/export."""
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
NAMES=tuple(os.environ.get('SCRUB_NAMES','Tree09,Tree10,Grass03,Grass04').split(','))
BLENDER=os.environ['BLENDER']
CONVERTER=os.environ['MU_BMDCONV']
ENV={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'}


def blender(arguments,log):
    command=[BLENDER,'-b','--python-exit-code','1','--python-expr','import sys; sys.dont_write_bytecode=True',*map(str,arguments)]
    result=subprocess.run(command,capture_output=True,text=True,env=ENV)
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
    old=REPO/'assets-work/World1/Scrub01'/name
    for stage,path in [('original',old/'original'/f'{name}.bmd'),('baseline',REPO/'src/bin/Data/Object1'/f'{name}.bmd')]:
        target=folder/stage/path.name
        if target.exists(): assert target.read_bytes()==path.read_bytes()
        else: shutil.copy2(path,target)
        (folder/stage/'info.txt').write_text(converter('info',target))
        converter('bmd2smd',target,folder/stage/'smd')
        if (folder/stage/'source.blend').exists(): continue
        blender(['--python',REPO/'tools/blender/mu_bmd_import.py','--','--bmd',target,'--data',REPO/'src/bin/Data/Object1','--out',folder/stage/'source.blend','--bmdconv',CONVERTER],folder/stage/'import.txt')
    shutil.copy2(old/'original/placements.json',folder/'placements.json')
    textures=('tree_07.OZT',) if name.startswith('Tree') else ('tree_01.OZT','tree_02.OZT')
    hashes={}
    for filename in textures:
        path=REPO/'src/bin/Data/Object1'/filename
        hashes[str(path.relative_to(REPO))]=hashlib.sha256(path.read_bytes()).hexdigest()
        shutil.copy2(path,folder/'exports'/filename)
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
        blender(['--python',ROOT/'build_source.py'],ROOT/'build.txt')
    if stage in ('export','experiment'):
        for name in NAMES: export(name)
    if stage=='experiment':
        result=subprocess.run([sys.executable,str(ROOT/'validate.py')],env=ENV,check=True)
        blender(['--python',ROOT/'audit_source.py'],ROOT/'source-audit.txt')
    if stage in ('review','experiment'):
        blender(['--python',ROOT/'review.py'],ROOT/'review.txt')
