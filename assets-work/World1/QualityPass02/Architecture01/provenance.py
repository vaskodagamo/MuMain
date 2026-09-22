"""Retain exact final sources, images, exports and immutable baseline fingerprints."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parent
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


records={}
for name in NAMES:
    folder=ROOT/name
    paths=[folder/'source.blend',folder/'original'/f'{name}.bmd',folder/'baseline'/f'{name}.bmd',folder/'exports'/f'{name}.bmd']
    paths+=list((folder/'review').glob('*.png'))+list((folder/'review').glob('*.jpg'))
    records[name]={str(path.relative_to(ROOT)):digest(path) for path in paths}
report=dict(baseline_revision='7b808473',toolchain=dict(blender='5.2.2',source_tools='3.4.3',importer='tools/blender/mu_bmd_import.py',exporter='tools/blender/mu_bmd_export.py'),assets=records,assemblies={str(p.relative_to(ROOT)):digest(p) for p in (ROOT/'review').glob('*') if p.is_file()},scripts={p.name:digest(p) for p in ROOT.glob('*.py')},runtime_installation=False,new_client_verification=False)
(ROOT/'provenance.json').write_text(json.dumps(report,indent=2)+'\n')
