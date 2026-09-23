"""Test alternate source normal coordinate spaces without changing export artifacts."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from posed_source import actual_triangles
folder=ROOT/'Waterspout01';triangles=actual_triangles(folder/'baseline/smd/Waterspout01.smd');reports=[]
for mode in ('sharp_water','sharp_all','flat_water'):
 bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
 obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'));mesh=obj.data
 water_edges={mesh.loops[i].edge_index for f in mesh.polygons if f.material_index==3 for i in f.loop_indices}
 for edge in mesh.edges:
  if mode=='sharp_all' or edge.index in water_edges:edge.use_edge_sharp=True
 if mode=='flat_water':
  for face in mesh.polygons:
   if face.material_index==3:face.use_smooth=False
 normals=[(obj.matrix_world.to_3x3().transposed()@Vector(row[4:7])).normalized() for material,rows in triangles for row in rows]
 mesh.normals_split_custom_set(normals);mesh.update()
 errors=[(mesh.corner_normals[i].vector-normals[i]).length for i in range(len(normals))]
 records=sorted([(error,i//3,i%3) for i,error in enumerate(errors)],reverse=True)
 reports.append(dict(mode=mode,maximum=max(errors),water_maximum=max(errors[549*3:]),worst=records[:20]))
(folder/'validation/fan-diagnostic.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports,indent=2))
