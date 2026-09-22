"""Hand-traced connected stone relief planes registered to the frozen dragon painting."""
import math
from mathutils import Vector,geometry

BORDER_UNITS=8.0
BORDER_BLEND_UNITS=4.0
BED_DEPTH=14.0
# Negative offsets bring the feature forward; the existing nose remains the envelope limit.
LEFT_BROW=[(.14,.665),(.23,.705),(.33,.695),(.425,.655),(.43,.615),(.35,.64),(.25,.652),(.17,.635)]
LEFT_EYE=[(.205,.61),(.32,.62),(.399,.593),(.36,.56),(.245,.575)]
LEFT_CHEEK=[(.145,.49),(.24,.55),(.355,.52),(.40,.45),(.35,.39),(.27,.36),(.23,.31),(.13,.34),(.215,.425)]
NOSE=[(.45,.67),(.407,.60),(.413,.52),(.393,.43),(.365,.36),(.405,.303),(.47,.322),(.50,.365),(.53,.322),(.595,.303),(.635,.36),(.607,.43),(.587,.52),(.593,.60),(.55,.67)]
MOUTH=[(.30,.235),(.40,.28),(.48,.26),(.52,.26),(.60,.28),(.70,.235),(.62,.17),(.52,.15),(.48,.15),(.38,.17)]
FOREHEAD=[(.43,.725),(.475,.825),(.50,.90),(.525,.825),(.57,.725),(.55,.681),(.45,.681)]
FEATURES=[('nose',NOSE,0),('mouth',MOUTH,-5),('forehead',FOREHEAD,-8)]
for name,polygon,depth in [('brow',LEFT_BROW,-8),('eye',LEFT_EYE,23),('cheek',LEFT_CHEEK,-4)]:
    FEATURES.extend([(name+'_left',polygon,depth),(name+'_right',[(1-u,v) for u,v in polygon],depth)])


def inside(point,polygon):
    x,y=point;result=False
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:result=not result
    return result


def inset(polygon):
    center=sum((Vector(p) for p in polygon),Vector((0,0)))/len(polygon)
    return [tuple(center+(Vector(p)-center)*.73) for p in polygon]


def distance(point,polygon,scale):
    p=Vector((point[0]*scale[0],point[1]*scale[1]));minimum=float('inf')
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        x=Vector((a[0]*scale[0],a[1]*scale[1]));y=Vector((b[0]*scale[0],b[1]*scale[1]));edge=y-x
        t=min(1,max(0,(p-x).dot(edge)/edge.length_squared));minimum=min(minimum,(p-x-t*edge).length)
    return minimum


def depth(point,scale):
    margin=min((point[0]-.0005)*scale[0],(.9995-point[0])*scale[0],(point[1]-.0005)*scale[1],(.9995-point[1])*scale[1])
    strength=min(1,max(0,(margin-BORDER_UNITS)/BORDER_BLEND_UNITS))
    raised=BED_DEPTH;recessed=BED_DEPTH
    for name,polygon,target in FEATURES:
        if not inside(point,polygon):continue
        inner=inset(polygon)
        if inside(point,inner):value=target
        else:
            outer_distance=distance(point,polygon,scale);inner_distance=distance(point,inner,scale)
            mix=outer_distance/max(1e-9,outer_distance+inner_distance)
            value=BED_DEPTH*(1-mix)+target*mix
        if target<=BED_DEPTH:raised=min(raised,value)
        else:recessed=max(recessed,value)
    value=raised if raised<BED_DEPTH else recessed

    return value*strength


def baseline_point(uv,faces,source):
    for face in faces:
        coords=[source.data.uv_layers[0].data[i].uv.copy() for i in face.loop_indices]
        a,b,c=[Vector((*p,0)) for p in coords];point=Vector((*uv,0))
        weights=geometry.barycentric_transform(point,a,b,c,Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
        if min(weights)>=-1e-5:
            vertices=[source.data.vertices[i].co for i in face.vertices]
            return sum((p*w for p,w in zip(vertices,weights)),Vector())
    raise AssertionError(('UV outside baseline surface',list(uv)))


def sculpture(source):
    faces=[f for f in source.data.polygons if source.data.materials[f.material_index].get('mu_texture')=='c_wall06.jpg']
    points=[];edges=[];lookup={}
    def add(point):
        key=tuple(round(v,7) for v in point)
        if key not in lookup:lookup[key]=len(points);points.append(Vector(point))
        return lookup[key]
    def loop(polygon):
        ids=[add(p) for p in polygon]
        edges.extend((a,b) for a,b in zip(ids,ids[1:]+ids[:1]))
    for face in faces:loop([source.data.uv_layers[0].data[i].uv for i in face.loop_indices])
    xs=[v.co.x for v in source.data.vertices];zs=[v.co.z for v in source.data.vertices]
    scale=((max(xs)-min(xs))/.999,(max(zs)-min(zs))/.999)
    for band in (BORDER_UNITS,BORDER_UNITS+BORDER_BLEND_UNITS):
        u,v=band/scale[0],band/scale[1]
        loop([(.0005+u,.0005+v),(.9995-u,.0005+v),(.9995-u,.9995-v),(.0005+u,.9995-v)])
    for name,polygon,target in FEATURES:loop(polygon);loop(inset(polygon))
    vertices,_,triangles,*_=geometry.delaunay_2d_cdt(points,list(set(tuple(sorted(e)) for e in edges)),[],0,1e-7)
    positions=[]
    original_min=min(v.co.y for v in source.data.vertices)
    for uv in vertices:
        point=baseline_point(uv,faces,source)
        if min((uv-Vector(a)).length for a in ((.4497,.5007),(.5503,.5007)))>1e-5:point.y+=depth(uv,scale)
        assert point.y>=original_min-.0001,(list(uv),point.y,original_min)
        positions.append(point)
    ordered=[]
    for face in triangles:
        a,b,c=[vertices[i] for i in face]
        det=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)
        ordered.append(face if det>0 else list(reversed(face)))
    return positions,ordered,vertices,dict(border_units=BORDER_UNITS,blend_units=BORDER_BLEND_UNITS,scale=list(scale),features=[dict(name=n,outline=p,offset=d) for n,p,d in FEATURES])
