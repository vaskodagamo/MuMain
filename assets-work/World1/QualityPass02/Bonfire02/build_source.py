"""Refine exposed log sections while retaining the complete additive shell."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT.parents[1]/'StaticBatch01'))
sys.path.insert(0,str(ROOT.parent/'Wells02'))
from geometry import collection,preserve_original
from review_scene import mesh_bounds
from vessels import connected
from log_geometry import log_surfaces
from audit_exports import triangles


def original_effect_normals(source,face):
    points=[source.matrix_world@source.data.vertices[i].co for i in face.vertices]
    uvs=[source.data.uv_layers[0].data[i].uv for i in face.loop_indices]
    baseline=triangles(ROOT/'Bonfire01/validation/baseline/Bonfire01.smd')
    for material,rows in baseline:
        if material!='fire_02.jpg':continue
        for shift in range(3):
            aligned=[rows[(j+shift)%3] for j in range(3)]
            if all((p-Vector(r[1:4])).length<.0001 and (uv-Vector(r[7:9])).length<.000001 for p,uv,r in zip(points,uvs,aligned)):
                matrix=source.matrix_world.to_3x3().transposed()
                return [(matrix@Vector(r[4:7])).normalized() for r in aligned]
    raise AssertionError(('No original effect corner match',face.index))


def gather(source):
    vertices=[];faces=[];uvs=[];materials=[];normals=[];smooth=[];lookup={};anchors=[]
    def add(points,uv,material,shading,original=None):
        indices=[]
        for p in points:
            key=tuple(round(v,6) for v in p)+(material,)
            if key not in lookup:lookup[key]=len(vertices);vertices.append(p.copy())
            indices.append(lookup[key])
        faces.append(indices);uvs.append(uv);materials.append(material);smooth.append(shading);normals.append(original)
    wood_faces=[f for f in source.data.polygons if f.material_index==0]
    wood_ids={i for f in wood_faces for i in f.vertices}
    groups=[g for g in connected(source.data.vertices,[list(f.vertices) for f in wood_faces]) if g<=wood_ids]
    assert len(groups)==6,[len(g) for g in groups]
    for group in groups:
        assert len(group)==8,len(group)
        surfaces,anchor=log_surfaces(source,group);anchors.append(anchor)
        for points,uv,shading in surfaces:add(points,uv,0,shading)
    for f in source.data.polygons:
        if f.material_index!=1:continue
        points=[source.data.vertices[i].co for i in f.vertices]
        uv=[source.data.uv_layers[0].data[i].uv.copy() for i in f.loop_indices]
        original=original_effect_normals(source,f)
        add(points,uv,1,f.use_smooth,original)
    return vertices,faces,uvs,materials,normals,smooth,anchors


def make_mesh(source,target,rig):
    vertices,faces,uvs,materials,normals,smooth,anchors=gather(source)
    data=bpy.data.meshes.new('SplitWoodAndProtectedFire');data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new('Bonfire01',data);target.objects.link(obj)
    obj.matrix_world=source.matrix_world.copy();obj.parent=rig;obj.modifiers.new('OriginalRig','ARMATURE').object=rig
    for old in source.data.materials:
        material=old.copy();material.name=old.get('mu_texture');data.materials.append(material)
    for name in rig['mu_bone_order']:obj.vertex_groups.new(name=name)
    obj.vertex_groups['Bone01'].add(list(range(len(vertices))),1,'REPLACE')
    layer=data.uv_layers.new(name='UVMap')
    for face,uv,material,shading in zip(data.polygons,uvs,materials,smooth):
        face.material_index=material;face.use_smooth=shading
        for loop,value in zip(face.loop_indices,uv):layer.data[loop].uv=value
    data.update();custom=[]
    for face,original in zip(data.polygons,normals):
        custom.extend(original if original else [data.corner_normals[i].vector.copy() for i in face.loop_indices])
    data.normals_split_custom_set(custom)
    assert all(f.area>1e-8 for f in data.polygons)
    assert all(min((p-v.co).length for v in data.vertices)<1e-6 for p in anchors)
    return obj,anchors


def main():
    folder=ROOT/'Bonfire01';bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'));bpy.context.scene.frame_set(0)
    rig,reference=preserve_original();target=collection('EXPORT_Bonfire01')
    assert len(reference.objects)==1
    source=next(iter(reference.objects));obj,anchors=make_mesh(source,target,rig)
    before,after=mesh_bounds([source]),mesh_bounds([obj])
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.0001,(before,after)
    report=dict(bounds_before=before,bounds_after=after,triangles=len(obj.data.polygons),protected_effect_triangles=38,
        wood_log_lowest_contacts=[list(p) for p in anchors],ground_contact_error=0,
        bone_order=list(rig['mu_bone_order']),actions=rig['mu_action_meta'].to_dict(),textures='Frozen',client_verified=False)
    (folder/'validation/source.json').write_text(json.dumps(report,indent=2))
    bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    print(report,flush=True)
if __name__=='__main__':main()
