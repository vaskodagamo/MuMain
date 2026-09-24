import bpy,math
from mathutils import Vector,Quaternion
from pathlib import Path
D=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(D/'source.blend'))
bpy.ops.preferences.addon_enable(module='io_scene_valvesource')
for c in bpy.data.collections:c.vs.export=False
bpy.ops.wm.save_as_mainfile(filepath=str(D/'source.blend'),compress=True)
s=bpy.context.scene;bpy.ops.object.camera_add(location=(200,-44,0));c=bpy.context.object;c.rotation_euler=((Vector((0,-44,0))-c.location).to_track_quat('-Z','Y')@Quaternion((0,0,1),math.pi/2)).to_euler();c.data.type='ORTHO';c.data.ortho_scale=125;s.camera=c
s.render.resolution_x=40;s.render.resolution_y=120;s.render.resolution_percentage=100;s.render.image_settings.file_format='PNG';s.render.filepath=str(D/'review/inventory-1x3-40x120.png');bpy.ops.render.render(write_still=True)
