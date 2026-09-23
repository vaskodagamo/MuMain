"""Show true neighboring records; no terrain proxy or simulated runtime particles."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector,Matrix,Euler
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[2]/'assets-work/World1/StaticBatch01'))
from review_scene import CAMERA_DIRECTION,mesh_bounds,render,set_camera,set_lighting


def load_instance(name,record,anchor):
    path=ROOT/'context/imports'/name/'source.blend'
    with bpy.data.libraries.load(str(path)) as (source,loaded):loaded.objects=source.objects
    matrix=(Matrix.Translation(Vector(record['position'])-anchor)
            @ Euler([math.radians(v) for v in record['rotation']]).to_matrix().to_4x4()
            @ Matrix.Scale(record['scale'],4))
    meshes=[]
    for obj in loaded.objects:
        if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper'):continue
        bpy.context.scene.collection.objects.link(obj)
        obj['placement_index']=record['index']
        if obj.type=='ARMATURE':obj.matrix_world=matrix
        else:meshes.append(obj)
    return meshes


def group(name,records):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene
    scene.world=bpy.data.worlds.new('OfflineNeutralWorld')
    anchor=Vector(records[0][1]['position'])
    objects=[]
    for asset,record in records:objects.extend(load_instance(asset,record,anchor))
    scene.frame_set(0)
    bpy.context.view_layer.update()
    graph=bpy.context.evaluated_depsgraph_get()
    bounds=mesh_bounds([obj.evaluated_get(graph) for obj in objects])
    camera=set_camera(bounds,1.35)
    set_lighting()
    scene.render.film_transparent=False
    folder=ROOT/'context'/name
    folder.mkdir(exist_ok=True)
    render(folder/'baseline.png')
    scene.render.resolution_percentage=50
    render(folder/'baseline-small.png')
    scene.render.resolution_percentage=100
    center=(Vector(bounds[0])+Vector(bounds[1]))/2
    span=max(Vector(bounds[1])-Vector(bounds[0]))
    camera.location=center-CAMERA_DIRECTION*span
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    render(folder/'baseline-reverse.png')
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'context.blend'))
    (folder/'proof.json').write_text(json.dumps(dict(records=records,relative_origin=list(anchor),bounds=bounds,projection='Orthographic',limitations='Offline diffuse static shell only. No terrain, particles, runtime jitter, terrain lighting or client verification.'),indent=2)+'\n')


for name,records in json.loads((ROOT/'context/placements.json').read_text()).items():
    if not (ROOT/'context'/name/'proof.json').exists():group(name,records)
