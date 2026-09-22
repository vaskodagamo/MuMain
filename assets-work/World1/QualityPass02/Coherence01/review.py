"""Render hash-verified integrated neighborhoods at unchanged placement transforms.

Run Blender --background --factory-startup --python-exit-code 1 --python this_file.
These diffuse bind-pose comparisons omit terrain and engine effect animation.
"""
import hashlib
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = Path(__file__).resolve().parent
WORLD = ROOT.parents[1]
REPO = WORLD.parents[1]
sys.path.insert(0, str(WORLD / 'StaticBatch01'))
from review_scene import mesh_bounds, render, set_camera, set_lighting

RADIUS = 650
REGIONS = {'tavern': (12738, 12838), 'well': (14700, 11700),
           'homes': (14700, 14700), 'awning': (11750, 14500),
           'dome': (11550, 11300)}
RESOLUTION = (1600, 1200)
LEDGER = json.loads((REPO / 'assets-work/Environment/coordination/accepted-exports.json').read_text())
MODELS = json.loads((WORLD / 'coordination/dependency-map.json').read_text())['models']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def references(name):
    inspection = WORLD / 'coordination/final-inspection' / name
    provenance = json.loads((inspection / 'provenance.json').read_text())
    game = f'src/bin/Data/Object1/{name}.bmd'
    for relative, expected in provenance['game_files'].items():
        if relative != game or game not in LEDGER:
            assert digest(REPO / relative) == expected, relative
    old = inspection / 'import.blend'
    new = old
    if game in LEDGER:
        export = REPO / LEDGER[game]
        assert digest(REPO / game) == digest(export), game
        new = export.parent.parent / 'validation/reimported.blend'
    assert old.exists() and new.exists(), name
    return {'baseline_blend': str(old.relative_to(REPO)),
            'integrated_blend': str(new.relative_to(REPO)),
            'baseline_blend_sha256': digest(old),
            'integrated_blend_sha256': digest(new),
            'baseline_game_files': provenance['game_files'],
            'integrated_bmd_sha256': digest(REPO / game)}


def records_for(anchor):
    return [dict(name=name, index=index, **placement)
            for name, model in MODELS.items() if not model.get('scope_exclusion')
            for index, placement in enumerate(model['placements'])
            if math.dist(placement['position'][:2], anchor) < RADIUS]


def append_placed(record, source, anchor):
    with bpy.data.libraries.load(str(source)) as (available, loaded):
        loaded.objects = available.objects
    meshes = []
    for obj in loaded.objects:
        if obj.type not in ('ARMATURE', 'MESH') or obj.get('mu_helper'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type == 'MESH':
            meshes.append(obj)
        else:
            rotation = Euler([math.radians(v) for v in record['rotation']], 'XYZ')
            transform = Matrix.Translation(Vector(record['position']) - anchor)
            obj.matrix_world = transform @ rotation.to_matrix().to_4x4() @ Matrix.Scale(record['scale'], 4)
    return meshes


def verify_images():
    for image in bpy.data.images:
        if image.type == 'IMAGE' and image.source == 'FILE':
            assert image.size[0] > 0 and image.size[1] > 0, image.name
            assert image.packed_file or Path(bpy.path.abspath(image.filepath)).exists(), image.name


def render_stage(records, sources, anchor, stage, out, bounds=None):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.world = bpy.data.worlds.new('NeighborhoodReviewWorld')
    meshes = []
    for record in records:
        source = REPO / sources[record['name']][stage + '_blend']
        meshes.extend(append_placed(record, source, anchor))
    bpy.context.scene.frame_set(0)
    bpy.context.view_layer.update()
    verify_images()
    bounds = bounds or mesh_bounds(meshes)
    camera = set_camera(bounds, scale_factor=1.3)
    set_lighting()
    scene = bpy.context.scene
    scene.cycles.samples = 16
    scene.render.resolution_x, scene.render.resolution_y = RESOLUTION
    render(out / (stage + '.png'))
    scene.render.resolution_percentage = 40
    render(out / (stage + '-small.png'))
    center = (Vector(bounds[0]) + Vector(bounds[1])) / 2
    camera.location = center + Vector((-1.35, 2, 1.3)) * max(Vector(bounds[1]) - Vector(bounds[0]))
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.resolution_percentage = 100
    render(out / (stage + '-reverse.png'))
    return bounds


def review_region(label, anchor_xy):
    out = ROOT / 'review' / label
    out.mkdir(parents=True, exist_ok=True)
    records = records_for(anchor_xy)
    sources = {name: references(name) for name in sorted({r['name'] for r in records})}
    anchor = Vector((*anchor_xy, records[0]['position'][2]))
    bounds = render_stage(records, sources, anchor, 'baseline', out)
    render_stage(records, sources, anchor, 'integrated', out, bounds)
    evidence = {'records': records, 'sources': sources, 'camera_bounds': bounds,
                'limits': 'Diffuse bind-pose only; no terrain or engine effect simulation. Not client evidence.',
                'image_hashes': {p.name: digest(p) for p in sorted(out.glob('*.png'))}}
    (out / 'provenance.json').write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    for label, anchor in REGIONS.items():
        review_region(label, anchor)
