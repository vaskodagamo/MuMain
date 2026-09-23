"""Render exact legacy scene selections from fresh current official BMD imports."""
import hashlib,json,math,os,pathlib,sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Euler,Matrix,Vector
OUT=pathlib.Path(__file__).resolve().parent
REPO=pathlib.Path(os.environ['MU_REVIEW_REPO'])
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds,render,set_camera,set_lighting
GROUP=sys.argv[sys.argv.index('--')+1]
def append(record,anchor):
 with bpy.data.libraries.load(str(OUT/record['name']/'import.blend')) as (source,loaded):loaded.objects=source.objects
 result=[];rot=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix().to_4x4();transform=Matrix.Translation(Vector(record['position'])-anchor)@rot@Matrix.Scale(record['scale'],4)
 for obj in loaded.objects:
  if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper') or obj.get('mu_reference'):continue
  bpy.context.scene.collection.objects.link(obj)
  if obj.type=='ARMATURE':obj.matrix_world=transform
  else:result.append(obj)
 return result
bpy.ops.wm.read_factory_settings(use_empty=True)
records=json.loads((OUT/(GROUP+'-placements.json')).read_text());anchor=Vector(records['coordinate_anchor']);objects=[o for r in records['objects'] for o in append(r,anchor)];bpy.context.scene.frame_set(0);bpy.context.view_layer.update();graph=bpy.context.evaluated_depsgraph_get();bounds=mesh_bounds([o.evaluated_get(graph) for o in objects]);bpy.context.scene.world=bpy.data.worlds.new('OfflineWorld')
# Legacy camera bounds preserve composition. All selected BMDs remained current baselines.
original_bounds=records['camera_bounds_from_original_evaluated_meshes'];camera=set_camera(original_bounds,scale_factor=2.2 if GROUP=='house-stack' else 1.4);set_lighting();scene=bpy.context.scene;scene.render.resolution_x=1440;scene.render.resolution_y=840
render(OUT/(GROUP+'-current.png'));scene.render.resolution_percentage=40;render(OUT/(GROUP+'-current-small.png'));scene.render.resolution_percentage=100
# Additional uncropped counterpart keeps main direction but fits evaluated geometry.
view=camera.matrix_world.inverted();points=[view @ (o.matrix_world @ v.co) for o in objects for v in o.evaluated_get(graph).data.vertices];aspect=scene.render.resolution_x/scene.render.resolution_y
camera.data.ortho_scale=max(2*max(abs(v.x) for v in points),2*max(abs(v.y) for v in points)*aspect)*(1.3 if GROUP=='siege-wall' else 1.08)
render(OUT/(GROUP+'-current-fit.png'));scene.render.resolution_percentage=40;render(OUT/(GROUP+'-current-fit-small.png'));scene.render.resolution_percentage=100
center=(Vector(bounds[0])+Vector(bounds[1]))/2;span=max(Vector(bounds[1])-Vector(bounds[0]));camera.location=center+Vector((-1.35,2,1.3))*span;camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();render(OUT/(GROUP+'-current-reverse.png'))
proof={'actual_bounds':bounds,'legacy_camera_bounds':original_bounds,'maximum_bounds_delta':max(abs(bounds[i][j]-original_bounds[i][j]) for i in range(2) for j in range(3)),'main_camera':'Legacy direction(1.35,-2,1.3), legacy bounds, orthographic factor2.2 stack/1.4 others,1440x840. Shared24sample diffuse renderer.','reverse_camera':'New supplemental positive-elevation reverse view, not an old composition.','limits':'Offline current baseline only, exact selected legacy placements. No terrain, collision, client lighting, other scene objects or effects. No candidate geometry.','images':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob(GROUP+'-current*.png')}};(OUT/(GROUP+'-render-proof.json')).write_text(json.dumps(proof,indent=2)+'\n')
