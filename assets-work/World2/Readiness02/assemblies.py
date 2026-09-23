"""Read-only bone-remains gathering at exact Dungeon placements."""
from pathlib import Path
import hashlib
import json
import math
import sys
import bpy
from mathutils import Vector, Euler, Matrix

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0, str(REPO / 'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds, set_camera, set_lighting, render

MEMBERS = [('Object47', 133), ('Object48', 126), ('Object48', 125),
           ('Object48', 135), ('Object47', 124), ('Object48', 134)]


def append_placed(name, record, anchor):
    with bpy.data.libraries.load(str(ROOT / name / 'baseline.blend')) as (_, loaded):
        loaded.objects = _.objects
    meshes = []
    for obj in loaded.objects:
        if obj.type not in ('MESH', 'ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type == 'ARMATURE':
            rotation = Euler([math.radians(value) for value in record['rotation']])
            obj.matrix_world = (Matrix.Translation(Vector(record['position']) - anchor)
                                @ rotation.to_matrix().to_4x4()
                                @ Matrix.Scale(record['scale'], 4))
        else:
            meshes.append(obj)
    return meshes


def main():
    data = json.loads((REPO / 'assets-work/Environment/coordination/dungeon-readiness.json').read_text())['models']
    records = [(name, next(item for item in data[name]['placements'] if item['index'] == index))
               for name, index in MEMBERS]
    anchor = Vector(records[0][1]['position'])
    bpy.context.scene.world = bpy.data.worlds.new('DungeonRemainsWorld')
    objects = [obj for name, record in records for obj in append_placed(name, record, anchor)]
    bpy.context.scene.frame_set(0)
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    bounds = mesh_bounds([obj.evaluated_get(graph) for obj in objects])
    set_camera(bounds, 1.65)
    set_lighting()
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = 1100, 900
    scene.cycles.samples = 16
    folder = ROOT / 'assemblies' / 'remains-cluster'
    folder.mkdir(parents=True, exist_ok=True)
    render(folder / 'baseline.png')
    scene.render.resolution_percentage = 30
    render(folder / 'baseline-small.png')
    scene.render.resolution_percentage = 100
    center = (Vector(bounds[0]) + Vector(bounds[1])) / 2
    span = max(Vector(bounds[1]) - Vector(bounds[0]))
    scene.camera.location = center + Vector((-1.6, 1.8, 1.2)) * span
    scene.camera.rotation_euler = (center - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
    render(folder / 'baseline-reverse.png')
    sources = {name: json.loads((ROOT / name / 'provenance.json').read_text())
               for name, _ in records}
    images = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
              for path in folder.glob('*.png')}
    proof = {'kind': 'OFFLINE_BASELINE_ACTUAL_PLACEMENTS', 'records': records,
             'sources': sources, 'bounds': bounds, 'anchor': list(anchor), 'images': images}
    (folder / 'evidence.json').write_text(json.dumps(proof, indent=2) + '\n')


main()
