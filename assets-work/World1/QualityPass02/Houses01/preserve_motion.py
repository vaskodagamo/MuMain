"""Retain official dome geometry while packaging exact immutable baseline motion."""
import hashlib
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
CONVERTER=os.environ['MU_BMDCONV']
spec=importlib.util.spec_from_file_location('reviewed_contract',REPO/'assets-work/World1/Architecture03/preserve_contract.py')
contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)


def run(*args):
    result=subprocess.run([CONVERTER,*map(str,args)],capture_output=True,text=True,check=True)
    return result.stdout+result.stderr


def preserve():
    folder=ROOT/'House04';name=folder.name
    official=folder/'validation/official-export';locked=folder/'validation/locked-motion'
    official.mkdir(exist_ok=True);locked.mkdir(exist_ok=True)
    game=folder/'exports'/f'{name}.bmd';shutil.copy2(game,official/game.name)
    messages=[run('bmd2smd',game,official)]
    baseline_dir=folder/'baseline/smd'
    baseline=(baseline_dir/f'{name}.smd').read_text();exported=(official/f'{name}.smd').read_text()
    assert baseline.split('nodes\n')[1].split('\nend')[0]==exported.split('nodes\n')[1].split('\nend')[0]
    original=contract.triangle_blocks(baseline);triangles=contract.triangle_blocks(exported)
    available=set(range(len(triangles)));restored=0
    for old in original:
        if old[0]=='tile_wood03.jpg':continue
        candidates=[i for i in available if contract.same_triangle(old,triangles[i])]
        assert candidates,('nonroof controlled triangle missing',old)
        i=candidates[0];triangles[i]=old;available.remove(i);restored+=1
    boundary=contract.restore_panel_boundaries(triangles,available,original)
    body='\n'.join(line for block in triangles for line in block)+'\nend\n'
    (locked/f'{name}.smd').write_text(baseline.split('triangles\n')[0]+'triangles\n'+body)
    for path in [*baseline_dir.glob(f'{name}_a*.smd'),baseline_dir/f'{name}.actions.txt']:shutil.copy2(path,locked/path.name)
    messages.extend([run('validate',locked/f'{name}.smd'),run('validate',locked/f'{name}_a00.smd','--animation'),run('smd2bmd',locked/f'{name}.smd',game,'--manifest',locked/f'{name}.actions.txt')])
    assert (locked/f'{name}_a00.smd').read_bytes()==(baseline_dir/f'{name}_a00.smd').read_bytes()
    report=dict(reason='Official Blender SourceTools Euler quantization near gimbal configuration; preserve exact baseline engine contract without relaxing validators',source='Official authored Blender export retained under validation/official-export',exact_baseline_smd_bind_actions=True,restored_nonroof_triangles=restored,roof_anchor_restoration=boundary,official_sha256=hashlib.sha256((official/game.name).read_bytes()).hexdigest(),final_sha256=hashlib.sha256(game.read_bytes()).hexdigest())
    (folder/'validation/contract-preservation.json').write_text(json.dumps(report,indent=2)+'\n')
    (folder/'validation/contract-preservation.txt').write_text('\n'.join(messages))

if __name__=='__main__':preserve()
