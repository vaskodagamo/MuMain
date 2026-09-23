"""Check protected well roof contact triangles against the untouched baseline."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from audit_composite import read, face_error

CONTACT_FACE_INDICES = (54, 55, 56, 57)
POSITION_UV_TOLERANCE = .0003


def check():
    baseline = read('Well02', 'baseline')
    results = {}
    for name in ('Well02', 'Well01'):
        candidate = read(name)
        matches = []
        for index in CONTACT_FACE_INDICES:
            error, match = min((face_error(baseline[index], face), i)
                               for i, face in enumerate(candidate))
            assert error < POSITION_UV_TOLERANCE, (name, index, error)
            matches.append(dict(baseline_triangle=index, candidate_triangle=match,
                                maximum_local_position_or_uv_error=error))
        results[name] = matches
    report = dict(status='PASS', protected_roof_underside=results,
                  tolerance=POSITION_UV_TOLERANCE,
                  criterion='Both full roof underside planes retain baseline position, named bone, UV, material and cyclic winding; these are the original structural support contacts.')
    (ROOT / 'roof-contact-proof.json').write_text(json.dumps(report, indent=2))


if __name__ == '__main__':
    check()
