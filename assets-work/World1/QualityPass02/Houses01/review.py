"""Actual diffuse/alpha exported candidates in matched full, reverse and reduced views."""
import json
from pathlib import Path
import sys

sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
NAMES=('House01','House03')
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import set_camera,set_lighting,render


def setup(bounds):
    set_camera(bounds,1.5); set_lighting()
    scene=bpy.context.scene
    scene.cycles.samples=16
    scene.cycles.transparent_max_bounces=16
    scene.render.film_transparent=False
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.055,.065,.075,1)
    scene.render.resolution_x=1000; scene.render.resolution_y=850


def review(name):
    folder=ROOT/name
    bounds=json.loads((folder/'validation/authored.json').read_text())['bounds_before']
    for stage,path in [('baseline','baseline/source.blend'),('candidate','validation/reimported.blend')]:
        bpy.ops.wm.open_mainfile(filepath=str(folder/path))
        bpy.context.scene.frame_set(0)
        setup(bounds)
        render(folder/'review'/f'{stage}.png')
        bpy.context.scene.render.resolution_percentage=30
        render(folder/'review'/f'{stage}-small.png')
        bpy.context.scene.render.resolution_percentage=100
        camera=bpy.context.scene.camera
        center=(Vector(bounds[0])+Vector(bounds[1]))/2
        camera.location=center+Vector((-1.35,2,1.3))*max(Vector(bounds[1])-Vector(bounds[0]))
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        render(folder/'review'/f'{stage}-reverse.png')
        bpy.context.scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.65,.65,.65,1)
        render(folder/'review'/f'{stage}-light.png')
    (folder/'review/context.json').write_text(json.dumps(dict(kind='OFFLINE DIFFUSE/ALPHA, NOT CLIENT',baseline='Actual merged BMD',candidate='Official exported/reimported candidate',bounds=bounds,backgrounds='Dark and light, reverse view',reduced='300x255'),indent=2)+'\n')


if __name__=='__main__':
    import os
    for name in os.environ.get('HOUSE_NAMES',','.join(NAMES)).split(','): review(name)
