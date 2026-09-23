"""Strict authored-to-export, frozen-input and support contact contracts."""
from pathlib import Path
import hashlib
import json
import math
import struct
import subprocess
import sys
import bpy
from mathutils import Euler, Matrix, Vector

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
sys.path.insert(0,str(ROOT))
import raw_bindings as raw
from pipeline import CONVERTER, NAMES
POSITION_TOLERANCE = .0003
UV_TOLERANCE = .000001
NORMAL_TOLERANCE = .001


def smd(path):
    lines = path.read_text().split('triangles\n')[1].splitlines()[:-1]
    return [(lines[i], [list(map(float,row.split())) for row in lines[i+1:i+4]]) for i in range(0,len(lines),4)]


def motion(path):
    data = raw.payload(path)
    meshes,bones,actions = struct.unpack_from('<3h',data,32)
    cursor = 38
    for _ in range(meshes):
        nv,nn,nu,nt,_ = struct.unpack_from('<5h',data,cursor)
        cursor += 10+nv*16+nn*20+nu*8+nt*64+32
    tail = data[cursor:]
    keys,lock = struct.unpack_from('<hB',data,cursor)
    assert (bones,actions,keys,lock)==(1,1,1,0)
    cursor += 3
    assert data[cursor]==0
    name = data[cursor+1:cursor+33].split(b'\0')[0]
    parent = struct.unpack_from('<h',data,cursor+33)[0]
    values = struct.unpack_from('<6f',data,cursor+35)
    matrix = Matrix.Translation(Vector(values[:3])) @ Euler(values[3:],'XYZ').to_matrix().to_4x4()
    return dict(tail=tail,name=name,parent=parent,values=values,matrix=matrix)


