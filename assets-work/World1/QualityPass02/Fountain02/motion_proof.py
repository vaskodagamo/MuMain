"""Read-only raw BMD motion audit against actual runtime float records."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
sys.dont_write_bytecode=True
from mathutils import Euler,Matrix,Vector
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('raw_bmd',ROOT.parents[1]/'Cannons01/raw_bindings.py')
raw=importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)


def motion(path):
    data=raw.payload(path);meshes,bones,actions=struct.unpack_from('<3h',data,32);cursor=38
    for _ in range(meshes):
        nv,nn,nu,nt,texture=struct.unpack_from('<5h',data,cursor)
        cursor+=10+nv*16+nn*20+nu*8+nt*64+32
    tail=data[cursor:];metadata=[]
    for _ in range(actions):
        keys,lock=struct.unpack_from('<hB',data,cursor);cursor+=3
        locked=data[cursor:cursor+keys*12] if lock else b'';cursor+=len(locked)
        metadata.append((keys,lock,locked.hex()))
    result=[]
    for index in range(bones):
        dummy=data[cursor];cursor+=1
        if dummy:result.append(dict(dummy=True));continue
        name=data[cursor:cursor+32].split(b'\0')[0].decode('ascii');cursor+=32
        parent=struct.unpack_from('<h',data,cursor)[0];cursor+=2
        tracks=[]
        for keys,lock,locked in metadata:
            positions=[struct.unpack_from('<3f',data,cursor+frame*12) for frame in range(keys)];cursor+=keys*12
            rotations=[struct.unpack_from('<3f',data,cursor+frame*12) for frame in range(keys)];cursor+=keys*12
            tracks.append(list(zip(positions,rotations)))
        result.append(dict(name=name,parent=parent,tracks=tracks))
    assert cursor==len(data),(cursor,len(data))
    return metadata,result,tail


def world(bones,frame):
    result={}
    for index,bone in enumerate(bones):
        position,rotation=bone['tracks'][0][frame]
        local=Matrix.Translation(Vector(position))@Euler(rotation,'XYZ').to_matrix().to_4x4()
        result[index]=local if bone['parent']<0 else result[bone['parent']]@local
    return result


def bounds(meshes,transforms):
    points=[transforms[vertex[0]]@Vector(vertex[1:4]) for mesh in meshes for vertex in mesh['vertices']]
    return [[f(p[a] for p in points) for a in range(3)] for f in (min,max)]


def audit():
    folder=ROOT/'Waterspout01';before_path=folder/'baseline/Waterspout01.bmd';after_path=folder/'exports/Waterspout01.bmd'
    old_meta,old,old_tail=motion(before_path);new_meta,new,new_tail=motion(after_path)
    assert old_meta==new_meta
    assert [(b['name'],b['parent']) for b in old]==[(b['name'],b['parent']) for b in new]
    differences=[abs(x-y) for a,b in zip(old,new) for ta,tb in zip(a['tracks'],b['tracks']) for fa,fb in zip(ta,tb) for va,vb in zip(fa,fb) for x,y in zip(va,vb)]
    maximum=max(differences);assert maximum<.0001,maximum
    old_mesh,new_mesh=raw.meshes(before_path),raw.meshes(after_path);frames=[]
    for frame in range(old_meta[0][0]):
        a,b=world(old,frame),world(new,frame)
        delta=max(abs(x-y) for bone in a for ra,rb in zip(a[bone],b[bone]) for x,y in zip(ra,rb))
        ba,bb=bounds(old_mesh,a),bounds(new_mesh,b)
        bound_delta=max(abs(x-y) for ra,rb in zip(ba,bb) for x,y in zip(ra,rb))
        assert delta<.0001 and bound_delta<.002,(frame,delta,bound_delta)
        frames.append(dict(frame=frame,raw_world_matrix_delta=delta,raw_posed_bounds_delta=bound_delta))
    report=dict(status='PASS',raw_tail_byte_identical=old_tail==new_tail,baseline_raw_motion_sha256=hashlib.sha256(old_tail).hexdigest(),final_raw_motion_sha256=hashlib.sha256(new_tail).hexdigest(),maximum_raw_float_delta=maximum,raw_float_components=len(differences),action_metadata=old_meta,frames=frames,local_component_tolerance=.0001,note='Direct official export measured against existing static-validator local-motion precision .0001. Raw byte identity and float drift are reported separately, never claimed equivalent to exact baseline bytes.')
    (folder/'validation/raw-motion-equivalence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Raw motion',maximum,'byte identical',old_tail==new_tail,'frames',len(frames))

if __name__=='__main__':audit()
