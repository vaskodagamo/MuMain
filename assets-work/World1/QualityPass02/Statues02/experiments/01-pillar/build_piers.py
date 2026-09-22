"""Layered carved capitals and deep painted figure/plaque relief; outer contacts retained."""
import importlib.util
import math
import os
from pathlib import Path
import sys
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('statue_common',ROOT/'build_source.py');common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
h=common.h

def clip(poly,axis,level,keep,uv=False):
 out=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  va=a[1][axis] if uv else a[0][axis];vb=b[1][axis] if uv else b[0][axis]
  ia=va>=level-1e-8 if keep else va<=level+1e-8
  ib=vb>=level-1e-8 if keep else vb<=level+1e-8
  if ia:out.append(a)
  if ia!=ib:
   t=(level-va)/(vb-va);out.append((a[0].lerp(b[0],t),a[1].lerp(b[1],t)))
 return out

def emit(output,poly,bone,mat,transform):
 for i in range(1,len(poly)-1):
  q=[poly[0],poly[i],poly[i+1]]
  h.triangle(output,[transform(p,u) for p,u in q],[u for p,u in q],bone,mat)

def molding(output,rows,mat,levels,insets,center):
 poly=[(p,u) for p,u,b in rows]
 for lo,hi,lower,upper in zip(levels,levels[1:],insets,insets[1:]):
  part=clip(clip(poly,2,lo,True),2,hi,False)
  def move(p,u):
   d=lower+(upper-lower)*(p.z-lo)/(hi-lo);p=p.copy()
   p.x-=math.copysign(d,p.x-center[0]);p.y-=math.copysign(d,p.y-center[1]);return p
  emit(output,part,rows[0][2],mat,move)

def plaque(output,rows,mat):
 poly=[(p,u) for p,u,b in rows]
 us=[0,.635,.651,.846,.862,1];vs=[0,.3,.317,.508,.525,1]
 def move(p,u):
  d=min((u.x-.635)/.016,(.862-u.x)/.016,(u.y-.3)/.017,(.525-u.y)/.017)
  p=p.copy();p.y-=6*max(0,min(1,d));return p
 for u0,u1 in zip(us,us[1:]):
  p=clip(clip(poly,0,u0,True,True),0,u1,False,True)
  for v0,v1 in zip(vs,vs[1:]):emit(output,clip(clip(p,1,v0,True,True),1,v1,False,True),rows[0][2],mat,move)

def build(name):
 import json
 folder=ROOT/name;bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
 obj=h.model_mesh();h.snapshot_reference(obj)
 ref=bpy.data.collections.new('REF_ORIGINAL');bpy.context.scene.collection.children.link(ref);ref.hide_render=ref.hide_viewport=True
 with bpy.data.libraries.load(str(folder/'original/source.blend')) as (s,d):d.objects=s.objects
 for item in d.objects:
  if item.type=='MESH' and not item.get('mu_helper'):
   item.parent=None;item.modifiers.clear();item['mu_reference']=True;ref.objects.link(item)
 output=[];normals=[];before=h.bound([obj.matrix_world@v.co for v in obj.data.vertices])
 for f in obj.data.polygons:
  rows=[h.corner(obj,i) for i in f.loop_indices];start=len(output);changed=True
  if name=='StoneStatue01' and 15<=f.index<=22:
   molding(output,rows,f.material_index,[375.99,379,383,388,393.41],[0,4,6,1,0],(0,1.067))
  elif name=='StoneStatue01' and f.index>=48:
   # Existing sculpted figure front receives measured depth with its rim anchored.
   def relief(p,u):
    p=p.copy();fade=max(0,min(1,(-p.y-6.3)/12))*max(0,min(1,(p.z-145.8)/22,(357.4-p.z)/9))
    p.y-=fade*(7+5*math.exp(-((p.z-340)/15)**2));return p
   emit(output,[(p,u) for p,u,b in rows],rows[0][2],f.material_index,relief)
  elif name=='SteelStatue01' and 52<=f.index<=59:
   molding(output,rows,f.material_index,[202.05,205,209,214,218,220.71],[0,4,5,5,0,0],(0,.879))
  elif name=='SteelStatue01' and 62<=f.index<=69:
   molding(output,rows,f.material_index,[220.69,229,242,253,265.4],[0,2,5,3,0],(0,.879))
  elif name=='SteelStatue01' and f.index in (18,19):plaque(output,rows,f.material_index)
  else:
   changed=False;h.triangle(output,[p for p,u,b in rows],[u for p,u,b in rows],rows[0][2],f.material_index)
  normals.extend([Vector() for _ in range(3*(len(output)-start))] if changed else [obj.data.corner_normals[i].vector.copy() for i in f.loop_indices])
 h.replace_mesh(obj,output)
 for f in obj.data.polygons:f.use_smooth=True
 obj.data.normals_split_custom_set(normals)
 after=h.bound([obj.matrix_world@v.co for v in obj.data.vertices]);assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.002,(name,before,after)
 bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
 (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(output),bounds_before=before,bounds_after=after,geometry_only=True),indent=2)+'\n')

if __name__=='__main__':
 for name in os.environ.get('STATUE_NAMES','StoneStatue01,SteelStatue01').split(','):build(name)
