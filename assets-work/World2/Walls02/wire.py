"""Retain a topology-only view; not an artistic acceptance image."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[2]/'assets-work/World1/StaticBatch01'))
from review_scene import render
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Object51/review/candidate-context.blend'))
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='SINGLE'
scene.display.shading.single_color=(.4,.45,.5)
for obj in scene.objects:
    if obj.type=='MESH' and not obj.get('mu_helper'):
        obj.show_wire=True
        obj.show_all_edges=True
render(ROOT/'Object51/review/candidate-wire.png')
