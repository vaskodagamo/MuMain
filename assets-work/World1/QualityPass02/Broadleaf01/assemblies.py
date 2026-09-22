"""Matching actual placement cluster from unchanged World1 Object1 records."""
from pathlib import Path
import os
import json
import math
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector,Matrix,Euler
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'StaticBatch01'))
from review_scene import mesh_bounds,set_camera,set_lighting,render
records=[]
for name in ('Grass05','Grass06'):
    records.extend(dict(name=name,index=i,**r) for i,r in enumerate(json.loads((ROOT/name/'placements.json').read_text())['placements']))
# Densest six-plant neighborhood, chosen from actual placements.
def nearest(record):
    return sorted(records,key=lambda r:(Vector(r['position'])-Vector(record['position'])).length)[:6]
anchor_record=min(records,key=lambda r:max((Vector(q['position'])-Vector(r['position'])).length for q in nearest(r)))
selected=nearest(anchor_record)
if os.environ.get('BROADLEAF_ASSEMBLY')=='Grass05':
    selected=json.loads((ROOT/'review-assemblies/grass05-selected.json').read_text())
    anchor_record=selected[0]
anchor=Vector(anchor_record['position'])
out=ROOT/'review-assemblies'
if os.environ.get('BROADLEAF_ASSEMBLY')=='Grass05':out=out/'Grass05'
out.mkdir(exist_ok=True)
(out/'placements.json').write_text(json.dumps(selected,indent=2))
for stage in ('current','candidate'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.world=bpy.data.worlds.new('ReviewWorld')
    meshes=[]
    for record in selected:
        folder=ROOT/record['name'];path=folder/('baseline/source.blend' if stage=='current' else 'validation/reimported.blend')
        with bpy.data.libraries.load(str(path)) as (source,loaded):loaded.objects=source.objects
        for obj in loaded.objects:
            if obj.type not in ('ARMATURE','MESH') or obj.get('mu_helper'):continue
            bpy.context.scene.collection.objects.link(obj)
            if obj.type=='MESH':meshes.append(obj)
            else:
                rot=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix().to_4x4()
                obj.matrix_world=Matrix.Translation(Vector(record['position'])-anchor)@rot@Matrix.Scale(record['scale'],4)
    bpy.context.scene.frame_set(0);bpy.context.view_layer.update()
    if stage=='current':bounds=mesh_bounds(meshes)
    set_camera(bounds);set_lighting();bpy.context.scene.cycles.samples=16
    render(out/(stage+'.png'))
    bpy.context.scene.render.resolution_percentage=40
    render(out/(stage+'-small.png'))
