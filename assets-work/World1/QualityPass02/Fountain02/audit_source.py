"""Match every authored triangle to actual BMD and preserve individual root contracts."""
import json
from pathlib import Path
import sys

sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
import os
NAMES=tuple(os.environ.get('FOUNTAIN_NAMES','Waterspout01').split(','))
POSITION_TOLERANCE=.0003
UV_TOLERANCE=.000001


def smd_triangles(path):
    lines=path.read_text().split('triangles\n')[1].splitlines()
    return [(lines[i],[list(map(float,row.split())) for row in lines[i+1:i+4]]) for i in range(0,len(lines)-1,4)]


def root_points(triangles):
    result={}
    for texture,rows in triangles:
        for row in rows: result.setdefault(int(row[0]),set()).add(tuple(row[1:4]))
    return result


def root_contract(folder,actual):
    old=root_points(smd_triangles(folder/'baseline/smd'/f'{folder.name}.smd'))
    new=root_points(actual)
    records={}
    assert old.keys()==new.keys()
    for bone,points in old.items():
        before=[[min(p[a] for p in points) for a in range(3)],[max(p[a] for p in points) for a in range(3)]]
        after=[[min(p[a] for p in new[bone]) for a in range(3)],[max(p[a] for p in new[bone]) for a in range(3)]]
        assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<POSITION_TOLERANCE,(folder.name,bone,before,after)
        contacts=[p for p in points if p[2]<before[0][2]+4]
        maximum=max(min(max(abs(a-b) for a,b in zip(p,q)) for q in new[bone]) for p in contacts)
        assert maximum<POSITION_TOLERANCE,(folder.name,bone,'contact drift',maximum)
        records[bone]=dict(bounds_before=before,bounds_after=after,ground_contacts=len(contacts),maximum_contact_delta=maximum)
    return records


def match_triangle(texture,rows,actual):
    errors=[]
    for candidate_index,(material,candidate) in enumerate(actual):
        if texture!=material: continue
        for shift in range(3):
            aligned=candidate[shift:]+candidate[:shift]
            if any(a[0]!=b[0] for a,b in zip(rows,aligned)): continue
            if max(abs(a-b) for expected,got in zip(rows,aligned) for a,b in zip(expected[7:9],got[7:9]))>=UV_TOLERANCE: continue
            errors.append((max(abs(a-b) for expected,got in zip(rows,aligned) for a,b in zip(expected[1:4],got[1:4])),candidate_index))
    assert errors and min(errors)[0]<POSITION_TOLERANCE,(texture,min(errors) if errors else 'missing triangle')
    error,index=min(errors)
    actual.pop(index)
    return error


def protected_contract(folder,actual):
    baseline=smd_triangles(folder/'baseline/smd/Waterspout01.smd')
    indices=json.loads((folder/'validation/authored.json').read_text())['protected_original_triangle_indices']
    protected=[baseline[i] for i in indices]
    remaining=list(actual)
    for material,rows in protected:match_triangle(material,rows,remaining)
    return dict(exact_protected_triangles=len(protected),policy='All3 non-dragon material slots and34 basin triangles inside dragon slot1 retain each corner position/UV/bone and winding')


def audit(name):
    folder=ROOT/name
    actual=smd_triangles(folder/'validation/new'/f'{name}.smd')
    bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    indices={name:index for index,name in enumerate(rig['mu_bone_order'])}
    assert len(obj.data.uv_layers)==1
    maximum=0
    unmatched=list(actual)
    for face in obj.data.polygons:
        rows=[]
        for loop in face.loop_indices:
            vertex=obj.data.vertices[obj.data.loops[loop].vertex_index]
            assert len(vertex.groups)==1 and vertex.groups[0].weight==1
            bone=indices[obj.vertex_groups[vertex.groups[0].group].name]
            position=obj.matrix_world@vertex.co
            uv=obj.data.uv_layers[0].data[loop].uv
            rows.append([bone,*position,0,0,0,*uv])
        maximum=max(maximum,match_triangle(obj.data.materials[face.material_index].name,rows,unmatched))
    assert len(obj.data.polygons)==len(actual) and not unmatched
    def area_metrics(triangles):
        geometric=[]; uv_areas=[]
        for material,rows in triangles:
            a,b,c=[Vector(row[1:4]) for row in rows]
            geometric.append((b-a).cross(c-a).length/2)
            x,y,z=[Vector(row[7:9]) for row in rows]
            uv_areas.append(abs((y.x-x.x)*(z.y-x.y)-(y.y-x.y)*(z.x-x.x))/2)
        return dict(minimum_geometry_area=min(geometric),minimum_uv_area=min(uv_areas),zero_geometry=sum(a<1e-8 for a in geometric),zero_uv=sum(a<1e-10 for a in uv_areas))
    baseline_metrics=area_metrics(smd_triangles(folder/'baseline/smd'/f'{name}.smd'))
    actual_metrics=area_metrics(actual)
    assert actual_metrics['zero_geometry']==0,actual_metrics
    assert actual_metrics['zero_uv']<=baseline_metrics['zero_uv'],(baseline_metrics,actual_metrics)
    images=[i for i in bpy.data.images if i.source=='FILE']
    assert all(i.packed_file for i in images)
    report=dict(status='PASS',triangles=len(actual),one_to_one_triangle_match=True,area_metrics=actual_metrics,baseline_area_metrics=baseline_metrics,protected_contract=protected_contract(folder,actual),every_authored_triangle_matches_material_bone_position_uv=True,maximum_position_delta=maximum,position_tolerance=POSITION_TOLERANCE,uv_tolerance=UV_TOLERANCE,uv_layers=list(obj.data.uv_layers.keys()),packed_images={i.name:list(i.size) for i in images},per_root_contract=root_contract(folder,actual))
    (folder/'validation/authored-match.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,'every authored corner + individual root extents/contact PASS')


for name in NAMES: audit(name)
