"""Import hash-frozen readonly current neighbors for actual-placement contact review."""
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
import pipeline
INTEGRATION=pipeline.REPO.parent/'MuMain-environment-remake'
MAP=json.loads((INTEGRATION/'assets-work/World1/coordination/dependency-map.json').read_text())['models']
SELECTIONS={'Waterspout01-0':('Waterspout01',0,450,('Fence03','Grass01','Grass04','Grass05','Grass06','HouseEtc01'))}



def freeze_neighbor(name):
    folder=ROOT/'context'/name;folder.mkdir(parents=True,exist_ok=True)
    original=INTEGRATION/'src/bin/Data/Object1'/f'{name}.bmd'
    manifest=ROOT/'context/frozen-neighbor-hashes.json'
    if manifest.exists():
        retained=json.loads(manifest.read_text())
        if name in retained and (folder/'source.blend').exists():
            expected=retained[name][str(original.relative_to(INTEGRATION))]
            assert hashlib.sha256((folder/original.name).read_bytes()).hexdigest()==expected
            return retained[name]
    target=folder/original.name
    if target.exists():assert target.read_bytes()==original.read_bytes()
    else:shutil.copy2(original,target)
    dependencies={str(original.relative_to(INTEGRATION)):hashlib.sha256(original.read_bytes()).hexdigest()}
    for paths in MAP[name]['textures'].values():
        for path in paths:dependencies[path]=hashlib.sha256((INTEGRATION/path).read_bytes()).hexdigest()
    if not (folder/'source.blend').exists():
        pipeline.blender(['--python',pipeline.REPO/'tools/blender/mu_bmd_import.py','--','--bmd',target,'--data',INTEGRATION/'src/bin/Data/Object1','--out',folder/'source.blend','--bmdconv',pipeline.CONVERTER],folder/'import.txt')
    return dependencies


if __name__=='__main__':
    groups={};neighbors=set()
    for label,(name,index,radius,allowed) in SELECTIONS.items():
        anchor=MAP[name]['placements'][index]
        records=[dict(name=name,index=index,**anchor)]
        for other in allowed:
            for i,record in enumerate(MAP.get(other,{}).get('placements',[])):
                if math.dist(record['position'][:2],anchor['position'][:2])<radius:
                    records.append(dict(name=other,index=i,**record))
                    if other!='Light03':neighbors.add(other)
        groups[label]=dict(anchor=anchor['position'],records=records)
    hashes={name:freeze_neighbor(name) for name in sorted(neighbors)}
    (ROOT/'context/groups.json').write_text(json.dumps(groups,indent=2)+'\n')
    (ROOT/'context/frozen-neighbor-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
