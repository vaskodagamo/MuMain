"""All40 hierarchical motion poses, frozen meshes and normal ownership for House04."""
import importlib.util
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]


def helper(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module

matrix=helper('house_matrix_audit',REPO/'assets-work/World1/Architecture03/validate_matrices.py')
raw=helper('house_raw_audit',REPO/'assets-work/World1/Cannons01/raw_bindings.py')


def normal_motion(folder):
    path=folder/'validation/new/House04.smd'
    parents,bind=matrix.parse_smd(path)
    _,frames=matrix.parse_smd(path.with_name('House04_a00.smd'))
    poses=[('bind',bind[0]),*[(str(frame),pose) for frame,pose in frames.items()]]
    report=[]
    meshes=raw.meshes(folder/'exports/House04.bmd')
    for label,local in poses:
        world=matrix.world_matrices(parents,local)
        maximum=0;shared=0
        for mesh in meshes:
            for vertices,normals in mesh['triangles']:
                for vertex,normal in zip(vertices,normals):
                    bone=mesh['vertices'][vertex][0];record=mesh['normals'][normal]
                    if bone==record[0]:continue
                    shared+=1
                    actual=world[record[0]].to_3x3()@Vector(record[1:4])
                    intended=world[bone].to_3x3()@Vector(record[1:4])
                    maximum=max(maximum,(actual-intended).length)
        assert maximum<.0001,(label,maximum)
        report.append(dict(pose=label,shared_normal_corners=shared,maximum_world_direction_delta=maximum))
    (folder/'validation/raw-normal-bindings.json').write_text(json.dumps(dict(status='PASS',poses=report,threshold=.0001,method='Actual raw normal nodes composed through full hierarchy at bind and all40 action frames'),indent=2)+'\n')


if __name__=='__main__':
    folder=ROOT/'House04'
    matrix.compare_asset(folder)
    normal_motion(folder)
    print('House04 all40 hierarchical poses, bounds and raw normal directions PASS')
