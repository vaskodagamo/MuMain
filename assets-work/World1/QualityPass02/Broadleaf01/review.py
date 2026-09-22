"""Compare current merged BMDs against official candidate reimports, identically lit."""
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_source import check

ROOT = Path(__file__).resolve().parent
WORLD = ROOT.parents[1]
sys.path.insert(0, str(WORLD / 'StaticBatch01'))
from review_scene import mesh_bounds, set_camera, set_lighting, render
spec = importlib.util.spec_from_file_location('audit', WORLD / 'coordination/audit_authored_vertices.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
NAMES = ('Grass05', 'Grass06')


def open_scene(path, bounds):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(0)
    set_camera(bounds)
    set_lighting()
    bpy.context.scene.cycles.samples = 16


def review_asset(name):
    folder = ROOT / name
    check(folder)
    bpy.ops.wm.open_mainfile(filepath=str(folder / 'validation/reimported.blend'))
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or obj.get('mu_helper'): continue
        for face in obj.data.polygons:
            assert face.area > 1e-8
            assert all(face.normal.dot(obj.data.corner_normals[i].vector) > 0 for i in face.loop_indices), (name, face.index)
            uv = [obj.data.uv_layers[0].data[i].uv for i in face.loop_indices]
            assert abs((uv[1]-uv[0]).cross(uv[2]-uv[0])) > 1e-9
            assert all(-.0001 <= x <= 1.0001 for v in uv for x in v)
    authored = audit.points(folder / 'source.blend')
    exported = audit.points(folder / 'validation/reimported.blend')
    deviations = [audit.distances(authored, exported), audit.distances(exported, authored)]
    assert max(deviations) < .001, deviations
    (folder / 'validation/authored-export.json').write_text(json.dumps(dict(
        result='PASS', max_distances=deviations, space='bone-matched bind/world coordinates'), indent=2))
    bounds = json.loads((folder / 'validation/source.json').read_text())['bounds_before']
    stages = {'original': folder / 'original/source.blend', 'current': folder / 'baseline/source.blend',
              'candidate': folder / 'validation/reimported.blend'}
    for stage, path in stages.items():
        open_scene(path, bounds)
        render(folder / 'review' / (stage + '.png'))
        camera = bpy.context.scene.camera
        old = camera.location.copy()
        center = (Vector(bounds[0])+Vector(bounds[1]))/2
        camera.location = center + Vector((-1.35, 2, 1.3))*max(Vector(bounds[1])-Vector(bounds[0]))
        camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
        render(folder / 'review' / (stage + '-reverse.png'))
        camera.location = old
        camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.render.resolution_percentage = 25
        render(folder / 'review' / (stage + '-small.png'))
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or obj.get('mu_helper'):
            continue
        wire = obj.modifiers.new('ReviewWire', 'WIREFRAME')
        wire.thickness = .20
    bpy.context.scene.render.resolution_percentage = 100
    render(folder / 'review/wireframe.png')


if __name__ == '__main__':
    for name in NAMES:
        review_asset(name)
