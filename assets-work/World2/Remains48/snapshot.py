"""Snapshot the assigned model, frozen texture and all actual placement records."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
HASHES = {'Object48.bmd': '17921ffd7b933790593e3f7cb21b1a2ca76ebbb863e3b17be3461b1651b2d686',
          'bons.OZJ': '0ed1d9cb1c8da9e0f128d51a607a7b4ecab8ee89bfdab67917ba0e7116542ec5'}


def main():
    folder = ROOT/'Object48'
    for name in ('baseline','original','textures','exports','roundtrip','validation/baseline','validation/new','validation/roundtrip','review'):
        (folder/name).mkdir(parents=True, exist_ok=True)
    for name, expected in HASHES.items():
        source = REPO/'src/bin/Data/Object2'/name
        assert hashlib.sha256(source.read_bytes()).hexdigest() == expected
        for stage in ('baseline','original'):
            target = folder/stage/name
            if target.exists():
                assert target.read_bytes() == source.read_bytes()
            else:
                shutil.copy2(source, target)
        if name.endswith('.OZJ'):
            shutil.copy2(source, folder/'exports'/name)
    (ROOT/'baseline-sha256.json').write_text(json.dumps(HASHES, indent=2))
    converter = os.environ['MU_BMDCONV']
    subprocess.run([converter,'bmd2smd',str(folder/'baseline/Object48.bmd'),str(folder/'validation/baseline')],check=True)
    (folder/'validation/info-before.txt').write_bytes(subprocess.check_output([converter,'info',str(folder/'baseline/Object48.bmd')]))
    data = json.loads((REPO/'assets-work/Environment/coordination/dungeon-readiness.json').read_text())
    records = data['models']['Object48']['placements']
    assert len(records) == 385
    (folder/'placements.json').write_text(json.dumps(records,indent=2))


if __name__ == '__main__':
    main()
