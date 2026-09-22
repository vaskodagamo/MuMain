"""Match authored triangles to actual BMD reimport, including material, UV and binding."""
import json
from pathlib import Path

import bpy
from mathutils import kdtree

TOLERANCE = .0001


def triangles(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(0)
    result = []
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or obj.get('mu_helper') or obj.get('mu_reference'):
            continue
        if any(c.name.startswith('REF_') for c in obj.users_collection):
            continue
        for face in obj.data.polygons:
            material = obj.data.materials[face.material_index].get('mu_texture', obj.data.materials[face.material_index].name)
            corners = []
            for loop in face.loop_indices:
                vertex = obj.data.vertices[obj.data.loops[loop].vertex_index]
                bone = obj.vertex_groups[vertex.groups[0].group].name
                corners.append((obj.matrix_world @ vertex.co, obj.data.uv_layers[0].data[loop].uv.copy(), bone))
            result.append((material, corners))
    return result


def corner_error(a, b):
    if a[2] != b[2]:
        return float('inf')
    return max((a[0]-b[0]).length, (a[1]-b[1]).length)


def check(folder):
    authored = triangles(folder / 'source.blend')
    actual = triangles(folder / 'validation/reimported.blend')
    assert len(authored) == len(actual)
    maximum = 0
    unused = set(range(len(actual)))
    for material, corners in authored:
        best_error, best_index = float('inf'), None
        for i in unused:
            if actual[i][0] != material:
                continue
            candidate = actual[i][1]
            error = min(max(corner_error(corners[j], candidate[(j+shift)%3]) for j in range(3)) for shift in range(3))
            if error < best_error:
                best_error, best_index = error, i
            if error < TOLERANCE:
                break
        assert best_error < TOLERANCE, (folder.name, best_error)
        maximum = max(maximum, best_error)
        unused.remove(best_index)
    (folder / 'validation/authored-triangle-match.json').write_text(json.dumps(dict(
        status='PASS', triangles=len(authored), maximum_position_or_uv_error=maximum,
        criteria='Every triangle has same material, winding, bone ownership, positions and UV corners',
        tolerance=TOLERANCE), indent=2))
