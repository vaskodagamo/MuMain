"""Read-only diffuse previews of frequent Dungeon architecture; no art acceptance."""
from pathlib import Path
import os
import sys

sys.dont_write_bytecode = True
import bpy

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0, str(REPO / 'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds, set_camera, set_lighting, render


def preview(name):
    folder = ROOT / name
    bpy.ops.wm.open_mainfile(filepath=str(folder / 'baseline.blend'))
    bpy.context.scene.frame_set(0)
    meshes = [obj for obj in bpy.context.scene.objects
              if obj.type == 'MESH' and not obj.get('mu_helper')]
    set_camera(mesh_bounds(meshes))
    set_lighting()
    bpy.context.scene.cycles.samples = 12
    render(folder / 'baseline.png')


for asset in os.environ['DUNGEON_PREVIEW_NAMES'].split(','):
    preview(asset)
