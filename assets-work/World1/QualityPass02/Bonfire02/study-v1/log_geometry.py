"""Measured split-log sections: charred square hot ends and softened exposed heels."""
import math
from mathutils import Vector, geometry

SECTION_TIMES = (0.0, .10, .56, 1.0)
SECTION_CHAMFERS = (.19, .24, .17, 0.0)


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


def log_surfaces(source, group):
    lower, upper, start, end = ordered_ends(source, group)
    source_faces = [f for f in source.data.polygons if f.vertices[0] in group]
    axis = (end-start).normalized()
    side_faces = [f for f in source_faces if abs(f.normal.dot(axis))<.5]
    cap_faces = [f for f in source_faces if abs(f.normal.dot(axis))>=.5]
    heel = min(range(4),key=lambda i:lower[i].z)
    rings=[]
    for t,amount in zip(SECTION_TIMES,SECTION_CHAMFERS):
        corners=[a.lerp(b,t) for a,b in zip(lower,upper)];ring=[]
        for i,p in enumerate(corners):
            cut=0 if t==0 and i==heel else amount
            ring.extend((p.lerp(corners[(i-1)%4],cut),p.lerp(corners[(i+1)%4],cut)))
        rings.append(ring)
    surfaces=[]
    for section in range(len(rings)-1):
        for i in range(8):
            j=(i+1)%8
            polygon=[rings[section][i],rings[section][j],rings[section+1][j],rings[section+1][i]]
            mid=sum(polygon,Vector())/4
            nearby=sorted(side_faces,key=lambda f:abs(f.normal.dot(mid-source.data.vertices[f.vertices[0]].co)))[:2]
            surfaces.append((polygon,[projected_uv(source,nearby,p) for p in polygon],True))
    for ring,t in ((rings[0],0),(rings[-1],1)):
        center=sum(ring,Vector())/8
        candidates=[f for f in cap_faces if f.normal.dot(axis)*(1 if t else -1)>.5]
        for i in range(8):
            polygon=[center,ring[i],ring[(i+1)%8]]
            surfaces.append((polygon,[projected_uv(source,candidates,p) for p in polygon],False))
    result=[]
    for points,uv,smooth in surfaces:
        for indices in ((0,1,2),(0,2,3)) if len(points)==4 else ((0,1,2),):
            ps=[points[i] for i in indices];coords=[uv[i] for i in indices]
            normal=(ps[1]-ps[0]).cross(ps[2]-ps[0])
            if normal.length<1e-7:continue
            mid=sum(ps,Vector())/3;fraction=(mid-start).dot(axis)/(end-start).length
            outward=mid-start.lerp(end,min(1,max(0,fraction))) if smooth else axis*(1 if fraction>.5 else -1)
            if normal.dot(outward)<0:ps.reverse();coords.reverse()
            result.append((ps,coords,smooth))
    return result,lower[heel]
