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

def vessel(profile,current):
    matrix=profile['matrix'];inverse=matrix.inverted();levels=profile['levels']
    ring_points=[points for v,points in levels]
    centers=[sum(points,Vector())/5 for points in ring_points]
    heights=[c.z for c in centers]
    radii=[sum((p-c).xy.length for p in points)/5 for c,points in zip(centers,ring_points)]
    local=[inverse@p for p in current];bottom=min(p.z for p in local)
    base=sorted([p for p in local if p.z<bottom+.001],key=lambda p:math.atan2(p.y-centers[0].y,p.x-centers[0].x))
    assert len(base)==10,len(base)
    angles=[math.atan2(p.y-centers[0].y,p.x-centers[0].x) for p in base]
    # Tall pitchers need an extra neck curve; shorter storage pots remain economical.
    tall=levels[0][0]<.1 or levels[0][0]>.8
    samples=sorted(heights+[(heights[0]+heights[1])/2]+([(heights[1]+heights[2])/2] if tall else []))
    vertices=[];coords=[];faces=[]
    control=[Vector((r,c.x,c.y)) for r,c in zip(radii,centers)]
    top=ring_points[-1];top_angles=[math.atan2(p.y-centers[-1].y,p.x-centers[-1].x) for p in top]
    def rim_height(angle):
        # Retain the pouring spout depression of the small pitcher.
        nearest=min(range(5),key=lambda i:abs(math.atan2(math.sin(angle-top_angles[i]),math.cos(angle-top_angles[i]))))
        d=abs(math.atan2(math.sin(angle-top_angles[nearest]),math.cos(angle-top_angles[nearest])))
        return heights[-1]+(top[nearest].z-heights[-1])*max(0,1-d/(math.pi*.4))
    for z in samples:
        values=evaluate(heights,control,z);radius,cx,cy=values
        v=evaluate(heights,[Vector((level[0],)*3) for level in levels],z)[0]
        for j,a in enumerate(angles):
            if z==samples[0]:p=base[j].copy()
            else:p=Vector((cx+radius*math.cos(a),cy+radius*math.sin(a),rim_height(a) if z==samples[-1] else z))
            vertices.append(p);coords.append((a/(2*math.pi)+.75,v))
    # A rounded inward lip and a vertical throat give the vessel real wall thickness.
    thickness=min(2.2,radii[-1]*.14)
    for ring in range(2):
        for a in angles:
            radius=radii[-1]-thickness*(.65 if ring==0 else 1)
            z=rim_height(a)-(thickness*.20 if ring==0 else thickness*2)
            vertices.append(Vector((centers[-1].x+radius*math.cos(a),centers[-1].y+radius*math.sin(a),z)))
            if ring==0:coords.append((a/(2*math.pi)+.75,levels[-1][0]+.004))
            else:coords.append((profile['interior_uv'][0]+.006*math.cos(a),profile['interior_uv'][1]+.006*math.sin(a)))
    rings=len(samples)+2
    for ring in range(rings-1):
        for j in range(10):
            k=(j+1)%10;a=ring*10+j;b=ring*10+k;c=(ring+1)*10+k;d=(ring+1)*10+j
            faces.extend(((a,b,c),(c,d,a)))
    # Interior funnel and bottom closure use a tiny, noncollapsed atlas patch.
    for ring,z,reverse in ((rings-1,heights[-1]-(heights[-1]-heights[-2])*.65,False),(0,bottom,True)):
        center=len(vertices);vertices.append(Vector((centers[-1 if ring else 0].x,centers[-1 if ring else 0].y,z)))
        coords.append(tuple(profile['interior_uv']) if ring else (.25,levels[0][0]+.002))
        for j in range(10):
            face=(ring*10+j,ring*10+(j+1)%10,center)
            faces.append(face[::-1] if reverse else face)
    world=[matrix@p for p in vertices];before,after=bounds(current),bounds(world)
    for p in world:
        for k in range(3):p[k]=before[0][k]+(p[k]-after[0][k])*(before[1][k]-before[0][k])/(after[1][k]-after[0][k])
    for i,p in enumerate(base):world[i]=matrix@p
    contacts=[p for p in current if abs(p.z-before[0][2])<.0001]
    used=set()
    for point in contacts:
        index=min((i for i in range(len(world)) if i not in used),key=lambda i:(world[i]-point).length)
        world[index]=point.copy();used.add(index)
    assert max(abs(a-b) for aa,bb in zip(before,bounds(world)) for a,b in zip(aa,bb))<.0001
    return world,faces,coords,dict(triangles=len(faces),ground_vertices=len(contacts),base_ring_vertices=len(base),bounds=before,tall=tall)
