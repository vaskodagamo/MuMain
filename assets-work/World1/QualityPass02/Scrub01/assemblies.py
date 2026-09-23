"""Compare exact World1 plant placements and stored small/large scales offline."""
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode=True
import bpy
from mathutils import Euler,Matrix,Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
NAMES=('Tree09','Tree10','Grass03','Grass04')
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds,set_camera,set_lighting,render


def append(record,stage,anchor):
    path=ROOT/record['name']/('baseline/source.blend' if stage=='baseline' else 'validation/reimported.blend')
    with bpy.data.libraries.load(str(path)) as (source,loaded): loaded.objects=source.objects
    result=[]
    for obj in loaded.objects:
        if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper'): continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type=='ARMATURE':
            rotation=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix().to_4x4()
            obj.matrix_world=Matrix.Translation(Vector(record['position'])-anchor)@rotation@Matrix.Scale(record['scale'],4)
        else: result.append(obj)
    return result


def group(label,records,anchor):
    bounds=None
    for stage in ('baseline','candidate'):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.scene.world=bpy.data.worlds.new('OfflinePlacementWorld')
        meshes=[obj for record in records for obj in append(record,stage,anchor)]
        bpy.context.scene.frame_set(0); bpy.context.view_layer.update()
        if bounds is None:
            graph=bpy.context.evaluated_depsgraph_get()
            bounds=mesh_bounds([obj.evaluated_get(graph) for obj in meshes])
        camera=set_camera(bounds,1.35); set_lighting()
        if len(records)==1: camera.data.ortho_scale=1000
        scene=bpy.context.scene
        scene.cycles.samples=16; scene.cycles.transparent_max_bounces=16
        scene.render.film_transparent=False
        scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.065,.075,.065,1)
        scene.render.resolution_x=1400 if len(records)>1 else 700
        scene.render.resolution_y=900 if len(records)>1 else 500
        render(ROOT/'review'/f'{label}-{stage}.png')
    report=dict(kind='OFFLINE ACTUAL WORLD1 TRANSFORMS, NOT CLIENT',anchor=list(anchor),placements=records,bounds=bounds,omitted='Terrain, collision, baked lighting and unrelated objects')
    (ROOT/'review'/f'{label}-placements.json').write_text(json.dumps(report,indent=2)+'\n')


(ROOT/'review').mkdir(exist_ok=True)
records=[dict(name=name,index=index,**record) for name in NAMES for index,record in enumerate(json.loads((ROOT/name/'placements.json').read_text()))]
for name,index,label in [('Grass03',105,'mixed-scrub'),('Tree09',38,'mixed-tall')]:
    selected=next(r for r in records if r['name']==name and r['index']==index)
    anchor=Vector(selected['position'])
    neighbors=[r for r in records if math.dist(r['position'][:2],anchor[:2])<600]
    group(label,neighbors,anchor)
for name,index,label in [('Grass03',69,'small-042'),('Grass04',108,'large-140')]:
    selected=next(r for r in records if r['name']==name and r['index']==index)
    group(label,[selected],Vector(selected['position']))
