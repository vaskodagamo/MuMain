"""Refresh provenance, immutable original sources and all retained study evidence."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
NAMES=('Object06','Object13','Object15')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def context_inputs():
    report={}
    for path in sorted((ROOT/'previous-context').glob('*/evidence.json')):
        changes=[]
        original=json.loads(path.read_text())
        for name,record in original['sources'].items():
            for relative,expected in record['dependencies'].items():
                current=digest(REPO/relative)
                changes.append(dict(path=relative,previous=expected,current=current,changed=current!=expected))
        report[path.parent.name]=dict(dependencies=changes,refreshed_current_imports=True,
                                    placements_sha256=digest(ROOT/'context'/path.parent.name/'placements.json'))
    return report


def main():
    for name in NAMES:
        shutil.copy2(ROOT/name/'baseline/source.blend',ROOT/name/'original/source.blend')
    (ROOT/'context-input-audit.json').write_text(json.dumps(context_inputs(),indent=2)+'\n')
    files={str(path.relative_to(ROOT)):digest(path) for path in sorted(ROOT.rglob('*'))
           if path.is_file() and path.name!='provenance.json' and not path.name.endswith(('.blend1','.blend2','.pyc')) and '__pycache__' not in str(path)}
    manifest=dict(base_commit=(ROOT/'baseline-commit.txt').read_text().strip(),
                  status='ARTIST_REJECTED_CANDIDATES_BASELINE_RECOMMENDED_COORDINATOR_REVIEW_PENDING',
                  installed=False,source_and_evidence=files,
                  candidate_hashes={name:digest(ROOT/name/'exports'/(name+'.bmd')) for name in NAMES})
    (ROOT/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__=='__main__':
    main()
