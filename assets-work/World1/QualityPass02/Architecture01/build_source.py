"""Model timber framing and actual shingle laps, inside existing modular envelopes."""
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
NAMES = ('HouseWall01', 'HouseWall04', 'HouseWall05', 'HouseWall06')
TIMBER = 'tile_wood02.jpg'
ROOF = 'tile_wood03.jpg'
CHAMFER = 1.6
LAP_DEPTH = 3.0
COURSE_V = (.222, .444, .655, .869)
EDGE_MARGIN = 5.0
U_STEP = .25


def meshes():
    return [o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.get('mu_helper') and not o.get('mu_reference')]


def reference(obj, collection):
    clone = obj.copy()
    clone.data = obj.data.copy()
    clone.name = collection.name + '_' + obj.name
    clone['mu_reference'] = True
    clone.hide_render = True
    collection.objects.link(clone)
    return clone


def reference_collection(name):
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    collection.hide_render = True
    collection.hide_viewport = True
    return collection


class Builder:
    """Build oriented, chamfered structural members with grain following their length."""
    def __init__(self, obj):
        self.obj = obj
        self.vertices, self.faces, self.uvs, self.materials = [], [], [], []

    def face(self, points, uvs, material):
        first = len(self.vertices)
        self.vertices.extend(points)
        for corner in range(1, len(points) - 1):
            edge_a = Vector(points[corner]) - Vector(points[0])
            edge_b = Vector(points[corner + 1]) - Vector(points[0])
            if edge_a.cross(edge_b).length < .00001:
                continue
            self.faces.append((first, first + corner, first + corner + 1))
            self.uvs.append([uvs[0], uvs[corner], uvs[corner + 1]])
            self.materials.append(material)

    def beam(self, a, b, width, depth, outward, phase=0):
        a, b, normal = Vector(a), Vector(b), Vector(outward)
        direction = (b - a).normalized()
        side = direction.cross(normal).normalized()
        half_w, half_d = width / 2, depth / 2
        bevel = min(CHAMFER, half_w / 3, half_d / 3)
        section = [(-half_w+bevel,-half_d),(half_w-bevel,-half_d),(half_w,-half_d+bevel),(half_w,half_d-bevel),(half_w-bevel,half_d),(-half_w+bevel,half_d),(-half_w,half_d-bevel),(-half_w,-half_d+bevel)]
        section.reverse()
        rings = [[p + side * x + normal * y for x,y in section] for p in (a,b)]
        grain_length = (b-a).length / 128
        for i in range(len(section)):
            j = (i+1) % len(section)
            self.face([rings[0][i],rings[0][j],rings[1][j],rings[1][i]], [(phase,.06),(phase,.22),(phase+grain_length,.22),(phase+grain_length,.06)], TIMBER)
        self.face(list(reversed(rings[0])), [(x/width+.5,y/depth+.5) for x,y in reversed(section)], TIMBER)
        self.face(rings[1], [(x/width+.5,y/depth+.5) for x,y in section], TIMBER)

    def install(self):
        mesh = bpy.data.meshes.new('AuthoredJoinery')
        local = [self.obj.matrix_world.inverted() @ Vector(p) for p in self.vertices]
        mesh.from_pydata(local, [], self.faces)
        mesh.update()
        material_names = [m.name for m in self.obj.data.materials]
        for material in self.obj.data.materials:
            mesh.materials.append(material)
        layer = mesh.uv_layers.new(name=self.obj.data.uv_layers[0].name)
        for face, coordinates, material in zip(mesh.polygons, self.uvs, self.materials):
            face.material_index = material_names.index(material)
            for loop, uv in zip(face.loop_indices, coordinates):
                layer.data[loop].uv = uv
        obj = bpy.data.objects.new('StructuralJoinery', mesh)
        bpy.context.scene.collection.objects.link(obj)
        obj.matrix_world = self.obj.matrix_world.copy()
        obj.parent = self.obj.parent
        group = obj.vertex_groups.new(name=self.obj.vertex_groups[0].name)
        group.add(list(range(len(mesh.vertices))), 1, 'REPLACE')
        for modifier in self.obj.modifiers:
            if modifier.type == 'ARMATURE':
                new = obj.modifiers.new('OriginalRig', 'ARMATURE')
                new.object = modifier.object
        return obj


def wall_frame(builder, start, end, outward):
    """Paired posts, elbow braces and stepped foot collars form a legible bay."""
    start, end, outward = Vector(start), Vector(end), Vector(outward)
    along = (end-start).normalized()
    for i, position in enumerate((start, end)):
        a, b = position.copy(), position.copy()
        a.z, b.z = 74.4, 239.2
        builder.beam(a,b,15,8,outward,phase=i*.31)
        inward = along if i == 0 else -along
        low, high = position.copy(), position + inward * 35
        low.z, high.z = 205, 238
        builder.beam(low,high,11,7,outward,phase=.18)
        foot, top = position.copy(), position.copy()
        foot.z, top.z = 75, 90
        builder.beam(foot,top,21,10,outward,phase=.4)
    a,b = start.copy(),end.copy()
    a.z,b.z = 100,100
    builder.beam(a,b,9,6,outward,phase=.24)


