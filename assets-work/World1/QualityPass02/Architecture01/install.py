"""Install only four reviewed architecture exports into this isolated source checkout."""
import hashlib
import json
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
from write_notes import accepted

assert accepted(), "Final independent review must match every candidate hash"

ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')

for name in NAMES:
    folder=ROOT/name
    source=folder/'exports'/f'{name}.bmd'
    target=REPO/'src/bin/Data/Object1'/source.name
    summary=json.loads((folder/'validation/summary.json').read_text())
    assert summary['status']=='PASS'
    assert hashlib.sha256(source.read_bytes()).hexdigest()==summary['sha256']
    assert target.read_bytes() in ((folder/'baseline'/target.name).read_bytes(),source.read_bytes())
    for path,sha in json.loads((folder/'validation/frozen-textures.json').read_text()).items():
        assert hashlib.sha256((REPO/path).read_bytes()).hexdigest()==sha
    shutil.copy2(source,target)
    print('Installed isolated source',target.relative_to(REPO))
