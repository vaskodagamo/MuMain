"""Broad hand-placed carving planes registered to the existing dragon diffuse painting."""
import math
from mathutils import Vector

# UV centres and elliptical extents follow the painted eye, cheek and muzzle recesses.
SOCKETS = ((.29, .625, .14, .045, 6), (.71, .625, .14, .045, 6),
           (.23, .42, .12, .12, 5), (.77, .42, .12, .12, 5),
           (.38, .27, .055, .075, 4), (.62, .27, .055, .075, 4),
           (.50, .15, .20, .045, 4))
SUBDIVISIONS = 5


def carving_depth(uv):
    u, v = uv
    edge_mask = min(1, max(0, min(u, v, 1-u, 1-v) / .1))
    depth = 0
    for cx, cy, rx, ry, strength in SOCKETS:
        distance = ((u-cx)/rx)**2 + ((v-cy)/ry)**2
        depth += strength * max(0, 1-distance)**2
    return depth * edge_mask


def carved_geometry(geometry, original):
    vertices, bindings, faces, uvs, slots, normals = geometry
    out_faces, out_uvs, out_slots, out_normals = [], [], [], []
    maximum_y = max(p.y for p in vertices)
    for face, tex, slot, corner in zip(faces, uvs, slots, normals):
        if not original.data.materials[slot].name.endswith('c_wall06.jpg'):
            out_faces.append(face); out_uvs.append(tex); out_slots.append(slot); out_normals.append(corner)
            continue
        points = [vertices[index] for index in face]
        table = {}
        for i in range(SUBDIVISIONS + 1):
            for j in range(SUBDIVISIONS + 1 - i):
                weights = (1-(i+j)/SUBDIVISIONS, i/SUBDIVISIONS, j/SUBDIVISIONS)
                uv = sum((Vector(value)*weight for value, weight in zip(tex, weights)), Vector((0, 0)))
                point = sum((value*weight for value, weight in zip(points, weights)), Vector())
                # Recess toward the block; preserve module silhouette and maximum nose projection.
                depth = carving_depth(uv)
                # Preserve every source triangle corner; seams use the same UV-defined offset.
                point.y = min(point.y + depth, maximum_y)
                table[i,j] = (len(vertices), uv)
                vertices.append(point)
                bindings.append(bindings[face[0]])
        for i in range(SUBDIVISIONS):
            for j in range(SUBDIVISIONS-i):
                cells = [((i,j), (i+1,j), (i,j+1))]
                if i+j < SUBDIVISIONS-1:
                    cells.append(((i+1,j), (i+1,j+1), (i,j+1)))
                for keys in cells:
                    indices = [table[k][0] for k in keys]
                    normal = (vertices[indices[1]]-vertices[indices[0]]).cross(vertices[indices[2]]-vertices[indices[0]]).normalized()
                    out_faces.append(indices); out_uvs.append([table[k][1] for k in keys])
                    out_slots.append(slot); out_normals.append([normal]*3)
    used = sorted({i for face in out_faces for i in face})
    remap = {old: new for new, old in enumerate(used)}
    faces = [[remap[i] for i in face] for face in out_faces]
    return [vertices[i] for i in used], [bindings[i] for i in used], faces, out_uvs, out_slots, out_normals
