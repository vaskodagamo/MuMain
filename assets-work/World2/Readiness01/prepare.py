"""Snapshot unchanged high-frequency architecture before planning later batches."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
NAMES = ('Object01', 'Object04', 'Object06', 'Object13', 'Object15')
ENV = {**os.environ, 'DUNGEON_PREVIEW_NAMES': ','.join(NAMES),
       'PYTHONDONTWRITEBYTECODE': '1'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dependencies(source):
    raw = subprocess.check_output([ENV['MU_BMDCONV'], 'info', str(source)])
    records = {str(source.relative_to(REPO)): digest(source)}
    for name in re.findall(rb'texture=(\S+)', raw):
        texture = Path(name.decode('ascii'))
        extension = {'.jpg': '.OZJ', '.tga': '.OZT'}[texture.suffix.lower()]
        path = source.parent / texture.with_suffix(extension)
        records[str(path.relative_to(REPO))] = digest(path)
    return records


def blender(script, log, arguments=()):
    command = [ENV['MU_BLENDER'], '-b', '--python-exit-code', '1',
               '--python', str(ROOT / script), *map(str, arguments)]
    result = subprocess.run(command, capture_output=True, text=True, env=ENV)
    log.write_text(result.stdout + result.stderr)
    result.check_returncode()


def snapshot(name):
    folder = ROOT / name
    folder.mkdir(exist_ok=True)
    source = REPO / 'src/bin/Data/Object2' / (name + '.bmd')
    record = {'source': str(source.relative_to(REPO)), 'sha256': digest(source),
              'dependencies': dependencies(source),
              'purpose': 'Read-only baseline triage, not an accepted remake'}
    existing = folder / 'provenance.json'
    if existing.exists():
        old = json.loads(existing.read_text())
        assert old['sha256'] == record['sha256'], 'Preserve older evidence separately'
        assert old.get('dependencies', record['dependencies']) == record['dependencies']
    (folder / 'provenance.json').write_text(json.dumps(record, indent=2) + '\n')
    blender('import_preview.py', folder / 'import.txt',
            ['--', '--bmd', source, '--out', folder / 'baseline.blend',
             '--bmdconv', ENV['MU_BMDCONV']])


def finalize():
    for name in NAMES:
        folder = ROOT / name
        record = json.loads((folder / 'provenance.json').read_text())
        record['dependencies'] = dependencies(REPO / record['source'])
        record['evidence'] = {file: digest(folder / file)
                              for file in ('baseline.blend', 'baseline.png')}
        (folder / 'provenance.json').write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    for asset in NAMES:
        snapshot(asset)
    blender('render_preview.py', ROOT / 'render.txt')
    finalize()
