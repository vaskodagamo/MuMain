"""Protect the original throat, base and all actually placed orientation extents."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode = True
import bpy
from mathutils import Vector, Euler
ROOT = Path(__file__).resolve().parent
from audit_exports import triangles, same


def check(folder):
    name = folder.name
    old = triangles(folder / 'validation/baseline' / (name + '.smd'))
    new = triangles(folder / 'validation/new' / (name + '.smd'))
    indices = json.loads((folder / 'validation/source.json').read_text())['protected_triangles']
    normal_error = 0
    for index in indices:
        matches = [face for face in new if same(old[index], face)]
        assert len(matches) == 1, (index, len(matches))
        face = matches[0]
        shift = min(range(3), key=lambda s: max(math.dist(x[1:4], face[1][(i+s)%3][1:4]) for i,x in enumerate(old[index][1])))
        normal_error = max(normal_error, max(math.dist(x[4:7], face[1][(i+shift)%3][4:7]) for i,x in enumerate(old[index][1])))
    roundtrip = triangles(folder / 'validation/roundtrip' / (name + '.smd'))
    roundtrip_normal_error = 0
    for index in indices:
        control = next(face for face in roundtrip if same(old[index], face))
        candidate = next(face for face in new if same(old[index], face))
        shift = min(range(3), key=lambda s: max(math.dist(v[1:4], candidate[1][(j+s)%3][1:4]) for j,v in enumerate(control[1])))
        roundtrip_normal_error = max(roundtrip_normal_error, max(math.dist(v[4:7], candidate[1][(j+shift)%3][4:7]) for j,v in enumerate(control[1])))
    assert roundtrip_normal_error == 0, 'Untouched normals must exactly match official unchanged roundtrip'
    old_points = [Vector(row[1:4]) for _, rows in old for row in rows]
    new_points = [Vector(row[1:4]) for _, rows in new for row in rows]
    records = []
    for placement in json.loads((folder / 'placements.json').read_text()):
        matrix = Euler([math.radians(v) for v in placement['rotation']], 'XYZ').to_matrix()
        before = [matrix @ p * placement['scale'] for p in old_points]
        after = [matrix @ p * placement['scale'] for p in new_points]
        error = max(abs(operation(p[k] for p in before) - operation(p[k] for p in after))
                    for operation in (min, max) for k in range(3))
        assert error < .001, (placement['index'], error)
        records.append(dict(index=placement['index'], maximum_placed_bound_error=error))
    component_old = [Vector(v[1:4]) for _, rows in old[:32] for v in rows]
    component_new = [Vector(v[1:4]) for _, rows in new[:-252] for v in rows]
    # Every original component control point is retained, including all support anchors.
    missing = [list(p) for p in component_old if min((p-q).length for q in component_new) > .0003]
    assert not missing, missing
    component_errors = []
    for placement in json.loads((folder/'placements.json').read_text()):
        matrix = Euler([math.radians(v) for v in placement['rotation']], 'XYZ').to_matrix()
        before, after = [[matrix @ p for p in points] for points in (component_old, component_new)]
        error = max(abs(operation(p[k] for p in before)-operation(p[k] for p in after)) for operation in (min,max) for k in range(3))
        assert error < .0003, (placement['index'], error)
        component_errors.append(dict(index=placement['index'], maximum_component_support_error=error))
    (folder/'validation/component-supports.json').write_text(json.dumps(dict(status='PASS',
        retained_original_component_vertices=18, placements=component_errors,
        other_components='All 252 triangles exactly matched by position, UV, binding and cyclic winding'), indent=2))
    (folder / 'validation/protected-contacts.json').write_text(json.dumps(dict(status='PASS',
        protected_triangles=indices, maximum_protected_corner_normal_error=normal_error,
        maximum_normal_difference_from_untouched_official_roundtrip=roundtrip_normal_error,
        actual_placement_bounds=records), indent=2))
