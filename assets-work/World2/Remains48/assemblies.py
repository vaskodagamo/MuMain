"""Compare the actual exported study at recorded Dungeon bone-scatter placements."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode = True
import bpy
from mathutils import Vector, Euler, Matrix
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds, set_camera, set_lighting, render
MEMBERS = [('Object47',133),('Object48',126),('Object48',125),('Object48',135),('Object47',124),('Object48',134)]


def append_placed(name, record, anchor, stage):
    path = ROOT/'Object48'/('baseline/source.blend' if stage=='current' else 'validation/reimported.blend')
    if name != 'Object48':
        path = REPO/'assets-work/World2/Readiness02'/name/'baseline.blend'
    with bpy.data.libraries.load(str(path)) as (available, loaded):
        loaded.objects = available.objects
    objects = []
    for obj in loaded.objects:
        if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type == 'ARMATURE':
            rotation = Euler([math.radians(v) for v in record['rotation']], 'XYZ')
            obj.matrix_world = Matrix.Translation(Vector(record['position'])-anchor) @ rotation.to_matrix().to_4x4() @ Matrix.Scale(record['scale'],4)
        else:
            objects.append(obj)
    return objects


def views(folder, stage, bounds):
    set_camera(bounds,1.65)
    set_lighting()
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = 1100,900
    scene.cycles.samples = 16
    render(folder/(stage+'.png'))
    scene.render.resolution_percentage = 30
    render(folder/(stage+'-small.png'))
    scene.render.resolution_percentage = 100
    center = (Vector(bounds[0])+Vector(bounds[1]))/2
    scene.camera.location = center+Vector((-1.6,1.8,1.2))*max(Vector(bounds[1])-Vector(bounds[0]))
    scene.camera.rotation_euler = (center-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    render(folder/(stage+'-reverse.png'))


def group(name, records):
    folder = ROOT/'review-assemblies'/name
    folder.mkdir(parents=True,exist_ok=True)
    anchor = Vector(records[0][1]['position'])
    bounds_by_stage = {}
    for stage in ('current','candidate'):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.scene.world = bpy.data.worlds.new('RemainsContextWorld')
        meshes = [obj for asset,record in records for obj in append_placed(asset,record,anchor,stage)]
        bpy.context.scene.frame_set(0)
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        bounds_by_stage[stage] = mesh_bounds([obj.evaluated_get(graph) for obj in meshes])
        views(folder,stage,bounds_by_stage['current'])
    error = max(abs(a-b) for old,new in zip(bounds_by_stage['current'],bounds_by_stage['candidate']) for a,b in zip(old,new))
    assert error < .001, error
    (folder/'evidence.json').write_text(json.dumps(dict(records=records,anchor=list(anchor),bounds=bounds_by_stage,maximum_bound_difference=error,kind='OFFLINE_DIFFUSE_ACTUAL_PLACEMENTS'),indent=2))


def main():
    models = json.loads((REPO/'assets-work/Environment/coordination/dungeon-readiness.json').read_text())['models']
    records = [(name,next(p for p in models[name]['placements'] if p['index']==index)) for name,index in MEMBERS]
    group('remains-cluster',records)
    placements = models['Object48']['placements']
    # Review every distinct nonzero pitch family once, at exact local transforms.
    selected = []
    seen = set()
    for p in placements:
        pitch = round((p['rotation'][0]+180)%360-180,3)
        if pitch and pitch not in seen:
            seen.add(pitch)
            selected.append(p)
    for record in selected:
        group('pitched-'+str(record['index']),[('Object48',record)])


if __name__=='__main__':
    main()
