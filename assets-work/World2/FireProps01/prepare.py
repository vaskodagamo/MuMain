"""Snapshot only the assigned two models and their frozen dependencies."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
sys.path.insert(0,str(ROOT))
import pipeline
NAMES=('Object42','Object43')
TEXTURES=('wood01.OZJ','fire0a.OZJ')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(name,records):
    folder=ROOT/name
    for stage in ('original','baseline','exports','validation','review','textures'):
        (folder/stage).mkdir(parents=True,exist_ok=True)
    path=REPO/'src/bin/Data/Object2'/f'{name}.bmd'
    original=subprocess.check_output(['git','show',f'7b808473:src/bin/Data/Object2/{name}.bmd'],cwd=REPO)
    assert path.read_bytes()==original
    for stage in ('baseline','original'):
        shutil.copy2(path,folder/stage/path.name)
        for texture in TEXTURES:
            shutil.copy2(path.parent/texture,folder/stage/texture)
    deps={str(p.relative_to(REPO)):digest(p) for p in [path,*[path.parent/t for t in TEXTURES]]}
    for texture in TEXTURES:
        subprocess.run([sys.executable,str(REPO/'tools/mu_texture.py'),'unwrap',str(path.parent/texture),'--out',str(folder/'textures'/Path(texture).with_suffix('.jpg'))],check=True)
        shutil.copy2(path.parent/texture,folder/'exports'/texture)
    (folder/'provenance.json').write_text(json.dumps(dict(dependencies=deps,placements=records,original_revision='7b808473',current_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO).decode().strip()),indent=2)+'\n')
    for command,args,out in [('info',[path],'baseline/info.txt'),('bmd2smd',[path,folder/'baseline/smd'],'baseline/convert.txt')]:
        result=subprocess.run([pipeline.CONVERTER,command,*map(str,args)],capture_output=True,check=True)
        (folder/out).write_bytes(result.stdout+result.stderr)
    pipeline.prepare(name)


def main():
    data=json.loads((REPO/'assets-work/Environment/coordination/dungeon-readiness.json').read_text())
    for name in NAMES:snapshot(name,data['models'][name]['placements'])
    source=REPO/'src/source/Engine/Object/ZzzObject.cpp'
    (ROOT/'flame-contract.json').write_text(json.dumps(dict(source=str(source.relative_to(REPO)),sha256=digest(source),Object42=dict(type=41,local_anchor=[0,-30,240]),Object43=dict(type=42,local_anchor=[0,0,190]),note='Anchor offsets must be checked against retained source excerpt before use.'),indent=2)+'\n')


if __name__=='__main__':main()
