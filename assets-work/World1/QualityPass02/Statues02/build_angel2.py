"""Sculpt bowed stone wings and broad drapery planes within retained placement bounds."""
import importlib.util
import json
import math
import os
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent

def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
h=load('mesh_helpers',ROOT.parent/'Scrub01/build_source.py')


def sculpt(point,part):
 p=point.copy()
 if part=='wing':
  x=abs(p.x);end=74.606697 if p.x>0 else 72.766998
  t=max(0,min(1,(x-19)/(end-19)))
  envelope=math.sin(math.pi*t)
  lower=201.81+(x-32.48)*(223.29-201.81)/(42.65-32.48) if x<42.65 else 223.29+(x-42.65)*(264.472-223.29)/(74.6067-42.65)
  edge=max(0,min(1,(lower+12-p.z)/12))
  # Inward scallops separate four broad feather masses along the free lower edge.
  cuts=(32,42.65,55.5,66.5,end)
  for lo,hi in zip(cuts,cuts[1:]):
   if lo<=x<=hi:
    rise=6.5*math.sin(math.pi*(x-lo)/(hi-lo))**2*edge
    ceiling=264.471985 if p.x>0 else 264.767609
    p.z+=min(rise,max(0,ceiling-p.z)*.75)
  p.y+=envelope*(3.2+(p.y-25)*1.15)
  ridge=math.sin(math.pi*3*t)**2
  p.y+=envelope*ridge*(3.6 if p.y>26 else -3.6)
 elif part=='arm':
  side=1 if p.x>0 else -1
  root=Vector((side*24,-7,197));tip=Vector((side*44,-47,167))
  axis=tip-root;t=(p-root).dot(axis)/axis.length_squared
  amount=max(0,min(1,t/.18,(1-t)/.2))
  center=root+axis*t;radial=p-center
  radius=10.2-4.7*max(0,min(1,t))
  if radial.length>.001:p=p.lerp(center+radial.normalized()*radius,amount*.68)
 elif part=='robe':
  z=max(0,min(1,(p.z-85)/115))
  strength=math.sin(math.pi*z)*max(0,min(1,(32-math.hypot(p.x,p.y))/8))
  theta=math.atan2(p.y,p.x)
  # Four generous drapery planes preserve the shoulder/hem junctions.
  radial=2.9*strength*math.cos(theta*4+.35)
  direction=Vector((p.x,p.y,0)).normalized()
  p+=direction*radial
 return p


def patch(output,rows,mat,part,n):
 points=[p for p,u,b in rows];uvs=[u for p,u,b in rows]
 def point(i,j):
  w=(1-(i+j)/n,i/n,j/n)
  p=sum((q*v for q,v in zip(points,w)),Vector())
  uv=sum((q*v for q,v in zip(uvs,w)),Vector((0,0)))
  return sculpt(p,part),uv
 for i in range(n):
  for j in range(n-i):
   ids=[(i,j),(i+1,j),(i,j+1)]
   pairs=[point(*idx) for idx in ids]
   h.triangle(output,[p for p,u in pairs],[u for p,u in pairs],rows[0][2],mat)
   if i+j<n-1:
    pairs=[point(*idx) for idx in [(i+1,j),(i+1,j+1),(i,j+1)]]
    h.triangle(output,[p for p,u in pairs],[u for p,u in pairs],rows[0][2],mat)


def build(name):
 folder=ROOT/name;bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
 obj=h.model_mesh();h.snapshot_reference(obj)
 reference=bpy.data.collections.new('REF_ORIGINAL');bpy.context.scene.collection.children.link(reference)
 reference.hide_viewport=reference.hide_render=True
 with bpy.data.libraries.load(str(folder/'original/source.blend')) as (s,d):d.objects=s.objects
 for item in d.objects:
  if item.type=='MESH' and not item.get('mu_helper'):
   item.parent=None;item.modifiers.clear();item['mu_reference']=True;reference.objects.link(item)
 before=h.bound([obj.matrix_world@v.co for v in obj.data.vertices]);output=[];normals=[]
 for face in obj.data.polygons:
  rows=[h.corner(obj,i) for i in face.loop_indices]
  part='arm' if face.index in list(range(100,132))+list(range(199,231)) else 'wing' if face.index in list(range(132,143))+list(range(231,242)) else 'robe' if face.material_index==1 and max(p.z for p,u,b in rows)>85 and min(p.z for p,u,b in rows)<200 and min(math.hypot(p.x,p.y) for p,u,b in rows)<32 else None
  first=len(output)
  if part:patch(output,rows,face.material_index,part,8 if part=='wing' else 3 if part=='arm' else 2)
  else:h.triangle(output,[p for p,u,b in rows],[u for p,u,b in rows],rows[0][2],face.material_index)
  normals.extend([Vector((0,0,0))]*(3*(len(output)-first)) if part else [obj.data.corner_normals[i].vector.copy() for i in face.loop_indices])
 h.replace_mesh(obj,output)
 for face in obj.data.polygons:face.use_smooth=True
 # Smooth sculptural patches through shared positional corners, retaining old normals elsewhere.
 buckets={}
 for f in obj.data.polygons:
  for loop in f.loop_indices:
   if normals[loop].length<.001:
    key=tuple(round(v,4) for v in obj.data.vertices[obj.data.loops[loop].vertex_index].co)
    buckets.setdefault(key,[]).append(f.normal.copy())
 for f in obj.data.polygons:
  for loop in f.loop_indices:
   if normals[loop].length<.001:
    key=tuple(round(v,4) for v in obj.data.vertices[obj.data.loops[loop].vertex_index].co)
    normals[loop]=sum((n for n in buckets[key] if n.dot(f.normal)>.5),Vector()).normalized()
 obj.data.normals_split_custom_set(normals)
 after=h.bound([obj.matrix_world@v.co for v in obj.data.vertices])
 assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.002,(before,after)
 bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
 (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(output),bounds_before=before,bounds_after=after,geometry_only=True),indent=2)+'\n')

if __name__=='__main__':
 for name in os.environ.get('STATUE_NAMES','StoneStatue03').split(','):build(name)
