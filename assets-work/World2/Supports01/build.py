"""Author bounded capital setbacks and a shallow collar arris through Blender API."""
from pathlib import Path
import json
import sys
import bpy
from mathutils import Vector

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
NAMES = ('Object06', 'Object13', 'Object15')
SHOULDER = {2, 3, 4, 12, 13, 14, 15, 22, 23, 24, 31, 32, 33}
SHOULDER_PROFILE = [(-96.882, 0), (-70, 3), (-45, 8), (-35, 1), (-17.052, 0)]


def interpolate(a, b, t):
    return tuple(x.lerp(y, t) for x,y in zip(a,b))


def clip(poly, z, above):
    output = []
    for a,b in zip(poly, poly[1:]+poly[:1]):
        inside_a = a[0].z >= z if above else a[0].z <= z
        inside_b = b[0].z >= z if above else b[0].z <= z
        if inside_a:
            output.append(a)
        if inside_a != inside_b:
            output.append(interpolate(a,b,(z-a[0].z)/(b[0].z-a[0].z)))
    return output


def offset(z, profile):
    if min(abs(z-profile[0][0]), abs(z-profile[-1][0])) < .005:
        return 0
    for (lo,a),(hi,b) in zip(profile, profile[1:]):
        if lo <= z <= hi:
            return a+(b-a)*(z-lo)/(hi-lo)
    return 0


def shaped(corner, profile):
    position, uv, normal = (item.copy() for item in corner)
    inset = offset(position.z, profile)
    radius = max(abs(position.x-.022), abs(position.y))
    if radius:
        factor = (radius-inset)/radius
        position.x = .022+(position.x-.022)*factor
        position.y *= factor
    return position, uv, normal


def split_face(corners, profile):
    result = []
    cuts = [-10000, *[z for z,_ in profile[1:-1]], 10000]
    for lo,hi in zip(cuts,cuts[1:]):
        poly = clip(clip(corners,lo,True),hi,False)
        poly = [shaped(c,profile) for c in poly]
        for i in range(1,len(poly)-1):
            tri = [poly[0],poly[i],poly[i+1]]
            normal = (tri[1][0]-tri[0][0]).cross(tri[2][0]-tri[0][0])
            if normal.length < 1e-6:
                continue
            normal.normalize()
            result.append([(p,uv,normal) for p,uv,_ in tri])
    return result


def reference(obj):
    collection = bpy.data.collections.new('REF_ORIGINAL')
    bpy.context.scene.collection.children.link(collection)
    collection.vs.export = False
    duplicate = obj.copy()
    duplicate.data = obj.data.copy()
    duplicate['mu_reference'] = True
    collection.objects.link(duplicate)
    collection.hide_render = collection.hide_viewport = True


def rebuild(obj, records):
    original = obj.data
    names = [g.name for g in obj.vertex_groups]
    positions, faces, lookup = [], [], {}
    inverse = obj.matrix_world.inverted()
    for _,corners,_ in records:
        indices = []
        for position,_,_ in corners:
            key = tuple(round(v,7) for v in position)
            if key not in lookup:
                lookup[key] = len(positions)
                positions.append(inverse @ position)
            indices.append(lookup[key])
        faces.append(indices)
    mesh = bpy.data.meshes.new('AuthoredSupport')
    mesh.from_pydata(positions,[],faces)
    for material in original.materials:
        mesh.materials.append(material)
    uv = mesh.uv_layers.new(name='UVMap')
    normals = []
    for face,(material,corners,_) in zip(mesh.polygons,records):
        face.material_index = material
        face.use_smooth = True
        for loop,(_,coordinate,normal) in zip(face.loop_indices,corners):
            uv.data[loop].uv = coordinate
            normals.append(inverse.to_3x3() @ normal)
    for edge in mesh.edges:
        edge.use_edge_sharp = True
    mesh.normals_split_custom_set(normals)
    obj.data = mesh
    obj.vertex_groups.clear()
    assert len(names) == 1
    obj.vertex_groups.new(name=names[0]).add(list(range(len(positions))),1,'REPLACE')


def build(name):
    folder = ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    bpy.context.scene.frame_set(0)
    obj = next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper'))
    reference(obj)
    old = obj.data
    normals = [obj.matrix_world.to_3x3() @ n.vector for n in old.corner_normals]
    records, protected = [], []
    for face in old.polygons:
        corners = [(obj.matrix_world @ old.vertices[old.loops[i].vertex_index].co,
                    old.uv_layers[0].data[i].uv.copy(), normals[i]) for i in face.loop_indices]
        selected = face.index in SHOULDER if name != 'Object15' else True
        if selected:
            profile = SHOULDER_PROFILE if name != 'Object15' else [(-17.181,0),(-7.0,2.2),(-.130,0)]
            records.extend((face.material_index,tri,face.index) for tri in split_face(corners,profile))
        else:
            protected.append(face.index)
            records.append((face.material_index,corners,face.index))
    rebuild(obj,records)
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    report = dict(original_triangles=len(old.polygons), candidate_triangles=len(records),
                  protected_original_faces=protected, source_original_face=[r[2] for r in records],
                  profile=SHOULDER_PROFILE if name!='Object15' else 'single inset arris at z=-7, depth2.2',
                  upper_support_and_cap='unchanged', opening='original complete boundary retained')
    (folder/'validation/authored.json').write_text(json.dumps(report,indent=2)+'\n')


for name in NAMES:
    build(name)
