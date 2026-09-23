"""Build an offline view of Object37 in its real support-corridor placements."""

from pathlib import Path
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
BLENDER_SCENE_HELPERS = REPO / 'assets-work/World1/StaticBatch01'
sys.path.insert(0, str(BLENDER_SCENE_HELPERS))
from review_scene import mesh_bounds, render, set_camera, set_lighting

CONVERTER = Path(os.environ.get('MU_BMDCONV', ''))
if not CONVERTER.is_file():
    CONVERTER = REPO / 'out/build/macos-arm64/tools/bmdconv/Release/bmdconv'
if not CONVERTER.is_file():
    CONVERTER = Path(shutil.which('bmdconv') or '')
PLACEMENT_FILE = REPO / 'assets-work/Environment/coordination/dungeon-readiness.json'
MEMBERS = [
    ('Object37', 310), ('Object37', 311), ('Object37', 312),
    ('Object37', 313), ('Object37', 567), ('Object37', 568),
    ('Object01', 317), ('Object01', 318), ('Object01', 319),
    ('Object04', 320), ('Object04', 604), ('Object08', 307),
]
BLEND_PATHS = {
    'Object37': ROOT / 'Object37' / 'baseline.blend',
    'Object01': ROOT / 'Object01-current' / 'baseline.blend',
    'Object04': ROOT / 'Object04-current' / 'baseline.blend',
    'Object08': ROOT / 'Object08-current' / 'baseline.blend',
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def get_records():
    models = json.loads(PLACEMENT_FILE.read_text())['models']
    return [(name, next(p for p in models[name]['placements'] if p['index'] == index))
            for name, index in MEMBERS]


def place_model(name, record, anchor):
    with bpy.data.libraries.load(str(BLEND_PATHS[name])) as (source, loaded):
        loaded.objects = source.objects
    rotation = Euler([math.radians(v) for v in record['rotation']])
    matrix = (Matrix.Translation(Vector(record['position']) - anchor)
              @ rotation.to_matrix().to_4x4()
              @ Matrix.Scale(record['scale'], 4))
    meshes = []
    for obj in loaded.objects:
        if obj.type not in ('MESH', 'ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type == 'ARMATURE':
            obj.matrix_world = matrix
        else:
            meshes.append(obj)
    return meshes


def source_record(name):
    if not CONVERTER.is_file():
        raise FileNotFoundError('Set MU_BMDCONV to the built bmdconv executable')
    bmd = REPO / 'src/bin/Data/Object2' / f'{name}.bmd'
    info = subprocess.check_output([str(CONVERTER), 'info', str(bmd)]).decode('utf-8', 'replace')
    files = {str(bmd.relative_to(REPO)): sha256(bmd)}
    for texture in re.findall(r'texture=(\S+)', info):
        image = Path(texture)
        extension = {'.jpg': '.OZJ', '.tga': '.OZT'}[image.suffix.lower()]
        path = bmd.parent / image.with_suffix(extension)
        files[str(path.relative_to(REPO))] = sha256(path)
    return files


def main():
    # This render-only file loads packed imports, not BMDs; Source Tools callbacks
    # are irrelevant and otherwise reference scene properties cleared by reset.
    for handler_list in (bpy.app.handlers.load_post,
                         bpy.app.handlers.depsgraph_update_post):
        for handler in tuple(handler_list):
            if 'valvesource' in getattr(handler, '__module__', '').lower():
                handler_list.remove(handler)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    records = get_records()
    anchor = Vector((1250.0, 24750.0, 169.5))
    meshes = [obj for name, record in records for obj in place_model(name, record, anchor)]
    scene = bpy.context.scene
    scene.frame_set(0)
    scene.world = bpy.data.worlds.new('Object37CorridorReviewWorld')
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    bounds = mesh_bounds([obj.evaluated_get(graph) for obj in meshes])
    camera = set_camera(bounds, 1.35)
    set_lighting()
    scene.cycles.samples = 24
    folder = ROOT / 'assemblies' / 'object37-corridor'
    folder.mkdir(parents=True, exist_ok=True)
    render(folder / 'baseline.png')
    scene.render.resolution_percentage = 35
    render(folder / 'baseline-small.png')
    scene.render.resolution_percentage = 100
    center = (Vector(bounds[0]) + Vector(bounds[1])) / 2
    span = max(Vector(bounds[1]) - Vector(bounds[0]))
    camera.location = center - Vector((1.35, -2.0, 1.3)) * span
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    render(folder / 'baseline-reverse.png')

    models = sorted({name for name, _ in records})
    proof = {
        'kind': 'OFFLINE_ACTUAL_PLACEMENT_STATIC_ASSEMBLY',
        'purpose': 'Read-only Object37 placement context; no terrain, lighting bake, or client.',
        'placement_source': str(PLACEMENT_FILE.relative_to(REPO)),
        'placement_source_sha256': sha256(PLACEMENT_FILE),
        'members': records,
        'sources': {name: source_record(name) for name in models},
        'anchor': list(anchor),
        'bounds': bounds,
        'blend_inputs': {name: sha256(path) for name, path in BLEND_PATHS.items()},
        'images': {p.name: sha256(p) for p in sorted(folder.glob('baseline*.png'))},
    }
    (folder / 'evidence.json').write_text(json.dumps(proof, indent=2) + '\n')


if __name__ == '__main__':
    main()
