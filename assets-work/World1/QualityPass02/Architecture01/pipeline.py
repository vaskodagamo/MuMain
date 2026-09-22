"""Reproduce architecture through official import/export, validation and render tools."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
from prepare import ROOT, REPO, NAMES, CONVERTER, run_blender, prepare


def load_module(name, path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def export_assets():
    for name in NAMES:
        folder=ROOT/name
        run_blender([str(folder/'source.blend'),'--python',str(REPO/'tools/blender/mu_bmd_export.py'),'--','--out',str(folder/'exports'/f'{name}.bmd'),'--bmdconv',CONVERTER],folder/'validation/export.txt')
        run_blender(['--python',str(REPO/'tools/blender/mu_bmd_import.py'),'--','--bmd',str(folder/'exports'/f'{name}.bmd'),'--out',str(folder/'validation/reimported.blend'),'--bmdconv',CONVERTER],folder/'validation/reimport.txt')


def validate():
    module=load_module('static_validation',REPO/'assets-work/World1/StaticBatch01/validate_export.py')
    module.EXPECTED_KEYS={name:1 for name in NAMES}
    binding=load_module('raw_binding',REPO/'assets-work/World1/Cannons01/raw_bindings.py')
    import hashlib
    import re
    for name in NAMES:
        folder=ROOT/name
        # Validation helper's original path must mean current merged baseline here.
        old_folder=folder/'validation/original'
        old_folder.mkdir(exist_ok=True)
        for source in (folder/'baseline/smd').iterdir():
            shutil.copy2(source,old_folder/source.name)
        new,metadata,messages=module.extract(folder,'new')
        old_info=(folder/'baseline/info.txt').read_text()
        info=module.run('info',folder/'exports'/f'{name}.bmd').stdout
        (folder/'validation/info-after.txt').write_text(info)
        assert re.findall(r'texture=(.*)',old_info)==re.findall(r'texture=(.*)',info)
        before,after=module.engine_bounds(old_info),module.engine_bounds(info)
        assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb)) <= .02
        module.check_local_motion(old_folder,new,name,folder/'validation')
        compare=module.run('compare',folder/'baseline'/f'{name}.bmd',folder/'exports'/f'{name}.bmd',check=False)
        (folder/'validation/compare.txt').write_text(compare.stdout)
        (folder/'validation/smd-validation.txt').write_text('\n'.join(messages))
        old_meta=module.manifest_meta((old_folder/f'{name}.actions.txt').read_text())
        assert old_meta==metadata
        # Compare only actual skeleton/actions, retaining empty geometry on both sides.
        header=(old_folder/f'{name}.smd').read_text().split('triangles\n')[0]
        (old_folder/'skeleton.smd').write_text(header+'triangles\nend\n')
        module.run('smd2bmd',old_folder/'skeleton.smd',old_folder/'skeleton.bmd','--manifest',old_folder/f'{name}.actions.txt')
        skeleton=module.run('compare',old_folder/'skeleton.bmd',new/'skeleton.bmd').stdout
        assert 'EQUIVALENT' in skeleton
        (folder/'validation/skeleton-compare.txt').write_text(skeleton)
        audit=binding.audit(folder/'exports'/f'{name}.bmd')
        assert all(not m['vertex_to_normal_node_mismatch_corner_counts'] for m in audit['mesh_bindings'])
        (folder/'validation/raw-normal-bindings.json').write_text(json.dumps(audit,indent=2)+'\n')
        textures=json.loads((folder/'validation/frozen-textures.json').read_text())
        assert all(hashlib.sha256((REPO/path).read_bytes()).hexdigest()==value for path,value in textures.items())
        check=subprocess.run([sys.executable,str(REPO/'tools/mu_texture.py'),'check',*map(str,(folder/'exports').glob('*.OZJ'))],capture_output=True,text=True,check=True)
        (folder/'validation/texture-check.txt').write_text(check.stdout)
        (folder/'validation/summary.json').write_text(json.dumps(dict(status='PASS',bounds_before=before,bounds_after=after,skeleton_actions='EQUIVALENT',full_comparison='DIFFERENT',sha256=hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest(),textures_frozen=True,client_verified=False),indent=2)+'\n')
        print(name,'converter, skeleton, raw normals, frozen textures PASS')


if __name__=='__main__':
    command=sys.argv[1]
    if command=='prepare':
        for name in NAMES: prepare(name)
    elif command=='build':
        run_blender(['--python',str(ROOT/'build_source.py')],ROOT/'build.log')
    elif command=='export': export_assets()
    elif command=='validate': validate()
    elif command=='review': run_blender(['--python',str(ROOT/'review.py')],ROOT/'review.log')
