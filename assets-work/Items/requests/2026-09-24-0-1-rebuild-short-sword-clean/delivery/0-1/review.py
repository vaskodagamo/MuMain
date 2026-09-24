import bpy,bmesh,numpy as np,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[6]
D=ROOT/'assets-work/Items/requests/2026-09-24-0-1-rebuild-short-sword-clean/delivery/0-1'
bpy.ops.wm.open_mainfile(filepath=str(D/'source.blend'))
o=bpy.data.objects['Short_Sword_Retopology'];ref=bpy.data.objects['REF_Original_Sword']
img=bpy.data.images['sword03'];a=np.array(img.pixels[:],dtype=np.float32).reshape(1024,1024,4)
valid=a[:,:,:3].max(axis=2)>.005
while not valid.all():
 old=valid.copy()
 for axis,shift in [(0,1),(0,-1),(1,1),(1,-1)]:
  mask=np.roll(old,shift,axis)&~valid
  if axis==0:mask[0 if shift==1 else -1,:]=False
  else:mask[:,0 if shift==1 else -1]=False
  a[mask]=np.roll(a,shift,axis)[mask];valid|=mask
 if np.array_equal(old,valid):raise RuntimeError('Padding failed')
a[:,:,3]=1;img.pixels.foreach_set(a.ravel());img.update()
s=bpy.context.scene;s.render.image_settings.file_format='JPEG';s.render.image_settings.quality=98;s.view_settings.view_transform='Standard';s.view_settings.look='None'
img.filepath_raw=str(D/'exports/sword03.jpg');img.file_format='JPEG';img.save();img.pack()
# An emission preview shows only the diffuse colour the legacy renderer receives.
for mat in [o.data.materials[0],ref.data.materials[0]]:
 nt=mat.node_tree;p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');tex=p.inputs['Base Color'].links[0].from_socket
 emission=nt.nodes.new('ShaderNodeEmission');nt.links.new(tex,emission.inputs[0]);nt.links.new(emission.outputs[0],next(n for n in nt.nodes if n.type=='OUTPUT_MATERIAL').inputs[0])
s.render.engine='CYCLES';s.cycles.samples=8
s.world.color=(.12,.12,.12)
bpy.ops.object.camera_add();cam=bpy.context.object;s.camera=cam;cam.data.type='ORTHO'
s.render.resolution_percentage=100;s.render.image_settings.file_format='PNG'
def render(name,loc=(170,-85,95),target=(0,-42,0),scale=135,res=(1000,700)):
 cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
 s.render.resolution_x,s.render.resolution_y=res;s.render.filepath=str(D/'review'/name);bpy.ops.render.render(write_still=True)
ref.hide_render=True;render('after.png')
o.hide_render=True;ref.hide_render=False;render('before.png')
o.hide_render=False;ref.hide_render=True;render('front.png',(200,-44,0),(0,-44,0));render('back.png',(-200,-44,0),(0,-44,0))
render('inventory.png',(200,-44,0),(0,-44,0),135,(180,180))
# Wireframe overlay uses the same reduced surface.
wire=o.copy();wire.data=o.data.copy();bpy.context.collection.objects.link(wire);wire.modifiers.clear()
m=bpy.data.materials.new('Review_wire');m.diffuse_color=(.02,.02,.02,1);m.use_nodes=True
p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(.005,.005,.005,1)
wire.data.materials.clear();wire.data.materials.append(m);w=wire.modifiers.new('Wire','WIREFRAME');w.thickness=.11;w.use_replace=True
render('wireframe.png');bpy.data.objects.remove(wire,do_unlink=True)
# Original attachment overlay: cyan original wire over the baked replacement.
ref.hide_render=False;ref.data.materials.clear()
m=bpy.data.materials.new('Review_original_cyan');m.use_nodes=True;nt=m.node_tree;e=nt.nodes.new('ShaderNodeEmission');e.inputs[0].default_value=(0,.8,1,1);nt.links.new(e.outputs[0],nt.nodes.get('Material Output').inputs[0]);ref.data.materials.append(m)
w=ref.modifiers.new('Original_wire','WIREFRAME');w.thickness=.20
render('original-overlay.png',(200,-43,1),(0,-43,1))
ref.hide_render=True
# Clearly schematic hand around the attachment origin, with fingers across the grip.
helpers=[]
m=bpy.data.materials.new('Review_hand');m.use_nodes=True;nt=m.node_tree;e=nt.nodes.new('ShaderNodeEmission');e.inputs[0].default_value=(.36,.28,.22,1);nt.links.new(e.outputs[0],nt.nodes.get('Material Output').inputs[0])
def block(loc,scale):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);h=bpy.context.object;h.scale=scale;h.data.materials.append(m);helpers.append(h)
 bevel=h.modifiers.new('Soft glove','BEVEL');bevel.width=.18;bevel.segments=3
block((-3,1,.6),(4,12,7))
for y in [-3.5,-.4,2.7,5.8]:block((1.7,y,.6),(3.0,2.6,6.4))
block((1.2,-1.5,4),(3,7,2.5));block((-3,11,.6),(5,12,6))
render('hand-fit-schematic.png',(100,-50,65),(0,-5,.6),47,(900,700))
for h in helpers:bpy.data.objects.remove(h,do_unlink=True)
# Delivery source contains reduced model and original rig only.
bpy.data.objects.remove(ref,do_unlink=True);bpy.data.objects.remove(cam,do_unlink=True)
for _ in range(3):bpy.data.orphans_purge(do_recursive=True)
bpy.ops.wm.save_as_mainfile(filepath=str(D/'source.blend'),compress=True)
(D/'validation/texture-padding.json').write_text(json.dumps({'margin_px':32,'margin_type':'EXTEND','unused_pixels':'filled iteratively with neighbouring baked colours','black_pixels':int((a[:,:,:3].max(axis=2)<.005).sum()),'jpeg_quality':98},indent=2)+'\n')
