"""Prove the complete additive mesh retains raw bindings, UVs, geometry and normals."""
from pathlib import Path
import json
import math
import struct
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from raw_audit import raw


def shell(path):
    data=raw.payload(path);cursor=38;result=[]
    for index in range(struct.unpack_from('<h',data,32)[0]):
        nv,nn,nu,nt,texture=struct.unpack_from('<5h',data,cursor);cursor+=10
        vertices=[struct.unpack_from('<h2x3f',data,cursor+i*16) for i in range(nv)];cursor+=nv*16
        normals=[struct.unpack_from('<h2x3fh2x',data,cursor+i*20) for i in range(nn)];cursor+=nn*20
        uvs=[struct.unpack_from('<2f',data,cursor+i*8) for i in range(nu)];cursor+=nu*8
        faces=[]
        for i in range(nt):
            offset=cursor+i*64
            vi=struct.unpack_from('<4h',data,offset+2)[:3];ni=struct.unpack_from('<4h',data,offset+10)[:3];ui=struct.unpack_from('<4h',data,offset+18)[:3]
            faces.append([(vertices[v],normals[n],uvs[u]) for v,n,u in zip(vi,ni,ui)])
        cursor+=nt*64;material=data[cursor:cursor+32].split(b'\0')[0].decode();cursor+=32
        if index==1:
            assert material=='fire_02.jpg' and nt==38
            result=faces
    return result


def check():
    folder=ROOT/'Bonfire01';old=shell(folder/'baseline/Bonfire01.bmd');new=shell(folder/'exports/Bonfire01.bmd')
    remaining=set(range(len(new)));maximum=[0,0,0]
    for face in old:
        best=(float('inf'),None,None)
        for i in remaining:
            for shift in range(3):
                other=[new[i][(j+shift)%3] for j in range(3)]
                if any(a[0][0]!=b[0][0] or a[1][0]!=b[1][0] for a,b in zip(face,other)):continue
                errors=[max(math.dist(a[k][start:end],b[k][start:end]) for a,b in zip(face,other)) for k,start,end in ((0,1,4),(1,1,4),(2,0,2))]
                if max(errors)<best[0]:best=(max(errors),i,errors)
        assert best[1] is not None
        position,normal,uv=best[2]
        assert position<.0003 and normal<.0003 and uv<.000001,(position,normal,uv)
        remaining.remove(best[1]);maximum=[max(a,b) for a,b in zip(maximum,best[2])]
    (folder/'validation/protected-shell.json').write_text(json.dumps(dict(status='PASS',triangles=38,
        maximum_raw_local_position_error=maximum[0],maximum_raw_local_normal_error=maximum[1],maximum_uv_error=maximum[2],
        vertex_and_normal_nodes='EXACT',material_and_cyclic_winding='EXACT'),indent=2))
if __name__=='__main__':check()
