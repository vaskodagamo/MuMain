"""Exact base/cap mating anchors and angel head/plinth contracts beyond global bounds."""
import json
import os
from pathlib import Path
ROOT=Path(__file__).resolve().parent

def triangles(path):
 s=path.read_text().split('triangles\n')[1].splitlines()
 return [(s[i],[list(map(float,x.split())) for x in s[i+1:i+4]]) for i in range(0,len(s)-1,4)]

for name in os.environ.get('STATUE_NAMES','StoneStatue01,StoneStatue03,SteelStatue01').split(','):
 folder=ROOT/name;old=triangles(folder/'baseline/smd'/f'{name}.smd');new=triangles(folder/'validation/new'/f'{name}.smd')
 points={tuple(row[1:4]) for m,rows in new for row in rows}
 ids=range(0,48) if name=='StoneStatue01' else list(range(0,10))+list(range(50,70)) if name=='SteelStatue01' else list(range(0,20))+list(range(83,100))+list(range(182,199))+list(range(242,248))
 required={tuple(row[1:4]) for i in ids for row in old[i][1]}
 errors=[min(max(abs(a-b) for a,b in zip(p,q)) for q in points) for p in required]
 report=dict(status='PASS' if max(errors)<.0003 else 'FAIL',protected_original_vertices=len(required),maximum_position_delta=max(errors),position_tolerance=.0003,policy='Original pillar shell/cap/base anchors; memorial ground/base and capital/pyramid perimeter anchors; angel complete plinth and head anchors')
 (folder/'validation/contact-contract.json').write_text(json.dumps(report,indent=2)+'\n')
 assert report['status']=='PASS',(name,report)
 print(name,'exact contact/anchor contract PASS')
