"""Run isolated official Blender stages with strict process and logged-error checks."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
NAMES = ('Object06', 'Object13', 'Object15')
BLENDER = '/Users/lukasmac/Documents/claude-test-mumain/astra-tools/Blender.app/Contents/MacOS/Blender'
CONVERTER = '/Users/lukasmac/Documents/claude-test-mumain/MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv'
ENV = dict(os.environ, MU_BMDCONV=CONVERTER,
           BLENDER_USER_SCRIPTS='/Users/lukasmac/Documents/claude-test-mumain/astra-tools/blender-user/scripts',
           BLENDER_USER_CONFIG='/Users/lukasmac/Documents/claude-test-mumain/astra-tools/blender-user/config')


def blender(log, *arguments):
    result = subprocess.run([BLENDER, '-b', '--python-exit-code', '1', *map(str, arguments)],
                            cwd=REPO, env=ENV, capture_output=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_bytes(result.stdout+result.stderr)
    text = (result.stdout+result.stderr).decode('utf8', 'replace')
    assert result.returncode == 0, str(log)
    assert not re.search(r'(?m)^\s*(?:ERROR:|Error:)|[1-9][0-9]* Errors|warning: no texture found', text), str(log)


def main():
    stage = sys.argv[1]
    if stage == 'prepare':
        for name in (*NAMES, 'Object04'):
            blender(ROOT/name/'validation/import.txt', '--python', ROOT/'official_io.py', '--', 'import', name, 'baseline')
        return
    if stage == 'export':
        for name in NAMES:
            for texture in (ROOT/name/'baseline').glob('*.OZJ'):
                assert texture.read_bytes()==(REPO/'src/bin/Data/Object2'/texture.name).read_bytes()
                shutil.copy2(texture,ROOT/name/'exports'/texture.name)
            blender(ROOT/name/'validation/export.txt', ROOT/name/'source.blend', '--python', ROOT/'official_io.py', '--', 'export', name)
            blender(ROOT/name/'validation/reimport.txt', '--python', ROOT/'official_io.py', '--', 'import', name, 'exports')
        return
    blender(ROOT/(stage+'.txt'), '--python', ROOT/(stage+'.py'))


if __name__ == '__main__':
    main()
