"""Preserve original and merged references, then import through official Blender tools."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[3]
NAMES = ('HouseWall01', 'HouseWall04', 'HouseWall05', 'HouseWall06')
CONVERTER = os.environ['MU_BMDCONV']
BLENDER = os.environ['BLENDER']
sys.path.insert(0, str(REPO / 'tools'))
import mu_texture


def run_blender(arguments, log):
    result = subprocess.run([BLENDER, '-b', '--python-exit-code', '1', '--python-expr', 'import sys; sys.dont_write_bytecode=True', *arguments], capture_output=True, text=True)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'Blender failed: {log}')


def prepare(name):
    folder = ROOT / name
    historical = REPO / 'assets-work/World1/Architecture02' / name
    for part in ('original', 'baseline', 'exports', 'validation', 'review', 'textures'):
        (folder / part).mkdir(parents=True, exist_ok=True)
    for stage, source in [('original', historical / 'original' / f'{name}.bmd'), ('baseline', REPO / 'src/bin/Data/Object1' / f'{name}.bmd')]:
        target = folder / stage / source.name
        if target.exists():
            assert target.read_bytes() == source.read_bytes()
        else:
            shutil.copy2(source, target)
        info = subprocess.check_output([CONVERTER, 'info', str(target)], text=True)
        (folder / stage / 'info.txt').write_text(info)
        subprocess.run([CONVERTER, 'bmd2smd', str(target), str(folder / stage / 'smd')], check=True)
        run_blender(['--python', str(REPO / 'tools/blender/mu_bmd_import.py'), '--', '--bmd', str(target), '--data', str(REPO / 'src/bin/Data/Object1'), '--out', str(folder / stage / 'source.blend'), '--bmdconv', CONVERTER], folder / stage / 'import.txt')
    shutil.copy2(historical / 'original/placements.json', folder / 'placements.json')
    textures = ['tile_wood02.OZJ', 'tile_wood03.OZJ'] if name in NAMES[2:] else ['tile_wood01.OZJ', 'tile_wood02.OZJ', 'tile_ston04.OZJ']
    hashes = {}
    for filename in textures:
        source = REPO / 'src/bin/Data/Object1' / filename
        hashes[str(source.relative_to(REPO))] = hashlib.sha256(source.read_bytes()).hexdigest()
        shutil.copy2(source, folder / 'exports' / filename)
        mu_texture.unwrap_file(source, folder / 'textures' / (source.stem + '.jpg'))
    (folder / 'validation/frozen-textures.json').write_text(json.dumps(hashes, indent=2) + '\n')


if __name__ == '__main__':
    for name in NAMES:
        prepare(name)
