"""Every authored corner evaluated under intended baseline motion against final BMD motion."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from motion_proof import motion,world


def actual_triangles(path):
    lines=path.read_text().split('triangles\n')[1].splitlines()
    return [(lines[i],[list(map(float,r.split())) for r in lines[i+1:i+4]]) for i in range(0,len(lines)-1,4)]


def match(texture,rows,actual):
    candidates=[]
    for index,(material,vertices) in enumerate(actual):
        if material!=texture:continue
        for shift in range(3):
            ordered=vertices[shift:]+vertices[:shift]
            if any(a[0]!=b[0] for a,b in zip(rows,ordered)):continue
            if max(abs(x-y) for a,b in zip(rows,ordered) for x,y in zip(a[4:6],b[7:9]))>.000001:continue
            error=max(abs(x-y) for a,b in zip(rows,ordered) for x,y in zip(a[1:4],b[1:4]))
            if error<.0003:candidates.append((error,index,shift))
    assert candidates
    error,index,shift=min(candidates);material,vertices=actual.pop(index)
    return vertices[shift:]+vertices[:shift]


def audit():
    folder=ROOT/'Waterspout01';actual=actual_triangles(folder/'validation/new/Waterspout01.smd')
    bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');order={name:i for i,name in enumerate(rig['mu_bone_order'])}
    pairs=[]
    for face in obj.data.polygons:
        rows=[]
        for loop in face.loop_indices:
            vertex=obj.data.vertices[obj.data.loops[loop].vertex_index]
            bone=order[obj.vertex_groups[vertex.groups[0].group].name]
            rows.append([bone,*(obj.matrix_world@vertex.co),*obj.data.uv_layers[0].data[loop].uv])
        final=match(obj.data.materials[face.material_index].name,rows,actual)
        pairs.extend((int(a[0]),Vector(a[1:4]),Vector(b[1:4])) for a,b in zip(rows,final))
    assert not actual
    old_meta,old,_=motion(folder/'baseline/Waterspout01.bmd');new_meta,new,_=motion(folder/'exports/Waterspout01.bmd')
    old_bind,new_bind=world(old,0),world(new,0)
    local=[(bone,old_bind[bone].inverted()@a,new_bind[bone].inverted()@b) for bone,a,b in pairs]
    frames=[]
    for frame in range(21):
        a,b=world(old,frame),world(new,frame)
        maximum=max((a[bone]@p-b[bone]@q).length for bone,p,q in local)
        assert maximum<.0003,(frame,maximum)
        frames.append(dict(frame=frame,maximum_authored_corner_distance=maximum))
    (folder/'validation/posed-authored-match.json').write_text(json.dumps(dict(status='PASS',method='Every authored source triangle corner mapped one-to-one to final SMD corner, transformed through original intended raw baseline action and actual final raw BMD action at all21 keys',corners=len(pairs),frames=frames,maximum=max(f['maximum_authored_corner_distance'] for f in frames),tolerance=.0003,normal_proof='raw-normal-bindings.json evaluates actual raw normal ownership/direction through every hierarchical frame'),indent=2)+'\n')
    print('Every authored source corner all21 poses PASS')

if __name__=='__main__':audit()
