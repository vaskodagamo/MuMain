"""Remodel measured broadleaf ribbons into smooth arcs with a central leaf rib."""
from pathlib import Path
import json
import math
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
WORLD = ROOT.parents[1]
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(WORLD/'StaticBatch01'))
from curves import long_arc as evaluate
from geometry import collection,material,preserve_original
from review_scene import mesh_bounds

NAMES = ('Grass05','Grass06')
RIB_DEPTH = 6.0


def components(obj):
    adjacent = {v.index:set() for v in obj.data.vertices}
    for edge in obj.data.edges:
        a,b=edge.vertices
        adjacent[a].add(b);adjacent[b].add(a)
    unseen = set(adjacent)
    while unseen:
        pending=[unseen.pop()];group=[]
        while pending:
            index=pending.pop();group.append(index)
            extra=adjacent[index]&unseen
            unseen-=extra;pending.extend(extra)
        yield group


def leaf_rows(obj,indices):
    coordinates={obj.data.loops[i].vertex_index:obj.data.uv_layers[0].data[i].uv.copy()
                 for face in obj.data.polygons for i in face.loop_indices}
    rows={}
    for index in indices:
        uv=coordinates[index]
        rows.setdefault(round(uv.y,4),[]).append((uv.x,obj.data.vertices[index].co.copy()))
    assert all(len(row)==2 for row in rows.values()),[len(row) for row in rows.values()]
    times=sorted(rows)
    ordered=[sorted(rows[t],key=lambda item:item[0]) for t in times]
    return times,ordered


def ribbon(obj,indices,vertices,uvs,faces,bones,bounds):
    bounds=[[min(obj.data.vertices[i].co[k] for i in indices) for k in range(3)],
            [max(obj.data.vertices[i].co[k] for i in indices) for k in range(3)]]
    times,rows=leaf_rows(obj,indices)
    left,right=[[row[side][1] for row in rows] for side in (0,1)]
    samples=sorted(set(times+[times[0]+(times[-1]-times[0])*i/8 for i in range(9)]))
    bone=obj.vertex_groups[obj.data.vertices[indices[0]].groups[0].group].name
    assert all(obj.vertex_groups[obj.data.vertices[i].groups[0].group].name==bone for i in indices)
    base=len(vertices)
    for row,t in enumerate(samples):
        a,b=evaluate(times,left,t),evaluate(times,right,t)
        previous=max(times[0],t-.001);following=min(times[-1],t+.001)
        tangent=evaluate(times,left,following)-evaluate(times,left,previous)
        normal=(b-a).cross(tangent).normalized()
        fade=math.sin(math.pi*(t-times[0])/(times[-1]-times[0]))
        center=(a+b)/2+normal*RIB_DEPTH*fade
        center=Vector([min(max(center[k],bounds[0][k]),bounds[1][k]) for k in range(3)])
        vertices.extend((a,center,b));bones.extend([bone]*3)
        low,high=rows[0][0][0],rows[0][1][0]
        uvs.extend(((low,t),((low+high)/2,t),(high,t)))
        if row:
            for column in range(2):
                p=base+(row-1)*3+column;q=p+3
                faces.extend(((p,p+1,q+1),(q+1,q,p)))
    return dict(bone=bone,original_sections=len(times),new_sections=len(samples))


def create_mesh(source,rig,target,surface):
    vertices,uvs,faces,bones=[],[],[],[]
    local_bounds=[[min(v.co[k] for v in source.data.vertices) for k in range(3)],
                  [max(v.co[k] for v in source.data.vertices) for k in range(3)]]
    records=[ribbon(source,group,vertices,uvs,faces,bones,local_bounds) for group in components(source)]
    assert len(records)==16,len(records)
    data=bpy.data.meshes.new('CurvedBroadleaf');data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new('CurvedBroadleaf',data);target.objects.link(obj)
    obj.matrix_world=source.matrix_world.copy();obj.parent=rig
    obj.modifiers.new('OriginalRig','ARMATURE').object=rig
    data.materials.append(surface);layer=data.uv_layers.new(name='UVMap')
    for face in data.polygons:
        face.use_smooth=True
        for loop in face.loop_indices:layer.data[loop].uv=uvs[data.loops[loop].vertex_index]
    for bone in rig['mu_bone_order']:
        obj.vertex_groups.new(name=bone).add([i for i,value in enumerate(bones) if value==bone],1,'REPLACE')
    return obj,records


def build(name):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    bpy.context.scene.frame_set(0)
    rig,reference=preserve_original()
    source=next(iter(reference.objects))
    surface=material(folder/'textures/tree_09.tga')
    shader=surface.node_tree.nodes.get('Principled BSDF')
    texture=next(n for n in surface.node_tree.nodes if n.type=='TEX_IMAGE')
    surface.node_tree.links.new(texture.outputs['Alpha'],shader.inputs['Alpha'])
    obj,records=create_mesh(source,rig,collection('EXPORT_'+name),surface)
    before,after=mesh_bounds([source]),mesh_bounds([obj])
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.001,(before,after)
    assert all(face.area>1e-8 for face in obj.data.polygons)
    anchors=[]
    for group in components(source):
        times,rows=leaf_rows(source,group)
        anchors.extend(point for row in (rows[0],rows[-1]) for u,point in row)
    retained = max(min((v - w.co).length for w in obj.data.vertices) for v in anchors)
    assert retained < .0001, retained
    per_root={}
    for bone in rig['mu_bone_order']:
        def bone_bounds(mesh):
            pts=[v.co for v in mesh.data.vertices if mesh.vertex_groups[v.groups[0].group].name==bone]
            return [[min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]]
        old,new=bone_bounds(source),bone_bounds(obj)
        assert max(abs(a-b) for aa,bb in zip(old,new) for a,b in zip(aa,bb))<.0001,(bone,old,new)
        per_root[bone]=dict(before=old,after=new)
    report=dict(per_root_bounds=per_root,retained_root_tip_vertices=len(anchors), max_control_vertex_distance=retained,
        control_contract='Root and tip edge controls exact; directional extrema preserved, intermediary controls redesigned', triangles=len(obj.data.polygons),bounds_before=before,bounds_after=after,
        bone_order=list(rig['mu_bone_order']),actions=rig['mu_action_meta'].to_dict(),leaves=records,
        texture='tree_09.OZT frozen,256x512 RGBA',client_verified=False)
    (folder/'validation/source.json').write_text(json.dumps(report,indent=2)+'\n')
    for image in bpy.data.images:
        if image.source=='FILE':image.pack()
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    print(name,report['triangles'],'triangles')


if __name__=='__main__':
    for name in NAMES:build(name)
