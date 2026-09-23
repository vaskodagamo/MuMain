"""Verify retained rock baseline dependencies and every supplied evidence hash."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
NAMES = ('Object49', 'Object50')
BASE_COMMIT = 'c40d0b0d313b44b99a4fd84bcac0a81e476f3ed7'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(path, expected, checks):
    actual = digest(path)
    assert actual == expected, str(path)
    checks.append(dict(path=str(path.relative_to(REPO)), sha256=actual))


def verify_asset(name, checks):
    folder = ROOT / 'baseline' / name
    provenance = json.loads((folder / 'provenance.json').read_text())
    check(REPO / provenance['source'], provenance['sha256'], checks)
    check(folder / (name + '.bmd'), provenance['sha256'], checks)
    for path, expected in provenance['dependencies'].items():
        check(REPO / path, expected, checks)
        if path.endswith('.OZJ'):
            check(ROOT / 'baseline' / Path(path).name, expected, checks)
        original = subprocess.run(['git', 'show', f'{BASE_COMMIT}:{path}'], cwd=REPO,
                                  capture_output=True, check=True).stdout
        assert original == (REPO / path).read_bytes(), path
    for key in ('evidence', 'supplemental_evidence'):
        for path, expected in provenance[key].items():
            check(folder / path, expected, checks)
    return provenance


def main():
    checks = []
    provenances = {name:verify_asset(name, checks) for name in NAMES}
    folder = ROOT / 'baseline' / 'rock-cluster'
    context = json.loads((folder / 'evidence.json').read_text())
    for name, record in context['sources'].items():
        assert record == provenances[name], name
    for image, expected in context['images'].items():
        check(folder / image, expected, checks)
    models = json.loads((ROOT / 'placements.json').read_text())['models']
    for name, record in context['members']:
        assert record in models[name]['placements'], record
    stats = {name:dict(count=len(data['placements']),
                      scale_range=[min(p['scale'] for p in data['placements']),
                                   max(p['scale'] for p in data['placements'])])
             for name, data in models.items()}
    report = dict(status='PASS_BASELINE_RETAINED', baseline_commit=BASE_COMMIT,
                  checks=checks, context_source_records_exact=True,
                  actual_cluster_members=len(context['members']), placements=stats,
                  game_changes=False, candidate_created=False)
    (ROOT / 'validation' / 'retention-hashes.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(status=report['status'], checks=len(checks), placements=stats)))


if __name__ == '__main__':
    main()
