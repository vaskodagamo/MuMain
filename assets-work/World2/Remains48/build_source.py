"""Shape one detached long bone; preserve other components and placed supports."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode = True
import bpy
from mathutils import Vector, Euler
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from audit_exports import triangles

SECTORS = 8
BONE_FACE_COUNT = 32
PROFILE_Y = [-31.402397, -27.0583, -17.910599, -5, 7.646801, 16.728001, 19.7379]
PROFILE_RADIUS = [0, 4.5072, 3.10405, 2.65, 3.10405, 4.27335, 0]
PROFILE_CENTER = [-3.2642, -3.2642, -3.26415, -3.38, -3.50375, -3.25095, -5.9361]
RINGS_Y = [-30.6, -29.4, -27.0583, -23, -17.910599, -5, 7.646801, 12.5, 16.728001, 18.2, 19.2]
CENTER_Z = 7.56005


def smooth_profile(y, values):
    """Monotone cubic segments avoid overshooting measured end widths."""
    for i in range(len(PROFILE_Y)-1):
        if PROFILE_Y[i] <= y <= PROFILE_Y[i+1]:
            width = PROFILE_Y[i+1] - PROFILE_Y[i]
            t = (y-PROFILE_Y[i])/width
            slopes = [(values[k+1]-values[k])/(PROFILE_Y[k+1]-PROFILE_Y[k]) for k in range(len(values)-1)]
            def tangent(k):
                if k in (0, len(values)-1):
                    return slopes[0 if k == 0 else -1]
                a, b = slopes[k-1:k+1]
                return 0 if a*b <= 0 else 2*a*b/(a+b)
            return ((2*t**3-3*t*t+1)*values[i] + (t**3-2*t*t+t)*width*tangent(i)
                    + (-2*t**3+3*t*t)*values[i+1] + (t**3-t*t)*width*tangent(i+1))
    raise ValueError(y)


def support_planes(points, placements):
    planes, seen = [], set()
    for record in placements:
        rotation = Euler([math.radians(v) for v in record['rotation']], 'XYZ').to_matrix()
        for axis in range(3):
            for sign in (-1, 1):
                normal = Vector(rotation[axis])*sign
                key = tuple(round(v, 6) for v in normal)
                if key not in seen:
                    seen.add(key)
                    planes.append((normal, max(normal.dot(p) for p in points)))
    return planes


def clipped(point, center, planes):
    amount = 1.0
    for normal, limit in planes:
        step = normal.dot(point-center)
        if step > 1e-9:
            amount = min(amount, max(0, (limit-normal.dot(center))/step))
    return center + (point-center)*amount


def reference(obj):
    collection = bpy.data.collections.new('REF_ORIGINAL')
    bpy.context.scene.collection.children.link(collection)
    original = obj.copy()
    original.data = obj.data.copy()
    original['mu_reference'] = True
    collection.objects.link(original)
    collection.hide_render = collection.hide_viewport = True


def uv(point, rows):
    """Keep the original projected bone-paint strip and its longitudinal direction."""
    for _, corners in rows[:BONE_FACE_COUNT]:
        for corner in corners:
            if (point-Vector(corner[1:4])).length < .000001:
                return Vector(corner[7:9])
    return Vector((.7811-(point.x+3.2642)*.01035, .4993+(point.y+31.402397)*.009374))


def bone_faces(rows, planes):
    original = [Vector(v[1:4]) for _, values in rows[:BONE_FACE_COUNT] for v in values]
    rings = []
    for y in RINGS_Y:
        center = Vector((smooth_profile(y, PROFILE_CENTER), y, CENTER_Z))
        radius = smooth_profile(y, PROFILE_RADIUS)
        ring = []
        for sector in range(SECTORS):
            angle = 2*math.pi*sector/SECTORS
            point = center + Vector((math.cos(angle)*radius, 0, math.sin(angle)*radius))
            # Original cardinal points remain exact where they define old support planes.
            anchor = min(original, key=lambda old: (old-point).length)
            point = anchor.copy() if (anchor-point).length < .0003 else clipped(point, center, planes)
            ring.append(point)
        rings.append(ring)
    faces = []
    def add(points):
        faces.append(([(p, uv(p, rows)) for p in points], None))
    first, last = Vector(rows[0][1][0][1:4]), Vector(rows[28][1][0][1:4])
    for i in range(SECTORS):
        j = (i+1)%SECTORS
        add([first, rings[0][i], rings[0][j]])
        for lower, upper in zip(rings, rings[1:]):
            add([lower[i], upper[i], upper[j]])
            add([lower[i], upper[j], lower[j]])
        add([last, rings[-1][j], rings[-1][i]])
    return faces


def create_mesh(obj, faces, rows):
    vertices, polygons, lookup = [], [], {}
    for corners, old in faces:
        polygon = []
        for point, texcoord in corners:
            key = tuple(point)
            if key not in lookup:
                lookup[key] = len(vertices)
                vertices.append(obj.matrix_world.inverted()@point)
            polygon.append(lookup[key])
        polygons.append(polygon)
    mesh = bpy.data.meshes.new('OneLongBoneRoundedShaftAndFullEnds')
    mesh.from_pydata(vertices, [], polygons)
    for material in obj.data.materials:
        mesh.materials.append(material)
    obj.data = mesh
    obj.vertex_groups.clear()
    obj.vertex_groups.new(name='Box14').add(list(range(len(vertices))), 1, 'REPLACE')
    layer = mesh.uv_layers.new(name='UVMap')
    mesh.update()
    neighbors = {}
    for face, (corners, old) in zip(mesh.polygons, faces):
        face.use_smooth = True
        for loop, (point, texcoord) in zip(face.loop_indices, corners):
            layer.data[loop].uv = texcoord
            if old is None:
                neighbors.setdefault(mesh.loops[loop].vertex_index, []).append(face.normal.copy())
    normals = []
    for face, (corners, old) in zip(mesh.polygons, faces):
        for corner, loop in enumerate(face.loop_indices):
            if old is not None:
                normals.append(Vector(rows[old][1][corner][4:7]))
            else:
                normals.append(sum(neighbors[mesh.loops[loop].vertex_index], Vector()).normalized())
    mesh.normals_split_custom_set(normals)
    return mesh


def main():
    folder = ROOT/'Object48'
    rows = triangles(folder/'validation/baseline/Object48.smd')
    bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    bpy.context.scene.frame_set(0)
    obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.get('mu_helper'))
    reference(obj)
    points = [Vector(row[1:4]) for _, values in rows[:BONE_FACE_COUNT] for row in values]
    planes = support_planes(points, json.loads((folder/'placements.json').read_text()))
    faces = bone_faces(rows, planes)
    for index in range(BONE_FACE_COUNT, len(rows)):
        faces.append(([(Vector(v[1:4]), Vector(v[7:9])) for v in rows[index][1]], index))
    mesh = create_mesh(obj, faces, rows)
    bpy.ops.file.pack_all()
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    (folder/'validation/source.json').write_text(json.dumps(dict(
        triangles=len(mesh.polygons), protected_triangles=list(range(BONE_FACE_COUNT, len(rows))),
        edited_original_faces=list(range(BONE_FACE_COUNT)), support_planes=len(planes),
        actual_placements=385, design='Single eight-sided tapered long bone with fuller rounded ends; seven other components unchanged'), indent=2))


if __name__ == '__main__':
    main()
