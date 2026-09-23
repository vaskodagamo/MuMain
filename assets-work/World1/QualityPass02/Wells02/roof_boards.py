"""Separate solid roof boards along painted grain; retain the exact structural underside."""
from mathutils import Vector,geometry

def roof(source,group):
    source_faces=[f for f in source.data.polygons if f.vertices[0] in group]
    top=[f for f in source_faces if f.normal.z>.3];bottom=[f for f in source_faces if f.normal.z<-.3]
    planes=[]
    for face in top:
        matching=next((g for g in planes if g[0].normal.dot(face.normal)>.9999),None)
        if matching is None:planes.append([face])
        else:matching.append(face)
    top_ids={i for plane in planes for f in plane for i in f.vertices}
    bottom=[f for f in source_faces if not any(i in top_ids for i in f.vertices)]
    assert len(planes)==2 and len(bottom)==4,(len(planes),len(bottom))
    vertices=[];faces=[];uv=[]
    def quad(points,coordinates):
        base=len(vertices);vertices.extend(points);uv.extend(coordinates);faces.extend(((base,base+1,base+2),(base+2,base+3,base)))
    for plane in planes:
        ids=set(i for face in plane for i in face.vertices);assert len(ids)==4
        points=[source.data.vertices[i].co for i in ids]
        front=sorted(sorted(points,key=lambda p:p.y)[:2],key=lambda p:p.x);back=sorted(sorted(points,key=lambda p:p.y)[2:],key=lambda p:p.x)
        lower_faces=[f for f in bottom if f.normal.dot(plane[0].normal)<-.99]
        lower_ids=set(i for f in lower_faces for i in f.vertices);assert len(lower_ids)==4
        lower=[source.data.vertices[i].co for i in lower_ids]
        lower_front=sorted(sorted(lower,key=lambda p:p.y)[:2],key=lambda p:p.x);lower_back=sorted(sorted(lower,key=lambda p:p.y)[2:],key=lambda p:p.x)
        a,b,c=[source.data.vertices[i].co for i in plane[0].vertices]
        au,bu,cu=[Vector((*source.data.uv_layers[0].data[i].uv,0)) for i in plane[0].loop_indices]
        def painted(point):
            value=geometry.barycentric_transform(point,a,b,c,au,bu,cu);return (value.x,value.y)
        def point(t,y,lower=False):
            left,right=(lower_front,lower_back) if lower else (front,back)
            return left[0].lerp(left[1],t).lerp(right[0].lerp(right[1],t),y)
        v0,v1=painted(front[0])[1],painted(front[1])[1]
        # The frozen atlas has three dark joints: line the physical boards up with them.
        divisions=sorted([0,1]+[(v-v0)/(v1-v0) for v in (.635,.785,.912) if 0<(v-v0)/(v1-v0)<1])
        length=(front[1]-front[0]).length;y_length=(back[0]-front[0]).length
        gap=.75/length;bevel=1.0/length;y_bevel=1.2/y_length
        for index,(start,end) in enumerate(zip(divisions,divisions[1:])):
            lo=start+(gap if index else 0);hi=end-(gap if index<len(divisions)-2 else 0)
            inset_lo=lo+(bevel if index else 0);inset_hi=hi-(bevel if index<len(divisions)-2 else 0)
            inner=[point(inset_lo,y_bevel),point(inset_hi,y_bevel),point(inset_hi,1-y_bevel),point(inset_lo,1-y_bevel)]
            top_ring=[point(lo,0),point(hi,0),point(hi,1),point(lo,1)]
            base_ring=[point(lo,0,True),point(hi,0,True),point(hi,1,True),point(lo,1,True)]
            outer=[p.lerp(q,.12) for p,q in zip(top_ring,base_ring)]
            quad(inner,[painted(p) for p in inner])
            for j in range(4):
                k=(j+1)%4
                bevel_points=[inner[j],outer[j],outer[k],inner[k]]
                quad(bevel_points,[painted(p) for p in bevel_points])
                side=[outer[j],base_ring[j],base_ring[k],outer[k]]
                if j in (0,2):
                    s0,s1=(lo,hi) if j==0 else (hi,lo)
                    coords=[(.23+.5*s0,.895),(.23+.5*s0,.81),(.23+.5*s1,.81),(.23+.5*s1,.895)]
                else:
                    u0,u1=painted(top_ring[j])[0],painted(top_ring[k])[0]
                    mid=(painted(top_ring[j])[1]+painted(top_ring[k])[1])/2
                    mid=min(.98,max(.53,mid))
                    coords=[(u0,mid+.012),(u0,mid-.012),(u1,mid-.012),(u1,mid+.012)]
                quad(side,coords)
    # The lower roof planes are protected support contact surfaces and remain untouched.
    return vertices,faces,uv,{f.index for f in source_faces if f not in bottom}
