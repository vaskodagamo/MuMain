"""Bowed growing ribbons and scalloped leafy boughs, preserving rooted cluster extents."""
import math
import json
from pathlib import Path
import sys

sys.dont_write_bytecode=True
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
NAMES=('Tree09','Tree10','Grass03','Grass04')
LEVELS=(0,.22,.48,.75,1)
ROOT_CHORD=1.30
TIP_CHORD=.72
BEND_VARIATION=.025
DOME_RISE=5.0
DOME_OFFSETS=((6,-4),(-5,4),(4,5),(-4,-3))


def model_mesh():
    return next(obj for obj in bpy.context.scene.objects if obj.type=='MESH' and not obj.get('mu_helper') and not obj.get('mu_reference'))


def snapshot_reference(obj):
    collection=bpy.data.collections.new('REF_BASELINE')
    bpy.context.scene.collection.children.link(collection)
    collection.hide_render=collection.hide_viewport=True
    clone=obj.copy(); clone.data=obj.data.copy()
    clone['mu_reference']=True
    collection.objects.link(clone)


def corner(obj,loop):
    vertex=obj.data.vertices[obj.data.loops[loop].vertex_index]
    assert len(vertex.groups)==1
    return (obj.matrix_world@vertex.co,obj.data.uv_layers[0].data[loop].uv.copy(),vertex.groups[0].group)


def bound(points):
    return [[min(p[a] for p in points) for a in range(3)],[max(p[a] for p in points) for a in range(3)]]


def clamp(point,limits):
    return Vector([min(limits[1][a],max(limits[0][a],point[a])) for a in range(3)])


def triangle(output,points,uvs,bone,material):
    if (points[1]-points[0]).cross(points[2]-points[0]).length<.00001: return
    output.append((points,uvs,bone,material))


def quad(output,points,uvs,bone,material):
    triangle(output,points[:3],uvs[:3],bone,material)
    triangle(output,[points[0],points[2],points[3]],[uvs[0],uvs[2],uvs[3]],bone,material)


def tall_tuft(output,points,bone,material):
    unique={tuple(p):uv for p,uv in points}
    roots=[Vector(p) for p,uv in unique.items() if uv.y<.01]
    tips=[Vector(p) for p,uv in unique.items() if uv.y>.99]
    limits=bound([Vector(p) for p in unique])
    center=sum(roots,Vector())/len(roots)
    tips.sort(key=lambda p:math.atan2(p.y-center.y,p.x-center.x))
    roots.sort(key=lambda p:math.atan2(p.y-center.y,p.x-center.x))
    for index,tip in enumerate(tips):
        root=roots[index%len(roots)]
        start_a=root.copy()
        start_b=root.lerp(center,ROOT_CHORD); start_b.z=root.z
        canopy=sum(tips,Vector())/len(tips)
        end_a=tip.copy()
        end_b=tip.lerp(canopy,TIP_CHORD); end_b.z=tip.z
        tangent=Vector((-(tip-center).y,(tip-center).x,0)).normalized()
        rings=[]
        for t in LEVELS:
            # A continuous growing arc opens smoothly; there is no waist ring.
            a=start_a.lerp(end_a,t); b=start_b.lerp(end_b,t)
            bow=math.sin(math.pi*t)*(1+BEND_VARIATION*bone)
            a+=tangent*bow*(3 if index%2 else -3)
            b-=tangent*bow*(3 if index%2 else -3)
            rings.append((clamp(a,limits),clamp(b,limits)))
        for row in range(len(LEVELS)-1):
            a,b=rings[row]; c,d=rings[row+1]
            quad(output,[a,b,d,c],[Vector((0,LEVELS[row])),Vector((1,LEVELS[row])),Vector((1,LEVELS[row+1])),Vector((0,LEVELS[row+1]))],bone,material)


def build_tall(obj):
    groups={}
    for face in obj.data.polygons:
        for loop in face.loop_indices:
            p,uv,bone=corner(obj,loop)
            groups.setdefault(bone,[]).append((p,uv))
    output=[]
    for bone,points in groups.items(): tall_tuft(output,points,bone,0)
    return output


