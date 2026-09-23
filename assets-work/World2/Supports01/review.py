"""Matched diffuse/wire and exact-placement context views of actual reimports."""
from pathlib import Path
import hashlib
import json
import math
import shutil
import sys
import bpy
from mathutils import Euler, Matrix, Vector

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds, set_camera, set_lighting, render
GROUPS = {'pier-support':[('Object04',9),('Object06',14)],
          'stepped-supports':[('Object13',22),('Object13',25)],
          'open-trim-run':[('Object15',65),('Object15',66),('Object15',360)]}


def path_for(name,stage):
    return ROOT/name/('validation/reimported.blend' if stage=='candidate' and name!='Object04' else 'baseline/source.blend')


def objects():
    return [obj for obj in bpy.context.scene.objects if obj.type=='MESH' and not obj.get('mu_helper') and not obj.get('mu_reference')]


def scene_setup(bounds):
    scene=bpy.context.scene
    scene.frame_set(0)
    if not scene.world:
        scene.world=bpy.data.worlds.new('ReviewWorld')
    set_camera(bounds,1.55)
    set_lighting()
    scene.cycles.samples=16
    scene.render.resolution_x,scene.render.resolution_y=800,900


def views(folder,label,bounds,wire=False):
    folder.mkdir(parents=True,exist_ok=True)
    scene=bpy.context.scene
    render(folder/(label+'.png'))
    scene.render.resolution_percentage=35
    render(folder/(label+'-small.png'))
    scene.render.resolution_percentage=100
    camera=scene.camera
    center=(Vector(bounds[0])+Vector(bounds[1]))/2
    span=max(Vector(bounds[1])-Vector(bounds[0]))
    camera.location=center+Vector((-1.6,1.8,1.2))*span
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    render(folder/(label+'-reverse.png'))
    if wire:
        for material in bpy.data.materials:
            if not material.use_nodes:
                continue
            nodes=material.node_tree.nodes
            shader=nodes.get('Principled BSDF')
            if not shader:
                continue
            wire_node=nodes.new('ShaderNodeWireframe')
            wire_node.inputs['Size'].default_value=.3
            mix=nodes.new('ShaderNodeMixRGB')
            mix.inputs[2].default_value=(.015,.015,.015,1)
            socket=shader.inputs['Base Color']
            if socket.links:
                material.node_tree.links.new(socket.links[0].from_socket,mix.inputs[1])
            material.node_tree.links.new(wire_node.outputs[0],mix.inputs[0])
            material.node_tree.links.new(mix.outputs[0],socket)
        render(folder/(label+'-wire.png'))


def individual(name):
    bounds=None
    for stage in ('current','candidate'):
        bpy.ops.wm.open_mainfile(filepath=str(path_for(name,stage)))
        bpy.context.scene.frame_set(0)
        bpy.context.view_layer.update()
        graph=bpy.context.evaluated_depsgraph_get()
        if bounds is None:
            bounds=mesh_bounds([obj.evaluated_get(graph) for obj in objects()])
        scene_setup(bounds)
        views(ROOT/name/'review',stage,bounds,True)
    # These three BMDs and their used diffuse textures have no original/current
    # difference in this batch; identical image bytes state that explicitly.
    for path in (ROOT/name/'review').glob('current*.png'):
        shutil.copy2(path,path.with_name(path.name.replace('current','original')))


def append(name,record,anchor,stage):
    with bpy.data.libraries.load(str(path_for(name,stage))) as (_,loaded):
        loaded.objects=_.objects
    transform=(Matrix.Translation(Vector(record['position'])-anchor)
               @ Euler([math.radians(a) for a in record['rotation']],'XYZ').to_matrix().to_4x4()
               @ Matrix.Scale(record['scale'],4))
    meshes=[]
    for obj in loaded.objects:
        if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type=='ARMATURE':
            obj.matrix_world=transform
        else:
            meshes.append(obj)
    return meshes


def context(label,members):
    records=[]
    for name,index in members:
        all_records=json.loads((ROOT/name/'placements.json').read_text())
        records.append((name,next(row for row in all_records if row['index']==index)))
    anchor=Vector(records[0][1]['position'])
    bounds=None
    for stage in ('current','candidate'):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        # Reset handlers belong only to this isolated review process, never an app.
        for handlers in (bpy.app.handlers.depsgraph_update_post,bpy.app.handlers.load_post):
            handlers.clear()
        meshes=[obj for name,record in records for obj in append(name,record,anchor,stage)]
        bpy.context.scene.frame_set(0)
        bpy.context.view_layer.update()
        graph=bpy.context.evaluated_depsgraph_get()
        if bounds is None:
            bounds=mesh_bounds([obj.evaluated_get(graph) for obj in meshes])
        scene_setup(bounds)
        views(ROOT/'context'/label,stage,bounds)
    (ROOT/'context'/label/'placements.json').write_text(json.dumps(dict(records=records,anchor=list(anchor),camera_bounds=bounds),indent=2)+'\n')


if __name__=='__main__':
    mode=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'individual'
    if mode=='context':
        for label,members in GROUPS.items():
            if len(sys.argv)>sys.argv.index('--')+2 and label!=sys.argv[-1]:
                continue
            context(label,members)
    else:
        for name in ('Object06','Object13','Object15'):
            individual(name)
