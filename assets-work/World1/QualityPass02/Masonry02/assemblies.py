"""Actual unchanged well/pot placement transforms, including every isolated pot placement."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector,Matrix,Euler
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'StaticBatch01'))
from review_scene import mesh_bounds,set_camera,set_lighting,render

def placement(name,index):
    return dict(name=name,index=index,**json.loads((ROOT/name/'placements.json').read_text())[index])

def group(label,records,scale_reference=False):
    anchor=Vector(records[0]['position']);out=ROOT/'review-assemblies'/label;out.mkdir(parents=True,exist_ok=True)
    (out/'placements.json').write_text(json.dumps(dict(records=records,scale_reference=190 if scale_reference else None),indent=2))
    for stage in ('current','candidate'):
        bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.world=bpy.data.worlds.new('ReviewWorld');meshes=[]
        for record in records:
            folder=ROOT/record['name'];path=folder/('baseline/source.blend' if stage=='current' else 'validation/reimported.blend')
            with bpy.data.libraries.load(str(path)) as (source,loaded):loaded.objects=source.objects
            for obj in loaded.objects:
                if obj.type not in ('ARMATURE','MESH') or obj.get('mu_helper'):continue
                bpy.context.scene.collection.objects.link(obj)
                if obj.type=='MESH':meshes.append(obj)
                else:
                    rot=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix().to_4x4()
                    obj.matrix_world=Matrix.Translation(Vector(record['position'])-anchor)@rot@Matrix.Scale(record['scale'],4)
        bpy.context.scene.frame_set(0);bpy.context.view_layer.update()
        if stage=='current':
            bounds=mesh_bounds(meshes)
            if scale_reference:bounds[1][0]+=25;bounds[1][2]=max(bounds[1][2],bounds[0][2]+190)
        if scale_reference:
            bpy.ops.mesh.primitive_cube_add(size=1,location=(bounds[1][0],bounds[0][1],bounds[0][2]+95))
            bar=bpy.context.object;bar.name='REVIEW_ONLY_190_UNIT_REFERENCE';bar.scale=(3,3,190)
            mat=bpy.data.materials.new('ReviewScale');mat.diffuse_color=(.45,.49,.5,1);bar.data.materials.append(mat)
        set_camera(bounds,1.65);set_lighting();bpy.context.scene.cycles.samples=16
        render(out/(stage+'.png'))
        bpy.context.scene.render.resolution_percentage=40;render(out/(stage+'-small.png'))
        camera=bpy.context.scene.camera;center=(Vector(bounds[0])+Vector(bounds[1]))/2
        camera.location=center+Vector((-1.35,2,1.3))*max(Vector(bounds[1])-Vector(bounds[0]));camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        bpy.context.scene.render.resolution_percentage=100;render(out/(stage+'-reverse.png'))

if __name__=='__main__':
    group('house-stack',[placement('HouseEtc01',30),placement('HouseEtc01',31)])
    group('house-adjacent',[placement('HouseEtc01',32),placement('HouseEtc01',38)])
