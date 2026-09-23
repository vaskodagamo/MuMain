"""Measure custom-normal encoding at protected rock face506 without modifying artifacts."""
import bpy,json,sys,os
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
folder=ROOT/'Waterspout01'
lines=(folder/'baseline/smd/Waterspout01.smd').read_text().split('triangles\n')[1].splitlines()
face_index=int(os.environ.get('NORMAL_FACE','506'))
rows=[list(map(float,s.split())) for s in lines[face_index*4+1:face_index*4+4]]
records=[]
for stage in ('baseline/source.blend','source.blend'):
 bpy.ops.wm.open_mainfile(filepath=str(folder/stage))
 for obj in bpy.context.scene.objects:
  if obj.type!='MESH' or obj.get('mu_helper') or obj.get('mu_reference'):continue
  for face in obj.data.polygons:
   for loop in face.loop_indices:
    v=obj.matrix_world@obj.data.vertices[obj.data.loops[loop].vertex_index].co
    if (v-Vector(rows[2][1:4])).length>.0003:continue
    normal=obj.data.corner_normals[loop].vector.copy();expected=(obj.matrix_world.to_3x3().transposed()@Vector(rows[2][4:7])).normalized()
    records.append(dict(stage=stage,obj=obj.name,face=face.index,loop=loop,actual=list(normal),expected=list(expected),error=(normal-expected).length,smooth=face.use_smooth))
    normals=[n.vector.copy() for n in obj.data.corner_normals];normals[loop]=expected
    obj.data.normals_split_custom_set(normals);obj.data.update()
    records[-1]['immediate_reassigned']=list(obj.data.corner_normals[loop].vector)
(folder/'validation/normal-encoding-diagnostic.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(records,indent=2))
