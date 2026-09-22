"""Authored masonry mouldings, preserving original perimeter and relief bindings."""
from pathlib import Path
import importlib.util
import json
import sys

sys.dont_write_bytecode = True
import bpy
sys.path.insert(0, str(Path(__file__).resolve().parent))
from relief import carved_geometry
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
LEGACY = ROOT.parents[1] / 'Masonry01'
spec = importlib.util.spec_from_file_location('masonry_source', LEGACY / 'build_source.py')
legacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(legacy)
NAMES = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')
# Face pairs are from the untouched original, not the first-pass triangulation.
PANELS = {'HouseEtc01': [(0, 1), (2, 3), (4, 5), (6, 7)],
          'StoneMuWall02': [(32, 33), (34, 35), (36, 37), (38, 39), (40, 41), (42, 43)],
          'StoneMuWall03': [(0, 1), (2, 3), (4, 5), (6, 7), (10, 11), (36, 37)]}
# Dressed perimeter, broad fillet, recessed bed: deliberately substantial at game scale.
MOULDING = ((0.035, 0.0), (0.06, 1.4), (0.085, 1.4), (0.12, 5.0))


def moulded_panel(original, pair, vertices, bindings):
    boundary, coordinates, slot = legacy.perimeter(original, pair)
    center = sum((vertices[i] for i in boundary), Vector()) / 4
    uv_center = sum(coordinates.values(), Vector((0, 0))) / 4
    rings = [boundary]
    uv_rings = [[coordinates[i] for i in boundary]]
    normal = sum((original.data.polygons[i].normal for i in pair), Vector()).normalized()
    profile = ((.055, .65),) if abs(normal.z) > .95 else MOULDING
    for fraction, depth in profile:
        indices, tex = [], []
        for index in boundary:
            indices.append(len(vertices))
            vertices.append(vertices[index].lerp(center, fraction) - normal * depth)
            bindings.append(bindings[index])
            tex.append(coordinates[index].lerp(uv_center, fraction))
        rings.append(indices)
        uv_rings.append(tex)
    faces, uvs = [], []
    for level in range(len(rings) - 1):
        for i in range(4):
            j = (i + 1) % 4
            q = [rings[level][i], rings[level][j], rings[level + 1][j], rings[level + 1][i]]
            uv = [uv_rings[level][i], uv_rings[level][j], uv_rings[level + 1][j], uv_rings[level + 1][i]]
            for order in ((0, 1, 2), (2, 3, 0)):
                faces.append([q[k] for k in order])
                uvs.append([uv[k] for k in order])
    for order in ((0, 1, 2), (2, 3, 0)):
        faces.append([rings[-1][k] for k in order])
        uvs.append([uv_rings[-1][k] for k in order])
    return (faces, uvs, slot), dict(original_face_pair=pair, profile=MOULDING,
        original_perimeter=[list(original.matrix_world @ vertices[i]) for i in boundary])


def build(name):
    root = ROOT / name
    bpy.ops.wm.open_mainfile(filepath=str(root / 'original/source.blend'))
    bpy.context.scene.frame_set(0)
    rig, reference = legacy.preserve_original()
    original = next(iter(reference.objects))
    bounds = legacy.mesh_bounds([original])
    legacy.PAIRS = PANELS
    legacy.make_recess = moulded_panel
    geometry, evidence = legacy.assemble_geometry(original, name)
    geometry = carved_geometry(geometry, original)
    obj = legacy.create_export(root, original, rig, geometry)
    legacy.retain_high_poly(obj)
    legacy.validate_source(root, obj, rig, original, bounds, evidence)
    report_path = root / 'validation/blender.json'
    report = json.loads(report_path.read_text())
    report.pop('original_vertices_retained', None)
    report['protected'] = 'Unmodified module perimeter and stone connection corners. Internal face relief deliberately reshaped; no all-original-vertices claim.'
    report_path.write_text(json.dumps(report, indent=2))
    legacy.set_camera(bounds)
    legacy.set_lighting()
    for image in bpy.data.images:
        if image.source == 'FILE':
            image.pack()
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(root / 'source.blend'))
    print(name, len(obj.data.polygons), 'triangles')


if __name__ == '__main__':
    for name in NAMES:
        build(name)
