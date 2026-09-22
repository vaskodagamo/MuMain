"""Visible masonry surrounds and load-bearing canopy timber additions."""
from mathutils import Vector

STONE_UV_SCALE=180.0


def block(output,common,center,size,material,bone=0):
    # Chamfered rectangular masonry block; bevel creates visible edge planes.
    x,y,z=center;w,d,h=size
    bevel=min(2.6,w/5,d/5,h/5)
    ring=[(-w/2+bevel,-d/2),(w/2-bevel,-d/2),(w/2,-d/2+bevel),(w/2,d/2-bevel),(w/2-bevel,d/2),(-w/2+bevel,d/2),(-w/2,d/2-bevel),(-w/2,-d/2+bevel)]
    rings=[[Vector((x+a,y+b,z+height)) for a,b in ring] for height in (-h/2,h/2)]
    for i in range(len(ring)):
        j=(i+1)%len(ring)
        points=[rings[0][i],rings[0][j],rings[1][j],rings[1][i]]
        distance=(points[1]-points[0]).length/STONE_UV_SCALE
        common.emit(output,points,[Vector((0,.2)),Vector((distance,.2)),Vector((distance,.2+h/STONE_UV_SCALE)),Vector((0,.2+h/STONE_UV_SCALE))],bone,material)
    for points in (list(reversed(rings[0])),rings[1]):
        common.emit(output,points,[Vector((p.x/STONE_UV_SCALE,p.y/STONE_UV_SCALE)) for p in points],bone,material)


def window_surrounds(output,common,obj,name):
    material=[m.name for m in obj.data.materials].index('tile_ston04.jpg')
    # Window front planes and surrounds; corners are clear of building mating edges.
    windows=[('x',190.56,63.56,1),('x',-193.09,127.345,-1),('x',-193.09,-113.825,-1),('y',286.99,23.755,1)] if name=='House01' else [('x',91.86,14.455,1)]
    for axis,plane,across,sign in windows:
        def masonry(offset,z,width,height,depth,projection):
            center=(plane+sign*projection,across+offset,z) if axis=='x' else (across+offset,plane+sign*projection,z)
            size=(depth,width,height) if axis=='x' else (width,depth,height)
            block(output,common,center,size,material)
        masonry(0,121,111,13,15 if axis=='y' else 21,4 if axis=='y' else 8)
        masonry(0,213,106,14,15,5)
        for side in (-1,1):
            for z in (140,166,192):masonry(side*48,z,13,25,15,4)
    if name=='House01':
        # Two pier strips flank the existing door opening without entering its approach.
        for y in (-213,-48):
            for z in (77,108,139,170):block(output,common,(187,y,z),(13,20,29),material)


def canopy_structure(output,common,obj):
    common.arch.TIMBER='tile_ston06.jpg'
    builder=common.arch.Builder(obj)
    # Transverse headers follow the original irregular canopy height at post lines.
    for a,b in (((-240.6,-420.5,267),(15.4,-420.5,281)),((15.4,-420.5,281),(281.6,-420.5,234)),((-240.9,-181.2,274),(15.2,-181.2,286)),((15.2,-181.2,286),(281.4,-181.2,250))):
        builder.beam(a,b,18,17,(0,0,1))
    for x,y,z in ((281.6,-420.5,234),(281.4,-181.2,250),(-240.6,-420.5,267),(-240.9,-181.2,274),(15.4,-420.5,281),(15.2,-181.2,286)):
        direction=1 if y<-300 else -1
        builder.beam((x,y,z-76),(x,y+direction*66,z-9),19,17,(1,0,0))
        builder.beam((x,y,z-15),(x,y,z+3),27,25,(1,0,0))
    material=[m.name for m in obj.data.materials].index('tile_ston06.jpg')
    for face,uvs in zip(builder.faces,builder.uvs):
        common.emit(output,[Vector(builder.vertices[i]) for i in face],[Vector((.04+u*.27,v)) for u,v in uvs],0,material)
