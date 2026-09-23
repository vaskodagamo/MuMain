"""Read-only verifier: retained bytes, all manifests and baseline-only scope."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest=json.loads((ROOT/'manifest.json').read_text())
    for path,expected in manifest['files'].items():
        assert digest(ROOT/path)==expected,path
    count=0
    for name in ('Object42','Object43'):
        folder=ROOT/name
        provenance=json.loads((folder/'provenance.json').read_text())
        for path,expected in provenance['dependencies'].items():assert digest(REPO/path)==expected,path
        contract=json.loads((folder/'validation/contracts.json').read_text())
        assert contract['status']=='BASELINE_RETAINED_BYTE_EXACT'
        assert contract['control_is_not_an_install_candidate']
        for stage in ('original','baseline'):
            assert digest(folder/stage/f'{name}.bmd')==contract['source_sha256']
        count+=len(contract['placements'])
    assert count==120
    sources=json.loads((ROOT/'context/sources.json').read_text())
    for model,paths in sources.items():
        for path,expected in paths.items():assert digest(REPO/path)==expected,(model,path)
    flame=json.loads((ROOT/'flame-contract.json').read_text())
    for path,expected in flame['source_hashes'].items():assert digest(REPO/path)==expected,path
    maprecord=flame['placement_file']
    data=subprocess.check_output(['git','show',maprecord['revision']+':'+maprecord['path']],cwd=REPO)
    assert hashlib.sha256(data).hexdigest()==maprecord['sha256']
    print(f'PASS: {len(manifest["files"])} evidence hashes; all120 placements; retained BMDs/textures and source contracts unchanged; roundtrips are controls only.')


if __name__=='__main__':main()
