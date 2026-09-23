"""Portable final exported-container, budget and recorded proof checks."""
import hashlib
import json
import os
from pathlib import Path
ROOT=Path(__file__).resolve().parent
for name in os.environ.get('STATUE_NAMES','StoneStatue01,StoneStatue03,SteelStatue01').split(','):
 folder=ROOT/name
 frozen=json.loads((folder/'validation/frozen-textures.json').read_text())
 for filename,sha in frozen.items():assert hashlib.sha256((folder/'exports'/Path(filename).name).read_bytes()).hexdigest()==sha
 authored=json.loads((folder/'validation/authored.json').read_text());assert authored['triangles']<=1500
 reports={key:json.loads((folder/'validation'/filename).read_text()) for key,filename in [('converter','summary.json'),('authored_triangles','authored-match.json'),('raw_normals','authored-raw-normals.json'),('contacts','contact-contract.json')]}
 assert all(report['status']=='PASS' for report in reports.values())
 assert reports['authored_triangles']['position_tolerance']<=.0003 and reports['authored_triangles']['uv_tolerance']<=1e-6
 actual=hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest();assert reports['converter']['sha256']==actual
 out=dict(status='PASS',bmd_sha256=actual,triangles=authored['triangles'],prop_target=1500,export_texture_containers_exact=frozen,client_verified=False,proofs=reports)
 (folder/'validation/final-proof.json').write_text(json.dumps(out,indent=2)+'\n')
 print(name,'final budget/frozen-container/proof checks PASS')
