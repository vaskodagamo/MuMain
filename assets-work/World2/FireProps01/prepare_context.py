"""Import read-only neighbors at their actual Dungeon placement coordinates."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
sys.path.insert(0,str(ROOT))
import pipeline
GROUPS={'Object42':[81,82,69,67,83,28], 'Object43':[295,298,296,285,299,286,291], 'Object42-wall':[297,280,281]}


def main():
    data=json.loads((REPO/'assets-work/Environment/coordination/dungeon-readiness.json').read_text())['models']
    indexed={p['index']:(name,p) for name,model in data.items() for p in model['placements']}
    groups={name:[indexed[index] for index in indices] for name,indices in GROUPS.items()}
    (ROOT/'context').mkdir(exist_ok=True)
    (ROOT/'context/placements.json').write_text(json.dumps(groups,indent=2)+'\n')
    sources={}
    for name in sorted({name for records in groups.values() for name,_ in records}):
        path=REPO/'src/bin/Data/Object2'/f'{name}.bmd'
        folder=ROOT/'context/imports'/name
        folder.mkdir(parents=True,exist_ok=True)
        info=subprocess.check_output([pipeline.CONVERTER,'info',str(path)])
        dependencies=[path]
        for texture in re.findall(rb'texture=(\S+)',info):
            t=Path(texture.decode());dependencies.append(path.parent/t.with_suffix({'.jpg':'.OZJ','.tga':'.OZT'}[t.suffix]))
        sources[name]={str(p.relative_to(REPO)):hashlib.sha256(p.read_bytes()).hexdigest() for p in dependencies}
        if (folder/'source.blend').exists():
            continue
        pipeline.blender(['--python',ROOT/'official_adapter.py','--','--bmd',path,'--data',path.parent,'--out',folder/'source.blend','--bmdconv',pipeline.CONVERTER],folder/'import.txt',name,'import')
    (ROOT/'context/sources.json').write_text(json.dumps(sources,indent=2)+'\n')


if __name__=='__main__':main()
