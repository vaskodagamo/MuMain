"""Structural corbels, recessed window panels and shaped awning board courses."""
import importlib.util
import json
import math
import os
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
NAMES=('House01','House03')
WINDOW_RECESS=4.8
WINDOW_MARGIN=.025
AWNING_BOW=5.5
BOARD_SEAMS=(0,.215,.425,.625,.84,1)


def load_helper(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module

mesh_helper=load_helper('scrub_mesh',ROOT.parent/'Scrub01/build_source.py')
arch=load_helper('architecture_mesh',ROOT.parent/'Architecture01/build_source.py')


def face_data(obj,face):
    return [mesh_helper.corner(obj,i) for i in face.loop_indices]


def emit(output,points,uvs,bone,material):
    for i in range(1,len(points)-1):
        mesh_helper.triangle(output,[points[0],points[i],points[i+1]],[uvs[0],uvs[i],uvs[i+1]],bone,material)


def perimeter(obj,indices):
    edges=[];values={}
    for index in indices:
        face=obj.data.polygons[index]
        rows=face_data(obj,face)
        keys=[tuple(round(v,5) for v in p) for p,uv,bone in rows]
        values.update({key:row for key,row in zip(keys,rows)})
        edges.extend(zip(keys,keys[1:]+keys[:1]))
    edges=[e for e in edges if (e[1],e[0]) not in edges]
    order=[edges[0][0],edges[0][1]]
    while len(order)<len(edges):order.append(next(b for a,b in edges if a==order[-1]))
    return [values[key] for key in order]


def window(output,obj,indices):
    rows=perimeter(obj,indices)
    points=[p for p,u,b in rows];uvs=[u for p,u,b in rows]
    center=sum(points,Vector())/4;uv_center=sum(uvs,Vector((0,0)))/4
    normal=(points[1]-points[0]).cross(points[2]-points[0]).normalized()
    inner=[p.lerp(center,WINDOW_MARGIN*2) for p in points]
    uv_inner=[u.lerp(uv_center,WINDOW_MARGIN*2) for u in uvs]
    recessed=[p-normal*WINDOW_RECESS for p in inner]
    bone=rows[0][2];material=obj.data.polygons[indices[0]].material_index
    for i in range(4):
        j=(i+1)%4
        emit(output,[points[i],points[j],inner[j],inner[i]],[uvs[i],uvs[j],uv_inner[j],uv_inner[i]],bone,material)
        offset=Vector((.006,.006))
        emit(output,[inner[i],inner[j],recessed[j],recessed[i]],[uv_inner[i],uv_inner[j],uv_inner[j]+offset,uv_inner[i]+offset],bone,material)
    emit(output,recessed,uvs,bone,material)


def canopy_edges(obj):
    edges={}
    for face in obj.data.polygons:
        if obj.data.materials[face.material_index].name!='tile_ston07.tga':continue
        points=[p for p,u,b in face_data(obj,face)]
        for a,b in zip(points,points[1:]+points[:1]):
            key=tuple(sorted((tuple(round(x,4) for x in a),tuple(round(x,4) for x in b))))
            edges.setdefault(key,[]).append((a,b))
    return [items[0] for items in edges.values() if len(items)==1]


def distance_to_edge(point,edge):
    a,b=edge;direction=b-a
    fraction=max(0,min(1,(point-a).dot(direction)/direction.length_squared))
    return (point-a-direction*fraction).length


def awning(output,obj,face,boundary_edges):
    rows=face_data(obj,face);polygon=[(p,u) for p,u,b in rows]
    low=min(u.y for p,u in polygon);high=max(u.y for p,u in polygon)
    cuts=sorted(set([low,high]+[v for a,b in zip(BOARD_SEAMS,BOARD_SEAMS[1:]) for v in (a,(a+b)/2,b) if low<v<high]))
    for bottom,top in zip(cuts,cuts[1:]):
        cut=arch.clip_polygon(arch.clip_polygon(polygon,1,bottom,True),1,top,False)
        if len(cut)<3:continue
        for left,right in zip((0,.5),(.5,1)):
            segment=arch.clip_polygon(arch.clip_polygon(cut,0,left,True),0,right,False)
            if len(segment)<3:continue
            positions=[]
            for p,uv in segment:
                course=next((a,b) for a,b in zip(BOARD_SEAMS,BOARD_SEAMS[1:]) if a-1e-6<=uv.y<=b+1e-6)
                t=(uv.y-course[0])/(course[1]-course[0])
                boundary=min(uv.x,1-uv.x,uv.y,1-uv.y)
                # Both longitudinal anchors and the external alpha perimeter stay fixed.
                strength=min(1,max(0,boundary/.07),min(distance_to_edge(p,e) for e in boundary_edges)/14)
                positions.append(p-Vector((0,0,AWNING_BOW*math.sin(math.pi*t)*strength)))
            emit(output,positions,[u for p,u in segment],rows[0][2],face.material_index)


def corbel(output,xwall,xend,y,material):
    profile=[(xwall,202),(xwall,236),(xend,236),(xend,226),(xwall+(xend-xwall)*.44,213)]
    front=[Vector((x,y-10,z)) for x,z in profile];back=[Vector((x,y+10,z)) for x,z in profile]
    uv=[Vector((.08+(x-xwall)/150,.05+(z-202)/90)) for x,z in profile]
    emit(output,list(reversed(front)),list(reversed(uv)),0,material)
    emit(output,back,uv,0,material)
    for i in range(len(profile)):
        j=(i+1)%len(profile)
        emit(output,[front[i],front[j],back[j],back[i]],[Vector((.05,0)),Vector((.25,0)),Vector((.25,.3)),Vector((.05,.3))],0,material)


def add_beams(output,obj,name):
    arch.TIMBER='tile_ston06.jpg'
    builder=arch.Builder(obj)
    if name=='House03':
        for x,y,z in ((281.6,-420.5,258),(281.4,-181.2,258),(-240.6,-420.5,259),(-240.9,-181.2,285),(15.4,-420.5,294),(15.2,-181.2,294)):
            direction=1 if y<-300 else -1
            builder.beam((x,y,z-58),(x,y+direction*47,z-10),13,12,(1,0,0))
    material=[m.name for m in obj.data.materials].index('tile_ston06.jpg')
    for face,uvs in zip(builder.faces,builder.uvs):
        emit(output,[Vector(builder.vertices[i]) for i in face],[Vector((.04+u*.35,v)) for u,v in uvs],0,material)


def chimney_belts(output,obj,name):
    # A masonry collar follows the tapered chimney inside its original foot envelope.
    material=[m.name for m in obj.data.materials].index('tile_ston06.jpg')
    chimneys=[(-110.84,-134.94,314.60,31.3,33.1)] if name=='House01' else [(-177.56,y,381.38,43.4,43.4) for y in (12.26,150.75,283.35)]
    for x,y,z,width,depth in chimneys:
        rings=[]
        for level,spread in ((z-21,2),(z-17,6),(z-9,6),(z-5,1)):
            rings.append([Vector((x+sx*(width/2+spread),y+sy*(depth/2+spread),level)) for sx,sy in ((-1,-1),(1,-1),(1,1),(-1,1))])
        for lower,upper in zip(rings,rings[1:]):
            for i in range(4):
                j=(i+1)%4
                emit(output,[lower[i],lower[j],upper[j],upper[i]],[Vector((.58,.13)),Vector((.95,.13)),Vector((.95,.24)),Vector((.58,.24))],0,material)


def build(name):
    folder=ROOT/name;bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    obj=mesh_helper.model_mesh();mesh_helper.snapshot_reference(obj)
    reference=arch.reference_collection('REF_ORIGINAL')
    with bpy.data.libraries.load(str(folder/'original/source.blend')) as (source,loaded):loaded.objects=source.objects
    for item in loaded.objects:
        if item.type=='MESH' and not item.get('mu_helper'):
            item.parent=None;item.modifiers.clear();item['mu_reference']=True;reference.objects.link(item)
    pairs={'House01':[(114,115),(178,179),(188,189),(198,199)],'House03':[(137,138)]}[name]
    replaced={i for pair in pairs for i in pair};output=[]
    boundary_edges=canopy_edges(obj)
    before=mesh_helper.bound([obj.matrix_world@v.co for v in obj.data.vertices])
    for face in obj.data.polygons:
        if face.index in replaced:continue
        if obj.data.materials[face.material_index].name=='tile_ston07.tga':awning(output,obj,face,boundary_edges)
        else:
            rows=face_data(obj,face);emit(output,[p for p,u,b in rows],[u for p,u,b in rows],rows[0][2],face.material_index)
    for pair in pairs:window(output,obj,pair)
    material=[m.name for m in obj.data.materials].index('tile_ston06.jpg')
    walls=(-183.6,181.4) if name=='House01' else (-280.9,82.8)
    ends=(-216,208) if name=='House01' else (-323,124)
    centers=(-199.0,-67.77,69.25,202.08) if name=='House01' else (-51.78,79.5,216.51,349.34)
    for wall,end in zip(walls,ends):
        for y in centers:corbel(output,wall,end,y,material)
    structural=load_helper('houses_structural',ROOT/'structural.py')
    structural.window_surrounds(output,sys.modules[__name__],obj,name)
    if name=='House03':structural.canopy_structure(output,sys.modules[__name__],obj)
    chimney_belts(output,obj,name)
    mesh_helper.replace_mesh(obj,output)
    after=mesh_helper.bound([obj.matrix_world@v.co for v in obj.data.vertices])
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.002,(name,before,after)
    bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(output),bounds_before=before,bounds_after=after,geometry_only=True),indent=2)+'\n')

if __name__=='__main__':
    for name in os.environ.get('HOUSE_NAMES',','.join(NAMES)).split(','):
        if name!='House04':build(name)
