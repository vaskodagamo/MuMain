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
  t=max(0,min(1,(abs(p.x)-19)/55.7))
  envelope=math.sin(math.pi*t)
  # A closed bowed shell broadens through the shoulder, then tightens to the old tip.
  p.y+=envelope*(3.2+(p.y-25)*.8)
  # Three broad carved feather ridges follow the ascending wing, rather than noise.
  ridge=math.sin(math.pi*3*t)**2
  p.y+=envelope*ridge*(2.4 if p.y>26 else -2.4)
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
  part='wing' if face.index in list(range(132,143))+list(range(231,242)) else 'robe' if face.material_index==1 and max(p.z for p,u,b in rows)>85 and min(p.z for p,u,b in rows)<200 and min(math.hypot(p.x,p.y) for p,u,b in rows)<32 else None
  first=len(output)
  if part:patch(output,rows,face.material_index,part,4 if part=='wing' else 2)
  else:h.triangle(output,[p for p,u,b in rows],[u for p,u,b in rows],rows[0][2],face.material_index)
  normals.extend([Vector((0,0,0))]*(3*(len(output)-first)) if part else [obj.data.corner_normals[i].vector.copy() for i in face.loop_indices])
 h.replace_mesh(obj,output)
 for face in obj.data.polygons:face.use_smooth=True
 obj.data.normals_split_custom_set(normals)
 after=h.bound([obj.matrix_world@v.co for v in obj.data.vertices])
 assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.002,(before,after)
 bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
 (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(output),bounds_before=before,bounds_after=after,geometry_only=True),indent=2)+'\n')

if __name__=='__main__':
 for name in os.environ.get('STATUE_NAMES','StoneStatue03').split(','):build(name)
