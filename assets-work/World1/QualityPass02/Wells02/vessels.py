"""Measured vessel profiles: retain ground ring, replace pentagonal funnels with lips."""
import math
from collections import Counter
from mathutils import Vector,Matrix,Euler
from curves import evaluate

def connected(vertices,faces):
    edges={i:set() for i in range(len(vertices))}
    for face in faces:
        for i in face:edges[i].update(face)
    unseen=set(edges)
    while unseen:
        pending=[unseen.pop()];group=set()
        while pending:
            i=pending.pop();group.add(i);extra=edges[i]&unseen;unseen-=extra;pending.extend(extra)
        yield group

def original_profiles(path):
    text=path.read_text();nodes={}
    for line in text.split('nodes\n')[1].split('\nend')[0].splitlines():
        i,name,parent=line.split();nodes[int(i)]=name.strip('"')
    poses={}
    for line in text.split('skeleton\n')[1].split('\nend')[0].splitlines()[1:]:
        values=list(map(float,line.split()));poses[nodes[int(values[0])]]=Matrix.Translation(Vector(values[1:4]))@Euler(values[4:],'XYZ').to_matrix().to_4x4()
    lines=text.split('triangles\n')[1].splitlines()[:-1];by_bone={}
    for i in range(0,len(lines),4):
        if lines[i]!='jar_01.jpg':continue
        rows=[list(map(float,line.split())) for line in lines[i+1:i+4]]
        by_bone.setdefault(nodes[int(rows[0][0])],[]).append(rows)
    result={}
    for bone,triangles in by_bone.items():
        keys=list(dict.fromkeys(tuple(row[1:4]) for face in triangles for row in face));lookup={k:i for i,k in enumerate(keys)}
        faces=[[lookup[tuple(row[1:4])] for row in face] for face in triangles]
        body=next(g for g in connected(keys,faces) if len(g)==21)
        inverse=poses[bone].inverted();levels={}
        for rows,indices in zip(triangles,faces):
            if indices[0] not in body:continue
            a,b,c=[Vector(row[7:9]) for row in rows]
            if abs((b-a).cross(c-a))<1e-10:continue
            for row in rows:
                levels.setdefault(round(row[8],4),{})[tuple(row[1:4])]=inverse@Vector(row[1:4])
        levels=sorted((v,list(points.values())) for v,points in levels.items() if len(points)==5)
        assert len(levels)==4,(bone,[(v,len(p)) for v,p in levels])
        outer=[p for v,points in levels for p in points]
        inner_key=next(keys[i] for i in body if min((inverse@Vector(keys[i])-p).length for p in outer)>.0001)
        inside_uv=next(row[7:9] for face in triangles for row in face if tuple(row[1:4])==inner_key)
        result[bone]=dict(matrix=poses[bone],levels=levels,interior_uv=inside_uv)
    return result

def bounds(points):
    return [[min(p[k] for p in points) for k in range(3)],[max(p[k] for p in points) for k in range(3)]]

def vessel(profile,current,neck,canonical):
    matrix=profile['matrix'];inverse=matrix.inverted();levels=canonical['levels']
    center=sum(levels[-2][1],Vector())/5
    top=sorted(levels[-1][1],key=lambda p:math.atan2(p.y-center.y,p.x-center.x))
    outer=[]
    for i,p in enumerate(top):
        q=top[(i+1)%5];outer.append(p.copy())
        pa=math.atan2(p.y-center.y,p.x-center.x);qa=math.atan2(q.y-center.y,q.x-center.x)
        if qa<pa:qa+=2*math.pi
        r0=(p-center).xy.length;r1=(q-center).xy.length
        if max(r0,r1)>min(r0,r1)*1.15:mid=(p+q)/2
        else:
            a=(pa+qa)/2;r=(r0+r1)/2;mid=Vector((center.x+r*math.cos(a),center.y+r*math.sin(a),(p.z+q.z)/2))
        outer.append(mid)
    # Canonical start angle avoids an integer UV wrap change between instances.
    outer.sort(key=lambda p:math.atan2(p.y-center.y,p.x-center.x))
    local_neck=[(inverse@p,uv) for p,uv in neck];available=set(range(len(local_neck)))
    vertices=[];coords=[]
    for point in outer:
        a=math.atan2(point.y-center.y,point.x-center.x)
        index=min(available,key=lambda i:abs(math.atan2(math.sin(math.atan2(local_neck[i][0].y-center.y,local_neck[i][0].x-center.x)-a),math.cos(math.atan2(local_neck[i][0].y-center.y,local_neck[i][0].x-center.x)-a))))
        available.remove(index);p,uv=local_neck[index];vertices.append(p);coords.append(tuple(uv))
    radius=sum((p-center).xy.length for p in top)/5;thickness=min(2.2,radius*.14)
    for ring in range(3):
        for point in outer:
            offset=point-center;offset.z=0;a=math.atan2(offset.y,offset.x)
            p=point.copy()
            if ring:p-=offset.normalized()*thickness*(.65 if ring==1 else 1);p.z-=thickness*(.2 if ring==1 else 2)
            vertices.append(p)
            if ring<2:coords.append((a/(2*math.pi)+.75,levels[-1][0]+(.004 if ring else 0)))
            else:coords.append((canonical['interior_uv'][0]+.006*math.cos(a),canonical['interior_uv'][1]+.006*math.sin(a)))
    faces=[]
    for row in range(3):
        for j in range(10):
            a=row*10+j;b=row*10+(j+1)%10;c=b+10;d=a+10
            faces.extend(((a,b,c),(c,d,a)))
    k=len(vertices);inside=center.copy();inside.z=sum(p.z for p in top)/5-(sum(p.z for p in top)/5-center.z)*.65
    vertices.append(inside);coords.append(tuple(canonical['interior_uv']))
    for j in range(10):faces.append((30+j,30+(j+1)%10,k))
    world=[matrix@p for p in vertices]
    # Join ring copies actual baseline positions without fitting the new mouth.
    for i,p in enumerate(vertices[:10]):world[i]=matrix@p
    return world,faces,coords,dict(triangles=len(faces),baseline_neck_vertices=10,canonical_section_triangles=50,canonical_profile_v=levels[0][0])
