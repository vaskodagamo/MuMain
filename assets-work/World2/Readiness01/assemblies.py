"""Read-only actual placement context for bridge supports and open trim."""
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

GROUPS = {
    'pier-support': [('Object04', 9), ('Object06', 14)],
    'stepped-supports': [('Object13', 22), ('Object13', 25)],
    'open-trim-run': [('Object15', 65), ('Object15', 66), ('Object15', 360)],
}


def append_placed(name, record, anchor):
    source = ROOT / name / 'baseline.blend'
    with bpy.data.libraries.load(str(source)) as (_, loaded):
        loaded.objects = _.objects
    meshes = []
    for obj in loaded.objects:
        if obj.type not in ('MESH', 'ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type == 'ARMATURE':
            rotation = Euler([math.radians(a) for a in record['rotation']])
            obj.matrix_world = (Matrix.Translation(Vector(record['position']) - anchor)
                                @ rotation.to_matrix().to_4x4()
                                @ Matrix.Scale(record['scale'], 4))
        else:
            meshes.append(obj)
    return meshes


def render_group(label, members, models):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.world = bpy.data.worlds.new("BaselineWorld")
    records = [(name, next(p for p in models[name]['placements'] if p['index'] == index))
               for name, index in members]
    anchor = Vector(records[0][1]['position'])
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
    output = ROOT / 'assemblies' / label
    output.mkdir(parents=True, exist_ok=True)
    render(output / 'baseline.png')
    scene.render.resolution_percentage = 30
    render(output / 'baseline-small.png')
    scene.render.resolution_percentage = 100
    center = (Vector(bounds[0]) + Vector(bounds[1])) / 2
    span = max(Vector(bounds[1]) - Vector(bounds[0]))
    scene.camera.location = center + Vector((-1.6, 1.8, 1.2)) * span
    scene.camera.rotation_euler = (center - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
    render(output / 'baseline-reverse.png')
    evidence = {'kind': 'OFFLINE_BASELINE_ACTUAL_PLACEMENTS', 'records': records,
                'bounds': bounds, 'anchor': list(anchor),
                'sources': {name: json.loads((ROOT / name / 'provenance.json').read_text())
                            for name, _ in records},
                'images': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in output.glob('*.png')}}
    (output / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


models = json.loads((REPO / 'assets-work/Environment/coordination/dungeon-readiness.json').read_text())['models']
for label, members in GROUPS.items():
    render_group(label, members, models)
