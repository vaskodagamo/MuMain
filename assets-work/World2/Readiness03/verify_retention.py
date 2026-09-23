#!/usr/bin/env python3
"""Verify Object37 retention evidence hashes and actual placement records."""

from hashlib import sha256
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = Path(__file__).resolve().parent
ASSEMBLY = PACKAGE / 'assemblies' / 'object37-corridor'
EVIDENCE = ASSEMBLY / 'evidence.json'
BLENDS = {
    'Object37': PACKAGE / 'Object37' / 'baseline.blend',
    'Object01': PACKAGE / 'Object01-current' / 'baseline.blend',
    'Object04': PACKAGE / 'Object04-current' / 'baseline.blend',
    'Object08': PACKAGE / 'Object08-current' / 'baseline.blend',
}


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def check(path, expected, label):
    if not path.is_file():
        raise SystemExit(f'MISSING {label}: {path}')
    actual = digest(path)
    if actual != expected:
        raise SystemExit(f'HASH MISMATCH {label}: expected {expected}, got {actual}')
    print(f'OK {label}')


def main():
    proof = json.loads(EVIDENCE.read_text())
    placement_path = ROOT / proof['placement_source']
    check(placement_path, proof['placement_source_sha256'], 'placement manifest')
    placements = json.loads(placement_path.read_text())['models']

    total = len(placements['Object37']['placements'])
    review = json.loads((PACKAGE / 'retention-review.json').read_text())
    if total != review['placement_count']:
        raise SystemExit(f'PLACEMENT COUNT MISMATCH: package says {review["placement_count"]}, manifest has {total}')

    for name, expected_record in proof['members']:
        actual = next((item for item in placements[name]['placements'] if item['index'] == expected_record['index']), None)
        if actual != expected_record:
            raise SystemExit(f'PLACEMENT MISMATCH: {name} index {expected_record["index"]}')
    print(f'OK {len(proof["members"])} exact placed records; Object37 total={total}')

    for name, expected in proof['sources'].items():
        for relative_path, file_hash in expected.items():
            check(ROOT / relative_path, file_hash, f'source {relative_path}')

    for name, expected in proof['blend_inputs'].items():
        check(BLENDS[name], expected, f'{name} packed blend')

    for name, expected in proof['images'].items():
        check(ASSEMBLY / name, expected, f'context image {name}')

    provenance = json.loads((PACKAGE / 'Object37' / 'provenance.json').read_text())
    for relative_path, file_hash in provenance['dependencies'].items():
        check(ROOT / relative_path, file_hash, f'Object37 dependency {relative_path}')
    for key in ('evidence', 'supplemental_evidence'):
        for relative_path, file_hash in provenance.get(key, {}).items():
            check(PACKAGE / 'Object37' / relative_path, file_hash, f'Object37 {relative_path}')

    print('PASS Object37 baseline-retention evidence is internally consistent.')


if __name__ == '__main__':
    main()