def authored(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(0)
    references=[c for c in bpy.data.collections if c.name.startswith('REF_ORIGINAL')]
    assert references and all(c.hide_render and not c.vs.export for c in references)
    for material in bpy.data.materials:
        if material.use_nodes:
            for node in material.node_tree.nodes:
                if node.type=='TEX_IMAGE' and node.image:
                    assert node.image.packed_file, node.image.name
    result = []
    for obj in bpy.context.scene.objects:
        if obj.type!='MESH' or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        mesh = obj.data
        assert len(mesh.uv_layers)==1
        for face in mesh.polygons:
            corners = []
            for loop in face.loop_indices:
                vertex = mesh.vertices[mesh.loops[loop].vertex_index]
                assert len(vertex.groups)==1 and vertex.groups[0].weight==1
                normal = obj.matrix_world.to_3x3() @ mesh.corner_normals[loop].vector
                corners.append([0,*list(obj.matrix_world@vertex.co),*list(normal),*list(mesh.uv_layers[0].data[loop].uv)])
            result.append((mesh.materials[face.material_index].name,corners))
    return result


def match(before,after,normal=False):
    unused = set(range(len(after)))
    maximum = [0,0,0]
    indices = []
    for material,corners in before:
        choices = []
        for index in unused:
            if after[index][0] != material:
                continue
            for shift in range(3):
                rows = [after[index][1][(j+shift)%3] for j in range(3)]
                if any(a[0]!=b[0] for a,b in zip(corners,rows)):
                    continue
                pos = max((Vector(a[1:4])-Vector(b[1:4])).length for a,b in zip(corners,rows))
                uv = max((Vector(a[7:9])-Vector(b[7:9])).length for a,b in zip(corners,rows))
                norm = max((Vector(a[4:7])-Vector(b[4:7])).length for a,b in zip(corners,rows))
                choices.append((pos+uv,index,pos,uv,norm))
        assert choices
        _,index,pos,uv,norm = min(choices)
        assert pos<POSITION_TOLERANCE and uv<UV_TOLERANCE,(pos,uv)
        if normal:
            assert norm<NORMAL_TOLERANCE,norm
        maximum = [max(a,b) for a,b in zip(maximum,(pos,uv,norm))]
        indices.append(index)
        unused.remove(index)
    return dict(maximum_position_delta=maximum[0],maximum_uv_delta=maximum[1],maximum_normal_delta=maximum[2],matched=len(indices)),indices


def bounds(points):
    return [[fn(p[a] for p in points) for a in range(3)] for fn in (min,max)]


def raw_normals(path,expected):
    transform = motion(path)['matrix']
    rows = []
    for mesh in raw.meshes(path):
        for vertices,normals in mesh['triangles']:
            corners=[]
            for vi,ni in zip(vertices,normals):
                v,n=mesh['vertices'][vi],mesh['normals'][ni]
                assert v[0]==n[0]==0
                corners.append(transform.to_3x3()@Vector(n[1:4]))
            rows.extend(corners)
    error=max((a-Vector(b[4:7])).length for a,b in zip(rows,[c for _,tri in expected for c in tri]))
    assert error<NORMAL_TOLERANCE,error
    return error


def placements(name,old,new):
    records=json.loads((ROOT/name/'placements.json').read_text())
    maximum=0
    for record in records:
        transform=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix()*record['scale']
        a=bounds([transform@p for p in old]);b=bounds([transform@p for p in new])
        maximum=max(maximum,max(abs(x-y) for row,col in zip(a,b) for x,y in zip(row,col)))
    assert maximum<POSITION_TOLERANCE,maximum
    return dict(count=len(records),maximum_transformed_bounds_delta=maximum)


def validate(name):
    folder=ROOT/name
    before=folder/'baseline'/(name+'.bmd');after=folder/'exports'/(name+'.bmd')
    result=subprocess.run([CONVERTER,'bmd2smd',str(after),str(folder/'validation/new')],capture_output=True,check=True)
    (folder/'validation/convert-new.txt').write_bytes(result.stdout+result.stderr)
    for path in (folder/'validation/new').glob('*.smd'):
        command=[CONVERTER,'validate',str(path)] + (['--animation'] if '_a' in path.stem else [])
        result=subprocess.run(command,capture_output=True,check=True)
        (folder/'validation'/(path.stem+'-validate.txt')).write_bytes(result.stdout+result.stderr)
    for verb in ('info','compare'):
        command=[CONVERTER,verb,*([str(before)] if verb=='compare' else []),str(after)]
        result=subprocess.run(command,capture_output=True)
        assert result.returncode in ((0,2) if verb=='compare' else (0,))
        (folder/'validation'/(verb+'.txt')).write_bytes(result.stdout+result.stderr)
    old=smd(folder/'validation/baseline'/(name+'.smd'));new=smd(folder/'validation/new'/(name+'.smd'))
    source=authored(folder/'source.blend');proof,_=match(source,new,True)
    description=json.loads((folder/'validation/authored.json').read_text())
    protected=[old[i] for i in description['protected_original_faces']]
    protected_proof,_=match(protected,new,True)
    a,b=motion(before),motion(after)
    assert a['name']==b['name'] and a['parent']==b['parent']
    motion_delta=max(abs(x-y) for x,y in zip(a['values'],b['values']))
    assert motion_delta<.0001
    assert raw.payload(before)[:38]==raw.payload(after)[:38]
    assert [m['material'] for m in raw.meshes(before)]==[m['material'] for m in raw.meshes(after)]
    original_points=[Vector(c[1:4]) for _,tri in old for c in tri]
    points=[Vector(c[1:4]) for _,tri in new for c in tri]
    anchor_error=max(min((a-b).length for b in points) for a in original_points)
    assert anchor_error<POSITION_TOLERANCE
    report=dict(status='PASS',authored=proof,protected=protected_proof,triangles=len(new),
                bounds=bounds(points),placements=placements(name,original_points,points),
                original_vertex_contact_anchor_max_delta=anchor_error,raw_name_hex=raw.payload(after)[:32].hex(),
                raw_world_normal_max_delta=raw_normals(after,new),raw_motion_byte_exact=a['tail']==b['tail'],
                maximum_motion_delta=motion_delta,raw_bindings=raw.audit(after))
    (folder/'validation/contract.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,'PASS',len(new),'triangles')


def frozen():
    hashes=json.loads((ROOT/'data-hashes.json').read_text())
    for path,expected in hashes.items():
        assert hashlib.sha256((REPO/path).read_bytes()).hexdigest()==expected,path
    for name in NAMES:
        for texture in (ROOT/name/'exports').glob('*.OZJ'):
            assert texture.read_bytes()==(REPO/'src/bin/Data/Object2'/texture.name).read_bytes()


for name in NAMES:
    validate(name)
frozen()
print('All original Object2 data and exported texture bytes unchanged')
