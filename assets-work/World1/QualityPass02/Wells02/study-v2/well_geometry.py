"""Measured stone coping profile and recessed timber board joints inside existing extents."""
import math
from mathutils import Vector,geometry

def basin(points):
    levels=sorted(set(round(p.z,3) for p in points));base=[p for p in points if abs(p.z-levels[0])<.001]
    center=sum(base,Vector())/len(base);radius=max((p-center).xy.length for p in base)
    inner=[p for p in points if abs(p.z-levels[1])<.001];inner_radius=sum((p-center).xy.length for p in inner)/len(inner)
    angles=sorted(math.atan2(p.y-center.y,p.x-center.x) for p in base)
    profile=[(radius,levels[0],.0005),(radius,88,.373),(radius-2,94,.399),
        (radius-2,levels[2],.4196),(radius-6,levels[3],.434),
        (inner_radius+3,levels[3],.455),(inner_radius,levels[3]-3.5,.4729),(inner_radius,levels[1],.2775)]
    vertices=[];uv=[];faces=[]
    for index,(r,z,v) in enumerate(profile):
        for a in angles:
            p=Vector((center.x+r*math.cos(a),center.y+r*math.sin(a),z))
            if index==0:p=min(base,key=lambda b:(b-p).length).copy()
            vertices.append(p);uv.append((a/(2*math.pi)+.75,v))
    for row in range(len(profile)-1):
        for j in range(8):
            a=row*8+j;b=row*8+(j+1)%8;c=b+8;d=a+8
            faces.extend(((a,b,c),(c,d,a)))
    # Keep a solid interior floor with noncollapsed stone-atlas coordinates.
    k=len(vertices);vertices.append(Vector((center.x,center.y,levels[1])));uv.append((.5,.26))
    for j in range(8):faces.append(((len(profile)-1)*8+j,(len(profile)-1)*8+(j+1)%8,k))
    return vertices,faces,uv

def roof(source,group):
    """Replace only upper roof planes; all fascia/underside/support interfaces remain."""
    top=[f for f in source.data.polygons if f.vertices[0] in group and f.normal.z>.3]
    planes=[]
    for face in top:
        matching=next((g for g in planes if g[0].normal.dot(face.normal)>.9999),None)
        if matching is None:planes.append([face])
        else:matching.append(face)
    assert len(planes)==2,[(len(g),list(g[0].normal)) for g in planes]
    vertices=[];faces=[];uv=[]
    for plane in planes:
        ids=set(i for face in plane for i in face.vertices);assert len(ids)==4,len(ids)
        points=[source.data.vertices[i].co for i in ids]
        front=sorted(sorted(points,key=lambda p:p.y)[:2],key=lambda p:p.x)
        back=sorted(sorted(points,key=lambda p:p.y)[2:],key=lambda p:p.x)
        p00,p10=front;p01,p11=back;normal=plane[0].normal.copy()
        a,b,c=[source.data.vertices[i].co for i in plane[0].vertices]
        au,bu,cu=[Vector((*source.data.uv_layers[0].data[i].uv,0)) for i in plane[0].loop_indices]
        length=((p01+p11-p00-p10)/2).length;inset=1.4/length
        rows=[(0,0)]
        for division in range(1,4):
            t=division/4
            rows.extend(((t-inset,0),(t,2.8),(t+inset,0)))
        rows.append((1,0));start=len(vertices)
        for t,depth in rows:
            for p,q in ((p00,p01),(p10,p11)):
                on_plane=p.lerp(q,t);vertices.append(on_plane-normal*depth)
                value=geometry.barycentric_transform(on_plane,a,b,c,au,bu,cu);uv.append((value.x,value.y))
        for row in range(len(rows)-1):
            a=start+row*2;b=a+1;c=b+2;d=a+2
            # Ensure generated plane follows original outward orientation.
            tri=(a,b,c)
            if (vertices[b]-vertices[a]).cross(vertices[c]-vertices[a]).dot(normal)<0:
                faces.extend(((c,b,a),(a,d,c)))
            else:faces.extend(((a,b,c),(c,d,a)))
    return vertices,faces,uv,{f.index for plane in planes for f in plane}
