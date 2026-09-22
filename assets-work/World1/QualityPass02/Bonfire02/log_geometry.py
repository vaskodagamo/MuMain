"""Measured split-log sections: charred square hot ends and softened exposed heels."""
import math
from mathutils import Vector, geometry

SECTION_TIMES = (0.0, .16, .58, .86, 1.0)
ARC_STEPS = 6


def ordered_ends(source, group):
    points = [source.data.vertices[i].co.copy() for i in group]
    lower, upper = sorted(points, key=lambda p:p.z)[:4], sorted(points, key=lambda p:p.z)[4:]
    center = sum(lower, Vector()) / 4
    end = sum(upper, Vector()) / 4
    axis = (end-center).normalized()
    u = (lower[0]-center).normalized();v = axis.cross(u).normalized()
    lower.sort(key=lambda p:math.atan2((p-center).dot(v),(p-center).dot(u)))
    upper = [min(upper,key=lambda q:((q-end)-(p-center)).length) for p in lower]
    return lower, upper, center, end


def projected_uv(source, faces, point):
    face = min(faces, key=lambda f:abs(f.normal.dot(point-source.data.vertices[f.vertices[0]].co)))
    positions = [source.data.vertices[i].co for i in face.vertices]
    coords = [Vector((*source.data.uv_layers[0].data[i].uv,0)) for i in face.loop_indices]
    value = geometry.barycentric_transform(point,*positions,*coords)
    return value.xy


def quarter_section(lower,upper,t):
    corners=[a.lerp(b,t) for a,b in zip(lower,upper)]
    a,b,c,d=corners
    points=[a.copy()];u_values=[0]
    square_blend=max(0,(t-.70)/.30)
    taper=1-.06*math.sin(math.pi*t)
    for step in range(ARC_STEPS+1):
        angle=math.pi*.5*step/ARC_STEPS
        x,y=math.cos(angle),math.sin(angle)
        square=max(x,y)
        x*=taper*((1-square_blend)+square_blend/square)
        y*=taper*((1-square_blend)+square_blend/square)
        points.append(a+(b-a)*x+(d-a)*y+(c-b-d+a)*(x*y))
        u_values.append(.28+.44*step/ARC_STEPS)
    return points,u_values


def surface_triangles(surfaces,start,end):
    axis=(end-start).normalized();result=[]
    for points,uv,smooth,is_cap in surfaces:
        for indices in ((0,1,2),(0,2,3)) if len(points)==4 else ((0,1,2),):
            ps=[points[i] for i in indices];coords=[uv[i] for i in indices]
            normal=(ps[1]-ps[0]).cross(ps[2]-ps[0])
            if normal.length<1e-7:continue
            mid=sum(ps,Vector())/3;fraction=(mid-start).dot(axis)/(end-start).length
            outward=mid-start.lerp(end,min(1,max(0,fraction))) if not is_cap else axis*(1 if fraction>.5 else -1)
            if normal.dot(outward)<0:ps.reverse();coords.reverse()
            result.append((ps,coords,smooth))
    return result


def log_surfaces(source,group):
    lower,upper,start,end=ordered_ends(source,group)
    heel=min(range(4),key=lambda i:lower[i].z)
    lower=lower[heel:]+lower[:heel];upper=upper[heel:]+upper[:heel]
    faces=[f for f in source.data.polygons if f.vertices[0] in group]
    axis=(end-start).normalized();caps=[f for f in faces if abs(f.normal.dot(axis))>=.5]
    rings=[quarter_section(lower,upper,t) for t in SECTION_TIMES]
    v_limits=[sum(projected_uv(source,[f for f in caps if f.normal.dot(axis)*sign>.5],p).y for p in points)/4 for points,sign in ((lower,-1),(upper,1))]
    assert all(min((p-q).length for q in rings[-1][0])<.00001 for p in upper)
    surfaces=[];count=len(rings[0][0])
    for section in range(len(rings)-1):
        first,values=rings[section];second,_=rings[section+1]
        v0,v1=[v_limits[0]*(1-t)+v_limits[1]*t for t in SECTION_TIMES[section:section+2]]
        for i in range(count):
            j=(i+1)%count;u0=values[i];u1=values[j] if j else 1
            points=[first[i],first[j],second[j],second[i]]
            uv=[Vector((u0,v0)),Vector((u1,v0)),Vector((u1,v1)),Vector((u0,v1))]
            surfaces.append((points,uv,i!=0 and j!=0,False))
    for (ring,_),sign in ((rings[0],-1),(rings[-1],1)):
        center=sum(ring,Vector())/len(ring);candidates=[f for f in caps if f.normal.dot(axis)*sign>.5]
        for i in range(count):
            points=[center,ring[i],ring[(i+1)%count]]
            surfaces.append((points,[projected_uv(source,candidates,p) for p in points],False,True))
    return surface_triangles(surfaces,start,end),lower[0]
