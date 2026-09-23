"""Broaden carved dragon anatomy and bow coherent bat-wing membrane panels."""
import importlib.util
import json
import math
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('sculpt_helpers',ROOT.parent/'Statues02/build_source.py');common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
h=common.h
WATER=[]

def smoothstep(t):
 t=max(0,min(1,t));return t*t*(3-2*t)

def sculpt(point,wing=False):
 p=point.copy()
 if wing:
  right=p.x>=-7.902;span=205.821 if right else 155.209
  across=abs(p.x+7.902);t=max(0,min(1,across/span))
  # The original bent pose is retained; the membrane opens into a broad web.
  profile=[(0,44),(36,63),(77,28),(119,23),(142,42),(163,39),(206,93)] if right else [(0,44),(36,63),(77,28),(121,34),(137,58),(150,58),(155.209,118.492)]
  leading=profile[-1][1]
  for (x0,y0),(x1,y1) in zip(profile,profile[1:]):
   if x0<=across<=x1:
    leading=y0+(y1-y0)*(across-x0)/(x1-x0);break
  web=smoothstep((p.y-leading-9)/42)
  growth=math.sin(math.pi*t)**1.3
  p.y+=min(18*growth*web,max(0,118.492-p.y)*.65)
  p.z-=min(21*growth*web,max(0,p.z-193.74)*.5)
  # Lift the long proximal chord into the wrist curve, without moving the old crest.
  curve=19*math.exp(-((across-43)/29)**2)*(1-web)*smoothstep(t/.13)
  p.z+=min(curve,max(0,311.204803-p.z)*.6)
  p.y-=11*math.exp(-((across-43)/29)**2)*(1-web)*smoothstep(t/.13)
  p=point.lerp(p,smoothstep((311.204803-point.z)/6))
  if t>.9999:p=point.copy()
 else:
  # Broader chest and a continuous neck; paws/tail tips and the water outlet stay fixed.
  z=(p.z-125)/120
  body=math.sin(math.pi*max(0,min(1,z)))
  radial=max(0,1-(abs(p.x+7.902)/57)**4)
  front=smoothstep((115-p.y)/55)
  p.x+=(p.x+7.902)*.55*body*radial*front
  p.y-=9*body*radial*front
  if 258<p.z<294 and p.y>6:
   hood=math.sin(math.pi*(p.z-258)/36)*smoothstep((p.y-6)/14)
   p.x+=(p.x+4.4)*.45*hood
  distance=min((point-water).length for water in WATER)
  preserve=smoothstep((distance-14)/14)
  p=point.lerp(p,preserve)
 return p

def patch(output,rows,mat,wing):
 if not wing:
  h.triangle(output,[sculpt(p) for p,u,b in rows],[u for p,u,b in rows],rows[0][2],mat);return
 pairs=[(p,u) for p,u,b in rows]
 mids=[(pairs[i][0].lerp(pairs[(i+1)%3][0],.5),pairs[i][1].lerp(pairs[(i+1)%3][1],.5)) for i in range(3)]
 for triangle in ((pairs[0],mids[0],mids[2]),(mids[0],pairs[1],mids[1]),(mids[2],mids[1],pairs[2]),tuple(mids)):
  h.triangle(output,[sculpt(p,True) for p,u in triangle],[u for p,u in triangle],rows[0][2],mat)

def replace_mesh(obj,triangles,topology):
 old=obj.data;groups=[g.name for g in obj.vertex_groups];mesh=bpy.data.meshes.new('CarvedDragonAndProtectedFountain')
 points=[];faces=[];lookup={}
 for face_index,(xyz,uv,bones,mat) in enumerate(triangles):
  if isinstance(bones,int):bones=(bones,)*3
  indices=[]
  for corner_index,(point,bone) in enumerate(zip(xyz,bones)):
   local=obj.matrix_world.inverted()@point
   key=("baseline",topology[face_index][corner_index]) if topology[face_index] is not None else ("authored",bone,mat,*local)
   if key not in lookup:lookup[key]=len(points);points.append(local)
   indices.append(lookup[key])
  faces.append(indices)
 mesh.from_pydata(points,[],faces)
 for mat in old.materials:mesh.materials.append(mat)
 layer=mesh.uv_layers.new(name=old.uv_layers[0].name);obj.data=mesh;obj.vertex_groups.clear()
 for name in groups:obj.vertex_groups.new(name=name)
 for face,(xyz,uv,bones,mat) in zip(mesh.polygons,triangles):
  face.material_index=mat
  if isinstance(bones,int):bones=(bones,)*3
  for loop,tex,bone in zip(face.loop_indices,uv,bones):
   layer.data[loop].uv=tex;obj.vertex_groups[bone].add([mesh.loops[loop].vertex_index],1,'REPLACE')
 mesh.update()


