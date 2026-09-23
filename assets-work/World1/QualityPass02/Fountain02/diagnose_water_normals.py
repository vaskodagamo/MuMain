"""Corner-exact protected water normal diagnosis; never saves source files."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from posed_source import actual_triangles
folder=ROOT/'Waterspout01'
material,rows=actual_triangles(folder/'baseline/smd/Waterspout01.smd')[565]
records=[]
for stage in ('baseline/source.blend','source.blend'):
 bpy.ops.wm.open_mainfile(filepath=str(folder/stage))
 for obj in bpy.context.scene.objects:
  if obj.type!='MESH' or obj.get('mu_helper') or obj.get('mu_reference'):continue
  for face in obj.data.polygons:
   if obj.data.materials[face.material_index].name!=material:continue
   loops=list(face.loop_indices)
   for shift in range(3):
    ordered=loops[shift:]+loops[:shift]
    if max((obj.matrix_world@obj.data.vertices[obj.data.loops[loop].vertex_index].co-Vector(row[1:4])).length for loop,row in zip(ordered,rows))>.0003:continue
    corners=[]
    for loop,row in zip(ordered,rows):
     vertex=obj.data.vertices[obj.data.loops[loop].vertex_index]
     current=obj.data.corner_normals[loop].vector.copy();expected=(obj.matrix_world.to_3x3().transposed()@Vector(row[4:7])).normalized()
     corners.append(dict(loop=loop,vertex=vertex.index,groups=[(obj.vertex_groups[g.group].name,g.weight) for g in vertex.groups],bone=row[0],actual=list(current),expected=list(expected),error=(current-expected).length))
    normals=[n.vector.copy() for n in obj.data.corner_normals]
    for loop,row in zip(ordered,rows):normals[loop]=(obj.matrix_world.to_3x3().transposed()@Vector(row[4:7])).normalized()
    obj.data.normals_split_custom_set(normals);obj.data.update()
    for record,loop in zip(corners,ordered):record['immediate_reassigned']=list(obj.data.corner_normals[loop].vector)
    records.append(dict(stage=stage,obj=obj.name,face=face.index,corners=corners))
(folder/'validation/water-normal-diagnostic.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(records,indent=2))
