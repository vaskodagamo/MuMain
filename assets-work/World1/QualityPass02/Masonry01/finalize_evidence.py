"""Refresh exact current/candidate comparison and hashes after the last successful build."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[3]
NAMES = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')
CONVERTER = Path(os.environ.get('MU_BMDCONV', REPOSITORY.parent/'MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv'))


def refresh(name):
    folder = ROOT/name
    comparison = subprocess.run([str(CONVERTER),'compare',str(folder/'baseline'/(name+'.bmd')),
        str(folder/'exports'/(name+'.bmd'))],capture_output=True,text=True)
    assert 'DIFFERENT' in comparison.stdout
    (folder/'validation/current-compare.txt').write_text(comparison.stdout+comparison.stderr)
    metadata = json.loads((folder/'validation/blender.json').read_text())
    (folder/'notes.md').write_text(f'''# {name}: dressed-cap candidate

{metadata['triangles']} triangles, unchanged original bind bounds {metadata['bounds_after']}.
Original/current/candidate game files and packed source preserved. Broad 5-unit inward chamfers target exposed upper c_wall04 stone edges. Original coarse dragon motif and frozen 512px diffuse painting retained. Skeleton, one-frame action, lock metadata and original mesh/material order preserved.

Converter validation, original/current comparisons, raw normal-node bindings, UV/winding tests, full authored triangle material/UV/bone/winding match and bidirectional vertex correspondence are in validation/. StoneMuWall02 alone retains the two documented original c_wall05 custom corner-normal anomalies; the audit proves unchanged incidence and original triangle correspondence.

Matching actual-export renders, reverse/quarter-scale/wireframes and the parent actual-transform assemblies are offline evidence. No client verification of this candidate. Awaiting independent artistic acceptance; source Data remains merged baseline.
''')


def main():
    for name in NAMES:
        refresh(name)
    paths = [p for name in NAMES for sub in ('exports','review','validation') for p in (ROOT/name/sub).rglob('*') if p.is_file()]
    paths += list((ROOT/'review-assemblies').glob('*'))
    hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.is_file()}
    (ROOT/'candidate-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')


if __name__=='__main__':
    main()
