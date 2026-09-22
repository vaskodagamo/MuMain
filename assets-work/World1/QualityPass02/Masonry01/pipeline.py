"""Run official BMD interchange and retained converter audits for this owned batch."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[3]
NAMES = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')
BLENDER = os.environ['MU_BLENDER']
CONVERTER = os.environ['MU_BMDCONV']


def blender(log, *arguments):
    result = subprocess.run([BLENDER, '-b', '--python-exit-code', '1', *map(str, arguments)],
        capture_output=True, text=True, cwd=REPOSITORY, env={**os.environ, 'PYTHONDONTWRITEBYTECODE': '1'})
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(str(log))


def import_stage(folder, stage):
    destination = folder / stage / 'source.blend' if stage == 'baseline' else folder / 'validation/reimported.blend'
    model = folder / ('baseline' if stage == 'baseline' else 'exports') / (folder.name + '.bmd')
    blender(folder / 'validation' / ('import-' + stage + '.txt'), '--python',
        REPOSITORY / 'tools/blender/mu_bmd_import.py', '--', '--bmd', model,
        '--textures', folder / 'textures', '--out', destination, '--bmdconv', CONVERTER)


def load_helper(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def validate():
    old = ROOT.parents[1] / 'StaticBatch01/validate_export.py'
    validator = load_helper('validator', old)
    validator.CONVERTER = Path(CONVERTER)
    validator.REPOSITORY = REPOSITORY
    validator.EXPECTED_TEXTURES = dict(HouseEtc01=['c_wall04.jpg', 'c_wall06.jpg'],
        StoneMuWall02=['c_wall06.jpg', 'c_wall05.jpg', 'c_wall04.jpg'],
        StoneMuWall03=['c_wall04.jpg', 'c_wall06.jpg'])
    validator.EXPECTED_KEYS = {name: 1 for name in NAMES}
    raw = load_helper('raw', ROOT.parents[1] / 'Architecture03/raw_bindings.py')
    for name in NAMES:
        folder = ROOT / name
        validator.validate(folder)
        report = {stage: raw.audit(folder / stage / (name + '.bmd'))
                  for stage in ('original', 'baseline', 'exports')}
        for mesh in report['exports']['mesh_bindings']:
            assert not mesh['vertex_to_normal_node_mismatch_corner_counts']
        (folder / 'validation/raw-normal-bindings.json').write_text(json.dumps(report, indent=2))
        import_stage(folder, 'exports')


def main():
    stage = sys.argv[1]
    if stage == 'prepare':
        for name in NAMES:
            import_stage(ROOT / name, 'baseline')
    elif stage == 'build':
        blender(ROOT / 'build-log.txt', '--python', ROOT / 'cap_source.py')
        for name in NAMES:
            folder = ROOT / name
            blender(folder / 'validation/export.txt', folder / 'source.blend', '--python',
                REPOSITORY / 'tools/blender/mu_bmd_export.py', '--', '--out',
                folder / 'exports' / (name + '.bmd'), '--bmdconv', CONVERTER)
        validate()
        blender(ROOT / 'render-log.txt', '--python', ROOT / 'review.py')
        blender(ROOT / 'assembly-log.txt', '--python', ROOT / 'assemblies.py')
        subprocess.run([sys.executable, str(ROOT / 'audit_exports.py')], check=True)
    elif stage == 'export':
        for name in NAMES:
            folder = ROOT / name
            blender(folder / 'validation/export.txt', folder / 'source.blend', '--python',
                REPOSITORY / 'tools/blender/mu_bmd_export.py', '--', '--out',
                folder / 'exports' / (name + '.bmd'), '--bmdconv', CONVERTER)
    elif stage == 'validate':
        validate()
    else:
        raise ValueError(stage)


if __name__ == '__main__':
    main()
