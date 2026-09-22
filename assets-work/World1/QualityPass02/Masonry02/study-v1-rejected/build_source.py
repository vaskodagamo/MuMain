"""Carve the dragon's interior face while freezing the complete block and perimeter band."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector,geometry
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT.parents[1]/'StaticBatch01'))
from geometry import collection,preserve_original
from review_scene import mesh_bounds
from face_design import sculpture
from audit_exports import triangles


def block_faces(source):
    baseline=triangles(ROOT/'HouseEtc01/validation/baseline/HouseEtc01.smd');records=[]
    for face in source.data.polygons:
        if source.data.materials[face.material_index].get('mu_texture')!='c_wall04.jpg':continue
        points=[source.data.vertices[i].co.copy() for i in face.vertices]
        uv=[source.data.uv_layers[0].data[i].uv.copy() for i in face.loop_indices];normal=None
        for material,rows in baseline:
            if material!='c_wall04.jpg':continue
            for shift in range(3):
                aligned=[rows[(j+shift)%3] for j in range(3)]
                if all((source.matrix_world@p-Vector(r[1:4])).length<.0001 and (u-Vector(r[7:9])).length<1e-6 for p,u,r in zip(points,uv,aligned)):
                    normal=[(source.matrix_world.to_3x3().transposed()@Vector(r[4:7])).normalized() for r in aligned]
        assert normal is not None,face.index
        records.append((points,uv,normal,face.use_smooth))
    assert len(records)==24
    return records


def border_normal(uv,source,scale):
    margin=min((uv.x-.0005)*scale[0],(.9995-uv.x)*scale[0],(uv.y-.0005)*scale[1],(.9995-uv.y)*scale[1])
    if margin>8.00001:return None
    for material,rows in triangles(ROOT/'HouseEtc01/validation/baseline/HouseEtc01.smd'):
        if material!='c_wall06.jpg':continue
        coords=[Vector((*r[7:9],0)) for r in rows]
        weights=geometry.barycentric_transform(Vector((*uv,0)),*coords,Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
        if min(weights)>=-1e-5:
            normal=sum((Vector(r[4:7])*w for r,w in zip(rows,weights)),Vector()).normalized()
            return (source.matrix_world.to_3x3().transposed()@normal).normalized()
    raise AssertionError(('border normal registration',list(uv)))


def carved_normals(data,source,design,layer,originals):
    adjacency={v.index:[] for v in data.vertices}
    for face in data.polygons:
        if face.material_index==1:
            for vertex in face.vertices:adjacency[vertex].append(face)
    custom=[];crease_cosine=math.cos(math.radians(40))
    sx,sy=design['scale']
    for face,original in zip(data.polygons,originals):
        if original:custom.extend(original);continue
        margins=[min((layer.data[i].uv.x-.0005)*sx,(.9995-layer.data[i].uv.x)*sx,(layer.data[i].uv.y-.0005)*sy,(.9995-layer.data[i].uv.y)*sy) for i in face.loop_indices]
        protected=max(margins)<=8.0001
        for loop in face.loop_indices:
            if protected:
                custom.append(border_normal(layer.data[loop].uv,source,design['scale']));continue
            vertex=data.loops[loop].vertex_index
            neighbors=[f for f in adjacency[vertex] if f.normal.dot(face.normal)>=crease_cosine]
            normal=sum((f.normal*f.area for f in neighbors),Vector()).normalized()
            assert normal.dot(face.normal)>0
            custom.append(normal)
    return custom


def make_mesh(source,rig,target):
    positions,face_indices,coordinates,design=sculpture(source)
    faces=list(face_indices);uvs=[[coordinates[i] for i in f] for f in faces]
    materials=[1]*len(faces);normals=[None]*len(faces);smooth=[True]*len(faces)
    for points,uv,normal,shading in block_faces(source):
        offset=len(positions);positions.extend(points);faces.append([offset,offset+1,offset+2])
        uvs.append(uv);materials.append(0);normals.append(normal);smooth.append(shading)
    order=sorted(range(len(faces)),key=lambda i:materials[i])
    faces,uvs,materials,normals,smooth=[[values[i] for i in order] for values in (faces,uvs,materials,normals,smooth)]
    data=bpy.data.meshes.new('CarvedDragonAndFrozenBlock');data.from_pydata(positions,[],faces);data.update()
    obj=bpy.data.objects.new('HouseEtc01',data);target.objects.link(obj);obj.parent=rig;obj.matrix_world=source.matrix_world.copy()
    obj.modifiers.new('OriginalRig','ARMATURE').object=rig
    for old in source.data.materials:
        material=old.copy();material.name=old.get('mu_texture');data.materials.append(material)
    obj.vertex_groups.new(name='Box01').add(list(range(len(positions))),1,'REPLACE')
    layer=data.uv_layers.new(name='UVMap')
    for face,uv,material,shading in zip(data.polygons,uvs,materials,smooth):
        face.material_index=material;face.use_smooth=shading
        for loop,value in zip(face.loop_indices,uv):layer.data[loop].uv=value
    data.update();data.normals_split_custom_set(carved_normals(data,source,design,layer,normals))
    assert all(f.area>1e-8 for f in data.polygons),min(f.area for f in data.polygons)
    return obj,design


def main():
    folder=ROOT/'HouseEtc01';bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'));bpy.context.scene.frame_set(0)
    rig,reference=preserve_original();target=collection('EXPORT_HouseEtc01');source=next(iter(reference.objects))
    obj,design=make_mesh(source,rig,target);before,after=mesh_bounds([source]),mesh_bounds([obj])
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.0001,(before,after)
    count=len(obj.data.polygons);assert count<=1500,count
    report=dict(bounds_before=before,bounds_after=after,triangles=count,protected_block_triangles=24,design=design,
        bone_order=list(rig['mu_bone_order']),actions=rig['mu_action_meta'].to_dict(),textures='Frozen',client_verified=False)
    (folder/'validation/source.json').write_text(json.dumps(report,indent=2))
    bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    print('HouseEtc01',count,flush=True)
if __name__=='__main__':main()
