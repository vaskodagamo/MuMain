"""Actual placement contacts, immutable neighbors, separate additive approximation."""
import json
import math
import os
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Euler,Matrix,Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds,set_camera,set_lighting,render
NAMES=os.environ.get('STATUE_NAMES','StoneStatue01,StoneStatue03,SteelStatue01').split(',')




def append(record,anchor,stage):
    name=record['name']
    if name in ('Light03','PoseBox01'):return []
    path=ROOT/name/('baseline/source.blend' if stage=='baseline' else 'validation/reimported.blend') if name in ('StoneStatue01','StoneStatue03','SteelStatue01') else ROOT/'context'/name/'source.blend'
    with bpy.data.libraries.load(str(path)) as (source,loaded):loaded.objects=source.objects
    result=[]
    for obj in loaded.objects:
        if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type=='ARMATURE':
            rotation=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix().to_4x4()
            obj.matrix_world=Matrix.Translation(Vector(record['position'])-anchor)@rotation@Matrix.Scale(record['scale'],4)
        else:result.append(obj)
    return result


def fit_camera(bounds):
    from bpy_extras.object_utils import world_to_camera_view
    scene=bpy.context.scene;bpy.context.view_layer.update()
    corners=[Vector((x,y,z)) for x in (bounds[0][0],bounds[1][0]) for y in (bounds[0][1],bounds[1][1]) for z in (bounds[0][2],bounds[1][2])]
    projected=[world_to_camera_view(scene,scene.camera,p) for p in corners]
    factor=max(max(abs(p.x-.5),abs(p.y-.5))*2 for p in projected)
    scene.camera.data.ortho_scale*=max(1,factor)*1.08


def group(label,data):
    bounds=None
    for stage in ('baseline','candidate'):
        for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
        bpy.context.scene.world=bpy.data.worlds.new('PlacementWorld')
        anchor=Vector(data['anchor']);meshes=[obj for r in data['records'] for obj in append(r,anchor,stage)]
        bpy.context.scene.frame_set(0);bpy.context.view_layer.update()
        if bounds is None:bounds=mesh_bounds([obj.evaluated_get(bpy.context.evaluated_depsgraph_get()) for obj in meshes])
        set_camera(bounds,1.28);set_lighting()
        scene=bpy.context.scene;scene.cycles.samples=16;scene.cycles.transparent_max_bounces=16
        scene.render.film_transparent=False;scene.render.resolution_x=1400;scene.render.resolution_y=1100
        scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.065,.07,.075,1)
        if label=='StoneStatue01-3':fit_camera(bounds)
        render(ROOT/'review'/f'{label}-{stage}.png')
        center=(Vector(bounds[0])+Vector(bounds[1]))/2
        camera=scene.camera;camera.location=center+Vector((-1.4,2,1.5))*max(Vector(bounds[1])-Vector(bounds[0]));camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        if label=='StoneStatue01-3':fit_camera(bounds)
        render(ROOT/'review'/f'{label}-{stage}-reverse.png')
    (ROOT/'review'/f'{label}-contacts.json').write_text(json.dumps(dict(kind='OFFLINE ACTUAL PLACEMENTS, NOT CLIENT',**data,bounds=bounds,omitted='Terrain/collision/baked lighting/unselected unrelated objects; PoseBox01 hidden operation marker omitted per ZzzObject.cpp4662-4665 (HiddenMesh=-2)'),indent=2)+'\n')


if __name__=='__main__':
    (ROOT/'review').mkdir(exist_ok=True)
    for label,data in json.loads((ROOT/'context/groups.json').read_text()).items():
        requested=os.environ.get('CONTEXT_GROUPS','').split(',')
        if data['records'][0]['name'] in NAMES and (requested==[''] or label in requested):group(label,data)
