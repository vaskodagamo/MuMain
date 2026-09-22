"""Refine pottery from measured profiles while retaining rig and untouched components."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT.parents[1]/'StaticBatch01'))
from geometry import collection,preserve_original
from review_scene import mesh_bounds
from vessels import connected,original_profiles,vessel
from well_geometry import basin
from roof_boards import roof
NAMES=('Well03','Well04','Well02','Well01')
CANONICAL=original_profiles(ROOT/'Well03/validation/original/Well03.smd')

def remodel(source,profiles,target,rig,shared_profile):
    groups=list(connected(source.data.vertices,[list(f.vertices) for f in source.data.polygons]))
    bodies=[];extras=[];removed_faces=set()
    for group in groups:
        v=source.data.vertices[next(iter(group))];bone=source.vertex_groups[v.groups[0].group].name
        if bone in profiles and len(group)==76:
            neck_v=profiles[bone]['levels'][-2][0];neck={}
            for face in source.data.polygons:
                if face.vertices[0] not in group:continue
                values=[source.data.uv_layers[0].data[i].uv for i in face.loop_indices]
                if max(uv.y for uv in values)>neck_v+.00001:removed_faces.add(face.index)
                for loop,uv in zip(face.loop_indices,values):
                    if abs(uv.y-neck_v)<.00001:
                        index=source.data.loops[loop].vertex_index
                        neck[index]=(source.matrix_world@source.data.vertices[index].co,uv.copy())
            assert len(neck)==10,(bone,len(neck))
            bodies.append((bone,group,list(neck.values())))
        if bone=='Cylinder04' and len(group)==32:
            vs,fs,uv=basin([source.data.vertices[i].co for i in group])
            extras.append((vs,fs,uv,'stone_coping'))
            removed_faces.update(f.index for f in source.data.polygons if f.vertices[0] in group)
        if bone=='Cylinder04' and len(group)==12:
            vs,fs,uv,removed_roof=roof(source,group)
            extras.append((vs,fs,uv,'roof_board_joints'));removed_faces.update(removed_roof)
    removed=set()
    vertices=[];bones=[];faces=[];uvs=[];materials=[];normals=[];smooth=[];lookup={}
    for face in source.data.polygons:
        if face.vertices[0] in removed or face.index in removed_faces:continue
        indices=[];uv=[];ns=[]
        for loop in face.loop_indices:
            index=source.data.loops[loop].vertex_index
            if index not in lookup:
                lookup[index]=len(vertices);v=source.data.vertices[index];vertices.append(v.co.copy());bones.append(source.vertex_groups[v.groups[0].group].name)
            indices.append(lookup[index]);uv.append(source.data.uv_layers[0].data[loop].uv.copy());ns.append(source.data.corner_normals[loop].vector.copy())
        faces.append(indices);uvs.append(uv);normals.append(ns);materials.append(face.material_index);smooth.append(face.use_smooth)
    retained_count=len(faces)
    records=[]
    for bone,group,neck in bodies:
        world=[source.matrix_world@source.data.vertices[i].co for i in group]
        canonical=next(p for p in CANONICAL.values() if abs(p['levels'][0][0]-profiles[bone]['levels'][0][0])<.00001) if shared_profile else profiles[bone]
        vs,fs,coords,record=vessel(profiles[bone],world,neck,canonical)
        base=len(vertices);vertices.extend(source.matrix_world.inverted()@v for v in vs);bones.extend([bone]*len(vs))
        material=next(i for i,m in enumerate(source.data.materials) if m.get('mu_texture')=='jar_01.jpg')
        for face in fs:
            faces.append([base+i for i in face]);uv=[list(coords[i]) for i in face]
            if max(p[0] for p in uv)-min(p[0] for p in uv)>.5:
                for p in uv:
                    if p[0]<.75:p[0]+=1
            uvs.append(uv);normals.append(None);materials.append(material);smooth.append(True)
        records.append(dict(bone=bone,kind='vessel',**record))
    for vs,fs,coords,kind in extras:
        base=len(vertices);vertices.extend(vs);bones.extend(['Cylinder04']*len(vs))
        material=next(i for i,m in enumerate(source.data.materials) if m.get('mu_texture')=='well.jpg')
        for face in fs:
            faces.append([base+i for i in face]);uv=[list(coords[i]) for i in face]
            if kind=='stone_coping' and max(p[0] for p in uv)-min(p[0] for p in uv)>.5:
                for p in uv:
                    if p[0]<.75:p[0]+=1
            uvs.append(uv);normals.append(None);materials.append(material);smooth.append(False)
        records.append(dict(kind=kind,bone='Cylinder04',triangles=len(fs)))
    data=bpy.data.meshes.new(source.name+'_Refined');data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new(source.name+'_Refined',data);target.objects.link(obj)
    obj.matrix_world=source.matrix_world.copy();obj.parent=rig;obj.modifiers.new('OriginalRig','ARMATURE').object=rig
    for original in source.data.materials:
        material=original.copy();material.name=original.get('mu_texture') or original.name.removeprefix('REF_');data.materials.append(material)
    for bone in rig['mu_bone_order']:
        obj.vertex_groups.new(name=bone).add([i for i,b in enumerate(bones) if b==bone],1,'REPLACE')
    layer=data.uv_layers.new(name='UVMap')
    for face,uv,material,is_smooth in zip(data.polygons,uvs,materials,smooth):
        face.material_index=material;face.use_smooth=is_smooth
        for loop,value in zip(face.loop_indices,uv):layer.data[loop].uv=value
    data.update();custom=[]
    for face,original in zip(data.polygons,normals):
        custom.extend(original if original else [data.corner_normals[i].vector.copy() for i in face.loop_indices])
    data.normals_split_custom_set(custom)
    for index in range(retained_count):
        face=data.polygons[index]
        assert face.material_index==materials[index]
        for loop,point,uv in zip(face.loop_indices,faces[index],uvs[index]):
            assert (data.vertices[data.loops[loop].vertex_index].co-vertices[point]).length<1e-7
            assert (layer.data[loop].uv-Vector(uv)).length<1e-7
    obj['mu_retained_triangles']=retained_count
    return obj,records

def build(name):
    folder=ROOT/name;bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'));bpy.context.scene.frame_set(0)
    profiles=original_profiles(folder/'validation/original'/f'{name}.smd')
    rig,reference=preserve_original();target=collection('EXPORT_'+name);objects=[];records=[]
    for source in reference.objects:
        obj,parts=remodel(source,profiles,target,rig,name in ('Well01','Well03'));objects.append(obj);records.extend(parts)
    before,after=mesh_bounds(list(reference.objects)),mesh_bounds(objects)
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.0001,(before,after)
    triangles=sum(len(obj.data.polygons) for obj in objects);assert triangles<=1500,triangles
    assert sum(r['kind']=='vessel' for r in records)==(8 if name=='Well04' else 0 if name=='Well02' else 4),records
    assert all(face.area>1e-8 for obj in objects for face in obj.data.polygons)
    per_root={}
    for bone in rig['mu_bone_order']:
        def points(meshes):
            return [obj.matrix_world@v.co for obj in meshes for v in obj.data.vertices if obj.vertex_groups[v.groups[0].group].name==bone]
        old,new=points(reference.objects),points(objects)
        if not old:continue
        low_high=lambda pts:[[min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]]
        old_bounds,new_bounds=low_high(old),low_high(new)
        delta=max(abs(a-b) for aa,bb in zip(old_bounds,new_bounds) for a,b in zip(aa,bb))
        contacts=[p for p in old if abs(p.z-old_bounds[0][2])<.0001]
        ground_error=max(min((a-b).length for b in new) for a in contacts)
        assert delta<.0001 and ground_error<.0001,(name,bone,delta,ground_error)
        per_root[bone]=dict(before=old_bounds,after=new_bounds,ground_contacts=len(contacts),maximum_ground_contact_error=ground_error)
    report=dict(per_root=per_root,retained_triangles=sum(obj['mu_retained_triangles'] for obj in objects),triangles=triangles,bounds_before=before,bounds_after=after,parts=records,
        bone_order=list(rig['mu_bone_order']),actions=rig['mu_action_meta'].to_dict(),textures='Frozen',client_verified=False)
    (folder/'validation/source.json').write_text(json.dumps(report,indent=2))
    bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    print(name,triangles,flush=True)
if __name__=='__main__':
    for name in (sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else NAMES):build(name)
