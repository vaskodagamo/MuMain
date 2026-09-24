"""Closed shape retopology, source colour bake and original attachment fit."""
import bpy,bmesh,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[6]
DEL=ROOT/'assets-work/Items/requests/2026-09-24-0-1-rebuild-short-sword-clean/delivery/0-1'
SOURCE=ROOT.parent/'item-sources/0-1/0-1-short-sword.glb'
SIZE=1024

def mesh(name,verts,faces):
 data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
 obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj)
 bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
 return obj

def rings(name,sections):
 n=len(sections[0]);verts=[p for section in sections for p in section]
 faces=[tuple(reversed(range(n))),tuple((len(sections)-1)*n+i for i in range(n))]
 for k in range(len(sections)-1):
  for i in range(n): faces.append((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i))
 return mesh(name,verts,faces)

def blade():
 sections=[]
 for y,top,low,width in [(-.60,.183,.083,.006),(-.50,.176,.046,.018),(-.25,.174,.067,.017),(.13,.18,.096,.014)]:
  mid=(top+low)/2
  coords=[(0,top),(width*.60,top-.019),(width,mid),(width*.55,low+.013),(0,low),(-width*.55,low+.013),(-width,mid),(-width*.60,top-.019)]
  section=[(x,y,z) for x,z in coords]
  if y==-.60:
   section=[(x*.05, -.711 if i==0 else (-.515 if i==4 else -.54), .187 if i==0 else (.045 if i==4 else .139)) for i,(x,z) in enumerate(coords)]
  sections.append(section)
 return rings('Blade_closed',sections)

def guard():
 outline=[(.09,.253),(.194,.195),(.197,.157),(.204,.134),(.196,.116),(.196,.060),(.140,.004),(.075,.023),(.096,.043),(.122,.065),(.131,.087),(.123,.109),(.107,.134),(.127,.156),(.133,.180),(.121,.202),(.076,.224)]
 obj=mesh('Guard_closed',[(x,y,z) for x in [-.015,.015] for y,z in outline],[])
 n=len(outline);faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 obj.data.clear_geometry();obj.data.from_pydata([(x,y,z) for x in [-.015,.015] for y,z in outline],[],faces)
 bpy.context.view_layer.objects.active=obj;obj.select_set(True)
 mod=obj.modifiers.new('Forged bevel','BEVEL');mod.width=.0035;mod.segments=1
 bpy.ops.object.modifier_apply(modifier=mod.name);obj.select_set(False)
 return obj

def grip():
 sections=[]
 for y,r in [(.196,.020),(.213,.018),(.25,.017),(.30,.017),(.35,.017),(.401,.017),(.45,.019)]:
  sections.append([(r*math.cos(i*math.tau/8),y,.13+r*1.08*math.sin(i*math.tau/8)) for i in range(8)])
 return rings('Leather_grip_closed',sections)

def pommel():
 return rings('Pommel_closed',[[(r*math.cos(math.pi/4+i*math.tau/4),y,.13+r*math.sin(math.pi/4+i*math.tau/4)) for i in range(4)] for y,r in [(.445,.026),(.45,.031),(.471,.031),(.483,.014)]])

def diamond():
 verts=[(.015,.16,.155),(.015,.14,.132),(.015,.16,.108),(.015,.181,.132),(.029,.16,.132),(.012,.16,.132)]
 return mesh('Guard_diamond_closed',verts,[(0,1,4),(1,2,4),(2,3,4),(3,0,4),(1,0,5),(2,1,5),(3,2,5),(0,3,5)])

def unwrap(obj):
 bpy.context.view_layer.objects.active=obj;obj.select_set(True)
 bm=bmesh.new();bm.from_mesh(obj.data)
 for edge in bm.edges:
  edge.seam=len(edge.link_faces)!=2 or edge.calc_face_angle()>.55
 bm.to_mesh(obj.data);bm.free()
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.unwrap(method='ANGLE_BASED',margin=.025);bpy.ops.object.mode_set(mode='OBJECT')

def bake(source,obj):
 mat=bpy.data.materials.new('sword03.jpg');mat.use_nodes=True
 img=bpy.data.images.new('sword03',width=SIZE,height=SIZE,alpha=True)
 node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=img;mat.node_tree.nodes.active=node
 mat.node_tree.links.new(node.outputs['Color'],mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'])
 obj.data.materials.append(mat)
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
 scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.055;scene.render.bake.max_ray_distance=.12
 scene.render.bake.margin=32;scene.render.bake.margin_type='EXTEND';scene.render.bake.use_pass_direct=False;scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
 bpy.ops.object.select_all(action='DESELECT');source.select_set(True);obj.select_set(True);bpy.context.view_layer.objects.active=obj
 bpy.ops.object.bake(type='DIFFUSE')
 img.filepath_raw=str(DEL/'exports/bake_raw.png');img.file_format='PNG';img.save()

def fit(obj):
 for v in obj.data.vertices:
  x,y,z=v.co;v.co=(x*80-.028, (y-.2885)*65 if y>=.15 else -9.0025+(y-.15)*108.218,.607+(z-.13)*125)
 obj.data.update()

bpy.ops.wm.open_mainfile(filepath='/tmp/short-sword-original.blend')
original=next(o for o in bpy.context.scene.objects if o.type=='MESH' and o.name!='smd_bone_vis')
original.name='REF_Original_Sword';original['mu_reference']=True;original.hide_render=True
arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
bpy.ops.import_scene.gltf(filepath=str(SOURCE));source=next(o for o in bpy.context.selected_objects if o.type=='MESH')
bpy.context.view_layer.objects.active=source;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.object.select_all(action='DESELECT')
parts=[blade(),guard(),grip(),pommel(),diamond()]
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();obj=bpy.context.object;obj.name='Short_Sword_Retopology'
unwrap(obj);bake(source,obj)
bpy.data.objects.remove(source,do_unlink=True)
fit(obj)
group=obj.vertex_groups.new(name=arm.data.bones[0].name);group.add(list(range(len(obj.data.vertices))),1,'REPLACE');obj.parent=arm
mod=obj.modifiers.new('Original skeleton','ARMATURE');mod.object=arm
bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bad=sum(not e.is_manifold for e in bm.edges);bm.to_mesh(obj.data);bm.free()
obj.data.calc_loop_triangles()
report=dict(triangles=len(obj.data.loop_triangles),non_manifold_edges=bad,bake_margin_px=32,bake_margin_type='EXTEND',texture_size=SIZE,source_reference=str(SOURCE),method='Closed shape retopology, no decimation',grip_original_y=[-6.0052,12.9662],grip_new_y=[(.196-.2885)*65,(.45-.2885)*65],origin_inside_grip=True)
(DEL/'validation/geometry.json').write_text(json.dumps(report,indent=2)+'\n')
# Remove all unused generator data before saving. Only reduced geometry and original reference remain.
for _ in range(3):bpy.data.orphans_purge(do_recursive=True)
bpy.ops.wm.save_as_mainfile(filepath=str(DEL/'source.blend'),compress=True)
print('GEOMETRY',report)
