"""Validate the integrated quality-pass scope and exact texture resolution.

Run from any cwd; accepts a JSON ledger of accepted game-path/export-path pairs.
Does not write game files, alter the converter, or imply artistic/client acceptance.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASELINE = '7b808473'
DATA_PREFIX = 'src/bin/Data/'
MODEL_PREFIX = DATA_PREFIX + 'Object1/'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True)


def check_scope(ledger):
    changed = set(git('diff', '--name-only', BASELINE, '--', DATA_PREFIX).splitlines())
    expected = set(ledger)
    if changed != expected:
        raise ValueError(f'Unexpected/missing game paths: {sorted(changed ^ expected)}')
    for game, export in ledger.items():
        if not game.startswith(MODEL_PREFIX):
            raise ValueError(f'Out-of-phase asset: {game}')
        if digest(ROOT / game) != digest(ROOT / export):
            raise ValueError(f'Export mismatch: {game}')
    return {game: {'sha256': digest(ROOT / game), 'export': export}
            for game, export in ledger.items()}


def check_textures(converter):
    folder = ROOT / MODEL_PREFIX
    available = {p.name.lower(): p for p in folder.iterdir() if p.is_file()}
    records = {}
    for model in sorted(folder.glob('*.bmd')):
        info = subprocess.check_output([str(converter), 'info', str(model)], text=True)
        names = re.findall(r'texture=(\S+)', info)
        resolved = []
        for name in names:
            suffix = {'.jpg': '.OZJ', '.tga': '.OZT'}.get(Path(name).suffix.lower())
            if suffix is None:
                raise ValueError(f'Unsupported texture convention: {model.name}: {name}')
            container = available.get(Path(name).with_suffix(suffix).name.lower())
            if container is None:
                raise ValueError(f'Missing dependency: {model.name}: {name}')
            resolved.append({'material': name, 'container': str(container.relative_to(ROOT)),
                             'sha256': digest(container)})
        records[model.stem] = {'sha256': digest(model), 'textures': resolved}
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--converter', type=Path, required=True)
    parser.add_argument('--ledger', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    ledger = json.loads(args.ledger.read_text())
    result = {'baseline': BASELINE, 'revision': git('rev-parse', 'HEAD').strip(),
              'game_files': check_scope(ledger), 'models': check_textures(args.converter),
              'client_verified': False,
              'limitations': 'Scope/hash/resolution checks only; separate art and engine-contract review required.'}
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(f'PASS: {len(result["game_files"])} changed assets, {len(result["models"])} models resolve.')


if __name__ == '__main__':
    main()