def build():
 folder=ROOT/'Waterspout01';bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
 obj=h.model_mesh();h.snapshot_reference(obj)
 assert len(obj.data.polygons)==639 and len(obj.data.materials)==4
 assert [m.name for m in obj.data.materials]==['stone_statue02.jpg','reagon_waterspout.jpg','ston01.jpg','ston02.jpg']
 for f in obj.data.polygons[72:489]:
  assert f.material_index==1 and all(h.corner(obj,i)[2]==0 for i in f.loop_indices)
 assert max(h.corner(obj,i)[0].z for f in obj.data.polygons[72:106] for i in f.loop_indices)<55
 collection=bpy.data.collections.new('REF_ORIGINAL');bpy.context.scene.collection.children.link(collection);collection.hide_render=collection.hide_viewport=True
 with bpy.data.libraries.load(str(folder/'original/source.blend')) as (s,d):d.objects=s.objects
 for item in d.objects:
  if item.type=='MESH' and not item.get('mu_helper'):
   item.parent=None;item.modifiers.clear();item['mu_reference']=True;collection.objects.link(item)
 WATER.extend(p for f in obj.data.polygons if f.material_index==3 for p,u,b in [h.corner(obj,i) for i in f.loop_indices])
 lines=(folder/'baseline/smd/Waterspout01.smd').read_text().split('triangles\n')[1].splitlines()
 baseline_rows=[[list(map(float,row.split())) for row in lines[i+1:i+4]] for i in range(0,len(lines)-1,4)]
 normal_to_local=obj.matrix_world.to_3x3().transposed()
 output=[];normals=[];topology=[];before=h.bound([obj.matrix_world@v.co for v in obj.data.vertices]);protected=[]
 for face in obj.data.polygons:
  rows=[h.corner(obj,i) for i in face.loop_indices];start=len(output)
  changed=395<=face.index<=488 or (106<=face.index<=394 and max((sculpt(p)-p).length for p,u,b in rows)>.0000001)
  if changed:patch(output,rows,face.material_index,face.index>=395)
  else:
   output.append(([p for p,u,b in rows],[u for p,u,b in rows],tuple(b for p,u,b in rows),face.material_index));protected.append(face.index)
  topology.extend([None]*(len(output)-start) if changed else [tuple(obj.data.loops[i].vertex_index for i in face.loop_indices)])
  normals.extend([Vector() for _ in range(3*(len(output)-start))] if changed else [(normal_to_local@Vector(row[4:7])).normalized() for row in baseline_rows[face.index]])
 replace_mesh(obj,output,topology)
 buckets={}
 for f in obj.data.polygons:
  for loop in f.loop_indices:
   if normals[loop].length<.001:
    key=tuple(round(v,4) for v in obj.data.vertices[obj.data.loops[loop].vertex_index].co);buckets.setdefault(key,[]).append(f.normal.copy())
 for f in obj.data.polygons:
  f.use_smooth=True
  for loop in f.loop_indices:
   if normals[loop].length<.001:
    key=tuple(round(v,4) for v in obj.data.vertices[obj.data.loops[loop].vertex_index].co)
    normals[loop]=sum((n for n in buckets[key] if n.dot(f.normal)>.5),Vector()).normalized()
 # Water's imported smooth fan cannot encode one legacy normal accurately.
 # A sharp fan boundary changes only the codec basis; custom directions remain exact.
 water_edges={obj.data.loops[i].edge_index for face in obj.data.polygons if face.material_index==3 for i in face.loop_indices}
 for edge in obj.data.edges:
  if edge.index in water_edges:edge.use_edge_sharp=True
 obj.data.normals_split_custom_set(normals)
 after=h.bound([obj.matrix_world@v.co for v in obj.data.vertices]);assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.0003,(before,after)
 bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
 (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(output),bounds_before=before,bounds_after=after,protected_original_triangle_indices=protected,geometry_only=True),indent=2)+'\n')

if __name__=='__main__':build()
