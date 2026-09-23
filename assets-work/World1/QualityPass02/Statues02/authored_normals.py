"""Compare actual authored corner normals to raw final BMD in the static action."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'Houses01'))
from raw_motion import motion,world,raw
from posed_source import actual_triangles,match
TOLERANCE=.001


def audit(name):
    folder=ROOT/name;actual=actual_triangles(folder/'validation/new'/f'{name}.smd')
    raw_triangles=[(mesh,vertices,normals) for mesh in raw.meshes(folder/'exports'/f'{name}.bmd') for vertices,normals in mesh['triangles']]
    assert len(actual)==len(raw_triangles)
    lookup={}
    for (material,rows),(mesh,vertices,normals) in zip(actual,raw_triangles):
        assert material==mesh['material']
        for row,vertex,normal in zip(rows,vertices,normals):
            assert int(row[0])==mesh['vertices'][vertex][0]
            record=mesh['normals'][normal];lookup[id(row)]=(record[0],Vector(record[1:4]))
    bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');order={name:i for i,name in enumerate(rig['mu_bone_order'])}
    normal_matrix=obj.matrix_world.to_3x3().inverted().transposed();pairs=[]
    for face in obj.data.polygons:
        rows=[];normals=[]
        for loop in face.loop_indices:
            vertex=obj.data.vertices[obj.data.loops[loop].vertex_index]
            bone=order[obj.vertex_groups[vertex.groups[0].group].name]
            rows.append([bone,*(obj.matrix_world@vertex.co),*obj.data.uv_layers[0].data[loop].uv])
            normals.append((normal_matrix@obj.data.corner_normals[loop].vector).normalized())
        material=obj.data.materials[face.material_index].name
        final=match(material,rows,actual)
        for row,normal,after in zip(rows,normals,final):
            node,value=lookup[id(after)];pairs.append((material,int(row[0]),normal,node,value))
    old_meta,old,_=motion(folder/'baseline'/f'{name}.bmd');new_meta,new,_=motion(folder/'exports'/f'{name}.bmd')
    old_bind=world(old,0)
    local=[(m,b,old_bind[b].to_3x3().inverted()@n,node,v) for m,b,n,node,v in pairs]
    records=[];materials={}
    for frame in range(1):
        a,b=world(old,frame),world(new,frame);maximum=0
        for material,bone,normal,node,final in local:
            delta=((a[bone].to_3x3()@normal).normalized()-(b[node].to_3x3()@final).normalized()).length
            maximum=max(maximum,delta);materials[material]=max(materials.get(material,0),delta)
        records.append(dict(frame=frame,maximum_direction_delta=maximum))
    maximum=max(r['maximum_direction_delta'] for r in records)
    report=dict(status='PASS' if maximum<TOLERANCE else 'FAIL',maximum_direction_delta=maximum,tolerance=TOLERANCE,materials=materials,frames=records,authored_corners=len(pairs),method='Blender authored split corner normal against actual BMD raw normal node/vector, each through full baseline/final hierarchy in the static bind/action frame')
    (folder/'validation/authored-raw-normals.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report));assert maximum<TOLERANCE,materials

if __name__=='__main__':
    import os
    for name in os.environ.get('STATUE_NAMES','StoneStatue01,StoneStatue03,SteelStatue01').split(','):audit(name)