def leafy_shell(output,points,uvs,bone,material,index):
    center,a,b=points
    uv_center,uv_a,uv_b=uvs
    layer=(index//6)%4
    dx,dy=DOME_OFFSETS[layer]
    core=center.copy()
    core.x+=dx; core.y+=dy
    core.z=center.z-2.0
    middle_a=core.lerp(a,.5); middle_b=core.lerp(b,.5)
    middle_a.z+=DOME_RISE; middle_b.z+=DOME_RISE
    uv_middle_a=uv_center.lerp(uv_a,.5)
    uv_middle_b=uv_center.lerp(uv_b,.5)
    triangle(output,[core,middle_a,middle_b],[uv_center,uv_middle_a,uv_middle_b],bone,material)
    quad(output,[middle_a,a,b,middle_b],[uv_middle_a,uv_a,uv_b,uv_middle_b],bone,material)


def bowed_card(output,polygon,bone,material,limits):
    polygon.sort(key=lambda item:(round(item[1].y,3),item[1].x))
    (a,ua),(b,ub),(c,uc),(d,ud)=polygon
    # Bottom and top edge anchors stay fixed, while stems bow out of the old flat cards.
    normal=(b-a).cross(c-a).normalized()
    span=max((c-a).length,(d-b).length)
    rings=[]
    for t in LEVELS:
        offset=normal*math.sin(math.pi*t)*span*.10
        rings.append((clamp(a.lerp(c,t)+offset,limits),clamp(b.lerp(d,t)+offset,limits)))
    for row in range(len(LEVELS)-1):
        p,q=rings[row]; r,s=rings[row+1]
        v,w=LEVELS[row],LEVELS[row+1]
        quad(output,[p,q,s,r],[ua.lerp(uc,v),ub.lerp(ud,v),ub.lerp(ud,w),ua.lerp(uc,w)],bone,material)


def inner_boughs(output,points,bone):
    # Three swept, cupped foliage boughs connect formerly detached canopy levels.
    limits=bound(points)
    center=Vector([(a+b)/2 for a,b in zip(*limits)])
    width=min(limits[1][0]-limits[0][0],limits[1][1]-limits[0][1])*.64
    low=limits[0][2]+9
    high=limits[0][2]+(limits[1][2]-limits[0][2])*.84
    for leaf in range(3):
        angle=leaf*math.tau/3+.21*bone
        axis=Vector((math.cos(angle),math.sin(angle),0))
        tangent=Vector((-axis.y,axis.x,0))
        grid=[]
        for row in range(5):
            v=row/4
            line=[]
            for column in range(5):
                u=column/4
                point=center+tangent*((u-.5)*width)
                point+=axis*(math.sin(v*math.pi)*width*.19+(2*u-1)**2*width*.16)
                point.z=low+(high-low)*v+math.sin(math.pi*u)*math.sin(math.pi*v)*5
                line.append(clamp(point,limits))
            grid.append(line)
        for row in range(4):
            for col in range(4):
                quad(output,[grid[row][col],grid[row][col+1],grid[row+1][col+1],grid[row+1][col]],
                     [Vector((col/4,row/4)),Vector(((col+1)/4,row/4)),Vector(((col+1)/4,(row+1)/4)),Vector((col/4,(row+1)/4))],bone,0)


def build_scrub(obj):
    output=[]; cards={}; by_bone={}
    for face in obj.data.polygons:
        data=[corner(obj,i) for i in face.loop_indices]
        bone=data[0][2]
        by_bone.setdefault(bone,[]).extend(p for p,uv,b in data)
    for index,face in enumerate(obj.data.polygons):
        data=[corner(obj,i) for i in face.loop_indices]
        points=[item[0] for item in data]; uvs=[item[1] for item in data]; bone=data[0][2]
        if face.material_index==0:
            leafy_shell(output,points,uvs,bone,0,index)
        else:
            # Imported quads are two consecutive triangles in this preserved family.
            key=(bone,(index-len([f for f in obj.data.polygons if f.material_index==0]))//2)
            cards.setdefault(key,{}).update({tuple(p):(p,uv) for p,uv,b in data})
    for (bone,index),points in cards.items():
        assert len(points)==4
        bowed_card(output,list(points.values()),bone,1,bound(by_bone[bone]))
    for bone,points in by_bone.items(): inner_boughs(output,points,bone)
    return output


def replace_mesh(obj,triangles):
    old=obj.data
    group_names=[group.name for group in obj.vertex_groups]
    mesh=bpy.data.meshes.new('OrganicRibbons')
    points=[obj.matrix_world.inverted()@point for corners,uvs,bone,mat in triangles for point in corners]
    mesh.from_pydata(points,[],[(i,i+1,i+2) for i in range(0,len(points),3)])
    for mat in old.materials: mesh.materials.append(mat)
    layer=mesh.uv_layers.new(name=old.uv_layers[0].name)
    obj.data=mesh
    obj.vertex_groups.clear()
    for name in group_names: obj.vertex_groups.new(name=name)
    for face,(corners,uvs,bone,material) in zip(mesh.polygons,triangles):
        face.material_index=material
        for loop,uv in zip(face.loop_indices,uvs): layer.data[loop].uv=uv
        obj.vertex_groups[bone].add(list(face.vertices),1,'REPLACE')
    mesh.update()


def build(name):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    obj=model_mesh(); snapshot_reference(obj)
    reference=bpy.data.collections.new('REF_ORIGINAL')
    bpy.context.scene.collection.children.link(reference)
    reference.hide_render=reference.hide_viewport=True
    with bpy.data.libraries.load(str(folder/'original/source.blend')) as (data,loaded): loaded.objects=data.objects
    for item in loaded.objects:
        if item.type=='MESH' and not item.get('mu_helper'):
            item.parent=None; item.modifiers.clear(); item['mu_reference']=True
            reference.objects.link(item)
    before=bound([obj.matrix_world@v.co for v in obj.data.vertices])
    triangles=build_tall(obj) if name.startswith('Tree') else build_scrub(obj)
    replace_mesh(obj,triangles)
    after=bound([obj.matrix_world@v.co for v in obj.data.vertices])
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.002,(name,before,after)
    bpy.ops.file.pack_all()
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(triangles),bounds_before=before,bounds_after=after,geometry_only=True),indent=2)+'\n')


if __name__=='__main__':
    import os
    for name in os.environ.get('SCRUB_NAMES',','.join(NAMES)).split(','): build(name)