def model_walls(name, builder):
    if name == 'HouseWall01':
        wall_frame(builder,(-78,9.0,0),(78,9.0,0),(0,-1,0))
        wall_frame(builder,(-78,43.8,0),(78,43.8,0),(0,1,0))
        return
    wall_frame(builder,(32,9.0,0),(218,9.0,0),(0,-1,0))
    wall_frame(builder,(4.9,42,0),(4.9,222,0),(-1,0,0))
    wall_frame(builder,(54,43.9,0),(218,43.9,0),(0,1,0))
    wall_frame(builder,(39.2,62,0),(39.2,222,0),(1,0,0))


def clip_polygon(polygon, axis, limit, keep_above):
    result=[]
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        inside_a = a[1][axis] >= limit if keep_above else a[1][axis] <= limit
        inside_b = b[1][axis] >= limit if keep_above else b[1][axis] <= limit
        if inside_a:
            result.append(a)
        if inside_a != inside_b:
            fraction = (limit-a[1][axis])/(b[1][axis]-a[1][axis])
            result.append((a[0].lerp(b[0],fraction),a[1].lerp(b[1],fraction)))
    return result


def roof_surface(builder, polygon, bottom, top, high, bounds):
    low_u,high_u=min(uv.x for _,uv in polygon),max(uv.x for _,uv in polygon)
    steps=sorted(set([low_u,high_u]+[i*U_STEP for i in range(-30,31) if low_u<i*U_STEP<high_u]))
    for left,right in zip(steps,steps[1:]):
        clipped=clip_polygon(clip_polygon(polygon,0,left,True),0,right,False)
        if len(clipped)<3: continue
        points=[]
        for position,uv in clipped:
            edge_distance=min(position.x-bounds[0][0],bounds[1][0]-position.x,position.y-bounds[0][1],bounds[1][1]-position.y)
            margin=max(0,min(1,edge_distance/EDGE_MARGIN))
            fraction=(uv.y-bottom)/(top-bottom)
            depth=LAP_DEPTH*fraction*margin if top<high-.0001 else 0
            points.append(position-Vector((0,0,depth)))
        builder.face(points,[uv for _,uv in clipped],ROOF)
        for i,(position,uv) in enumerate(clipped):
            j=(i+1)%len(clipped)
            next_position,next_uv=clipped[j]
            if abs(uv.y-top)<.00001 and abs(next_uv.y-top)<.00001 and top<high-.0001:
                if (points[i]-position).length+(points[j]-next_position).length<.00001: continue
                builder.face([points[i],points[j],next_position,position],[uv,next_uv,next_uv,uv],ROOF)


def roof_courses(obj, builder):
    """Create true paint-aligned lap surfaces, preserving the outer modular perimeter."""
    remove=[]
    roof_faces=[face for face in obj.data.polygons if obj.data.materials[face.material_index].name==ROOF]
    vertices=[obj.matrix_world@obj.data.vertices[i].co for face in roof_faces for i in face.vertices]
    bounds=[[min(p[a] for p in vertices) for a in range(3)],[max(p[a] for p in vertices) for a in range(3)]]
    for face in roof_faces:
        remove.append(face.index)
        polygon=[(obj.matrix_world @ obj.data.vertices[obj.data.loops[i].vertex_index].co, obj.data.uv_layers[0].data[i].uv.copy()) for i in face.loop_indices]
        low,high=min(uv.y for _,uv in polygon),max(uv.y for _,uv in polygon)
        breaks=sorted(set([low,high]+[v for v in COURSE_V if low+.01<v<high-.01]))
        for bottom,top in zip(breaks,breaks[1:]):
            clipped=clip_polygon(clip_polygon(polygon,1,bottom,True),1,top,False)
            if len(clipped)>=3: roof_surface(builder,clipped,bottom,top,high,bounds)
    return remove


def remove_faces(obj, indices):
    import bmesh
    mesh=bmesh.new()
    mesh.from_mesh(obj.data)
    mesh.faces.ensure_lookup_table()
    bmesh.ops.delete(mesh,geom=[mesh.faces[i] for i in indices],context='FACES_ONLY')
    mesh.to_mesh(obj.data)
    mesh.free()


def build(name):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    obj=meshes()[0]
    baseline=reference_collection('REF_BASELINE')
    reference(obj,baseline)
    original=reference_collection('REF_ORIGINAL')
    with bpy.data.libraries.load(str(folder/'original/source.blend')) as (data,loaded):
        loaded.objects=data.objects
    for item in loaded.objects:
        if item.type=='MESH' and not item.get('mu_helper'):
            item.parent=None
            item.modifiers.clear()
            item['mu_reference']=True
            original.objects.link(item)
            item.hide_render=True
    builder=Builder(obj)
    if name in NAMES[:2]:
        model_walls(name,builder)
    else:
        remove_faces(obj,roof_courses(obj,builder))
    addition=builder.install()
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    addition.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.join()
    # Triangulate the authored geometry without changing original face orientations.
    import bmesh
    bm=bmesh.new(); bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data); bm.free()
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    report=dict(triangles=len(obj.data.polygons),vertices=len(obj.data.vertices),design='Chamfered structural joinery' if name in NAMES[:2] else 'Paint-aligned recessed shingle laps',new_textures=False)
    (folder/'validation/authored.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    for name in NAMES:
        build(name)
