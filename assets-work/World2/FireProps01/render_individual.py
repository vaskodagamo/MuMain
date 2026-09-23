"""Actual baseline geometry, with full-shell and wood-only diagnostic views."""
from pathlib import Path
import sys
import os
sys.dont_write_bytecode=True
import bpy
import bmesh
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
STAGE=os.environ.get('FIRE_VIEW_STAGE','baseline')
SOURCE='baseline/source.blend' if STAGE=='baseline' else 'validation/reimported.blend'
sys.path.insert(0,str(ROOT.parents[2]/'assets-work/World1/StaticBatch01'))
from review_scene import CAMERA_DIRECTION,mesh_bounds,render,set_camera,set_lighting


def meshes():
    return [o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper')]


def render_asset(name):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/SOURCE))
    scene=bpy.context.scene
    scene.frame_set(0)
    objects=meshes()
    bounds=mesh_bounds(objects)
    camera=set_camera(bounds)
    set_lighting()
    scene.render.film_transparent=False
    center=(Vector(bounds[0])+Vector(bounds[1]))/2
    span=max(Vector(bounds[1])-Vector(bounds[0]))
    originals = {obj: obj.data for obj in objects}
    for mode in ('full','wood'):
        for obj in objects:
            obj.data = originals[obj]
            if mode == 'wood':
                obj.data = originals[obj].copy()
                mesh = bmesh.new()
                mesh.from_mesh(obj.data)
                selected = [face for face in mesh.faces if obj.data.materials[face.material_index].get('mu_texture') == 'fire0a.jpg']
                bmesh.ops.delete(mesh, geom=selected, context='FACES')
                mesh.to_mesh(obj.data)
                mesh.free()
        camera.location=center+CAMERA_DIRECTION*span
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        render(folder/'review'/f'{STAGE}-{mode}.png')
        scene.render.resolution_percentage=35
        render(folder/'review'/f'{STAGE}-{mode}-small.png')
        scene.render.resolution_percentage=100
        camera.location=center-CAMERA_DIRECTION*span
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        render(folder/'review'/f'{STAGE}-{mode}-reverse.png')
    for obj in objects:obj.data=originals[obj]
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'review'/f'{STAGE}-context.blend'))
    scene.render.engine='BLENDER_WORKBENCH'
    scene.display.shading.color_type='MATERIAL'
    for obj in objects:obj.show_wire=True;obj.show_all_edges=True
    render(folder/'review'/f'{STAGE}-wire.png')


for name in ('Object42','Object43'):render_asset(name)
