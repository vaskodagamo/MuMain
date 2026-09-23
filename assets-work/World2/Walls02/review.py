"""Matching diffuse-only actual-export views and the recorded three-module run."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode = True
import bpy
from mathutils import Vector, Matrix, Euler
import math
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0, str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import CAMERA_DIRECTION, mesh_bounds, render, set_camera, set_lighting
FOLDER = ROOT/'Object51'


def views(folder, stage, objects, factor):
    scene = bpy.context.scene
    scene.frame_set(0)
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    bounds = mesh_bounds([obj.evaluated_get(graph) for obj in objects])
    if scene.world is None:
        scene.world = bpy.data.worlds.new('OfflineWorld')
    camera = set_camera(bounds, factor)
    set_lighting()
    folder.mkdir(parents=True, exist_ok=True)
    render(folder/f'{stage}.png')
    scene.render.resolution_percentage = 35
    render(folder/f'{stage}-small.png')
    scene.render.resolution_percentage = 100
    center = (Vector(bounds[0])+Vector(bounds[1]))/2
    span = max(Vector(bounds[1])-Vector(bounds[0]))
    camera.location = center-CAMERA_DIRECTION*span
    camera.rotation_euler = (center-camera.location).to_track_quat('-Z','Y').to_euler()
    render(folder/f'{stage}-reverse.png')
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/f'{stage}-context.blend'))
    return bounds


def main():
    records = json.loads((ROOT/'context/wall51-run/evidence.json').read_text())['members']
    for stage, source in [('current',FOLDER/'baseline/source.blend'),
                          ('candidate',FOLDER/'validation/reimported.blend')]:
        bpy.ops.wm.open_mainfile(filepath=str(source))
        objects = [o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper')]
        views(FOLDER/'review',stage,objects,1.4)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        anchor = Vector(records[0][1]['position'])
        objects = []
        for _, record in records:
            with bpy.data.libraries.load(str(source)) as (available,loaded):
                loaded.objects = available.objects
            matrix = (Matrix.Translation(Vector(record['position'])-anchor)
                      @ Euler([math.radians(v) for v in record['rotation']]).to_matrix().to_4x4()
                      @ Matrix.Scale(record['scale'],4))
            for obj in loaded.objects:
                if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper'):continue
                bpy.context.scene.collection.objects.link(obj)
                if obj.type=='ARMATURE':obj.matrix_world=matrix
                else:objects.append(obj)
        bounds=views(ROOT/'context/candidate-comparison',stage,objects,1.8)
        (ROOT/f'context/candidate-comparison/{stage}-placements.json').write_text(json.dumps(dict(records=records,bounds=bounds),indent=2)+'\n')


if __name__=='__main__':main()
