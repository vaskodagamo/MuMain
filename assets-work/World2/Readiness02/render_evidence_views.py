"""Render supplemental diffuse-only baseline views without changing BMDs or blends."""

from pathlib import Path
import hashlib
import json
import os
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0, str(REPO / 'assets-work/World1/StaticBatch01'))
from review_scene import CAMERA_DIRECTION, mesh_bounds, render, set_camera, set_lighting


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def render_asset(name):
    folder = ROOT / name
    if not folder.exists():
        folder = ROOT.parent / name
    blend = folder / 'baseline.blend'
    views = folder / 'individual'
    views.mkdir(exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(blend))
    scene = bpy.context.scene
    scene.frame_set(0)
    meshes = [obj for obj in scene.objects
              if obj.type == 'MESH' and not obj.get('mu_helper')]
    bounds = mesh_bounds(meshes)
    low, high = map(Vector, bounds)
    center = (low + high) / 2
    span = max(high - low)
    camera = set_camera(bounds)
    set_lighting()
    scene.cycles.samples = 24
    scene.render.resolution_percentage = 100
    render(views / 'baseline-front.png')

    camera.location = center - CAMERA_DIRECTION * span
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    render(views / 'baseline-reverse.png')

    camera.location = center + CAMERA_DIRECTION * span
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale *= 1.8
    scene.render.resolution_percentage = 45
    render(views / 'baseline-reduced.png')

    provenance = folder / 'provenance.json'
    if provenance.exists():
        record = json.loads(provenance.read_text())
        record.setdefault('supplemental_evidence', {})
        record['supplemental_evidence'].update({
            str(path.relative_to(folder)): digest(path)
            for path in sorted(views.glob('baseline-*.png'))
        })
        provenance.write_text(json.dumps(record, indent=2) + '\n')


names = os.environ.get('DUNGEON_EVIDENCE_NAMES', '').split(',')
for asset in filter(None, names):
    render_asset(asset)
