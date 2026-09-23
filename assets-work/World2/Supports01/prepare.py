"""Freeze current support inputs, consumers, placements and prior context evidence."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
CONVERTER = '/Users/lukasmac/Documents/claude-test-mumain/MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv'
BASE_COMMIT = '7c25cce6911a0e3f8737e3214dcd12c982c18728'
NAMES = ('Object06', 'Object13', 'Object15', 'Object04')
TEXTURES = ('deep_wall01.OZJ', 'deep_wall04.OZJ')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare_model(name, records):
    folder = ROOT / name
    for child in ('baseline', 'original', 'textures', 'exports', 'validation/baseline', 'review'):
        (folder / child).mkdir(parents=True, exist_ok=True)
    source = REPO / 'src/bin/Data/Object2' / (name+'.bmd')
    original = subprocess.check_output(['git','show',f'{BASE_COMMIT}:src/bin/Data/Object2/{name}.bmd'],cwd=REPO)
    assert source.read_bytes() == original, 'Baseline changed; do not overwrite retained inputs'
    for stage in ('original', 'baseline'):
        shutil.copy2(source, folder / stage / source.name)
    for texture in (*TEXTURES, *(['deep_wall03.OZJ'] if name=='Object04' else [])):
        source_texture = REPO / 'src/bin/Data/Object2' / texture
        expected = subprocess.check_output(['git','show',f'{BASE_COMMIT}:src/bin/Data/Object2/{texture}'],cwd=REPO)
        assert source_texture.read_bytes() == expected, 'Frozen texture changed'
        for stage in ('original', 'baseline'):
            shutil.copy2(source_texture, folder / stage / texture)
        subprocess.run([sys.executable, str(REPO/'tools/mu_texture.py'), 'unwrap', str(source_texture),
                        '--out', str(folder/'textures'/Path(texture).with_suffix('.jpg'))], check=True, capture_output=True)
    result = subprocess.run([CONVERTER, 'bmd2smd', str(source), str(folder/'validation/baseline')], capture_output=True, check=True)
    (folder/'validation/convert-baseline.txt').write_bytes(result.stdout+result.stderr)
    (folder/'placements.json').write_text(json.dumps(records[name]['placements'], indent=2)+'\n')


def main():
    data = json.loads((REPO/'assets-work/Environment/coordination/dungeon-readiness.json').read_text())['models']
    consumers = {texture:dict(sha256=digest(REPO/'src/bin/Data/Object2'/texture), consumers=[]) for texture in TEXTURES}
    for path in sorted((REPO/'src/bin/Data/Object2').glob('*.bmd')):
        output = subprocess.run([CONVERTER, 'info', str(path)], capture_output=True, check=True).stdout.decode('utf8', 'backslashreplace')
        for texture, record in consumers.items():
            if 'texture='+Path(texture).with_suffix('.jpg').name in output:
                record['consumers'].append(dict(name=path.stem, sha256=digest(path)))
        if path.stem in NAMES:
            prepare_model(path.stem, data)
            (ROOT/path.stem/'validation/info-before.txt').write_text(output)
    shutil.copytree(REPO/'assets-work/World2/Readiness01/assemblies', ROOT/'previous-context', dirs_exist_ok=True)
    snapshots = {str(p.relative_to(REPO)):digest(p) for p in (REPO/'src/bin/Data/Object2').iterdir() if p.is_file()}
    (ROOT/'data-hashes.json').write_text(json.dumps(snapshots, indent=2)+'\n')
    (ROOT/'texture-consumers.json').write_text(json.dumps(consumers, indent=2)+'\n')
    (ROOT/'baseline-commit.txt').write_text(BASE_COMMIT+'\n')


if __name__ == '__main__':
    main()
