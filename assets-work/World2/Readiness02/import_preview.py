"""Delegate unchanged official import, parsing ASCII actions around legacy comments."""
from pathlib import Path
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'tools/blender'))
import mu_bmd_import as official


def action_lines(path):
    records = []
    for line in Path(path).read_bytes().splitlines():
        if not line.startswith(b'action '):
            continue
        words = line.decode('ascii').split()
        record = {'index': int(words[1])}
        for word in words[2:]:
            key, _, value = word.partition('=')
            record[key] = value
        records.append(record)
    return records


official.read_manifest = action_lines
official.main()
