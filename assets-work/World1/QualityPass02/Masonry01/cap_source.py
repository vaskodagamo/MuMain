"""Broad dressed-stone cap chamfers on measured exposed upper masonry edges."""
from pathlib import Path
import importlib.util
import json
import math
import sys

sys.dont_write_bytecode = True
import bpy
import bmesh
from mathutils import Vector
from mathutils.geometry import barycentric_transform

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('legacy', ROOT.parents[1] / 'Masonry01/build_source.py')
legacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(legacy)
NAMES = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')
CHAMFER_WIDTH = 5.0


def original_geometry(original):
    vertices = [v.co.copy() for v in original.data.vertices]
    bindings = [[(original.vertex_groups[g.group].name,g.weight) for g in v.groups] for v in original.data.vertices]
    faces = [list(p.vertices) for p in original.data.polygons]
    uvs = [[original.data.uv_layers[0].data[i].uv.copy() for i in p.loop_indices] for p in original.data.polygons]
    normals = [[original.data.corner_normals[i].vector.copy() for i in p.loop_indices] for p in original.data.polygons]
    return vertices, bindings, faces, uvs, [p.material_index for p in original.data.polygons], normals


def chamfer(obj, name):
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    bmesh.ops.remove_doubles(mesh, verts=list(mesh.verts), dist=.00001)
    mesh.normal_update()
    points = [v.co for v in mesh.verts]
    low, high = min(p.z for p in points), max(p.z for p in points)
    threshold = 70 if name == 'HouseEtc01' else 180
    if name != 'HouseEtc01':
        bmesh.ops.bisect_plane(mesh, geom=list(mesh.verts)+list(mesh.edges)+list(mesh.faces),
            dist=.00001, plane_co=(0,0,threshold), plane_no=(0,0,1), clear_inner=False, clear_outer=False)
        mesh.normal_update()
    edges = []
    for edge in mesh.edges:
        if len(edge.link_faces) != 2 or min(v.co.z for v in edge.verts) < threshold:
            continue
        materials = [obj.data.materials[f.material_index].name for f in edge.link_faces]
        if any(not material.endswith('c_wall04.jpg') for material in materials):
            continue
        if edge.calc_face_angle() < math.radians(25):
            continue
        if name != 'HouseEtc01' and any(v.co.z > high-1 for v in edge.verts):
            continue
        edges.append(edge)
    assert edges, name
    evidence = [[list(v.co) for v in e.verts] for e in edges]
    bmesh.ops.bevel(mesh, geom=edges, offset=CHAMFER_WIDTH, segments=1,
        affect='EDGES', clamp_overlap=True)
    bmesh.ops.triangulate(mesh, faces=list(mesh.faces), quad_method='FIXED', ngon_method='EAR_CLIP')
    collapsed = [face for face in mesh.faces if face.calc_area() <= .000001]
    print(name, 'zero-area bevel cleanup', len(collapsed))
    if collapsed:
        bmesh.ops.delete(mesh, geom=collapsed, context='FACES_ONLY')
    loose = [vertex for vertex in mesh.verts if not vertex.link_faces]
    if loose:
        bmesh.ops.delete(mesh, geom=loose, context='VERTS')
    mesh.faces.sort(key=lambda face: face.material_index)
    mesh.faces.index_update()
    mesh.to_mesh(obj.data)
    mesh.free()
    obj.data.update()
    obj.vertex_groups[0].add(list(range(len(obj.data.vertices))),1,'REPLACE')
    return evidence


def dress_chamfer_uvs(obj, original):
    originals = {}
    for face in original.data.polygons:
        key = tuple(sorted(tuple(round(c,5) for c in original.data.vertices[i].co) for i in face.vertices))
        originals[key] = face
    for face in obj.data.polygons:
        key = tuple(sorted(tuple(round(c,5) for c in obj.data.vertices[i].co) for i in face.vertices))
        if key in originals:
            continue
        candidates = [old for old in original.data.polygons if old.material_index == face.material_index]
        reference = min(candidates, key=lambda old: (1-face.normal.dot(old.normal))*1000 + (face.center-old.center).length)
        points = [original.data.vertices[i].co for i in reference.vertices]
        tex = [Vector((*original.data.uv_layers[0].data[i].uv,0)) for i in reference.loop_indices]
        for loop in face.loop_indices:
            vertex = obj.data.vertices[obj.data.loops[loop].vertex_index]
            uv = barycentric_transform(vertex.co,*points,*tex)
            obj.data.uv_layers[0].data[loop].uv = uv.xy


def restore_normals(obj, original):
    old_faces = {}
    for face in original.data.polygons:
        key = tuple(sorted(tuple(round(c,5) for c in original.data.vertices[i].co) for i in face.vertices))
        old_faces[key] = {tuple(round(c,5) for c in original.data.vertices[original.data.loops[i].vertex_index].co):
            original.data.corner_normals[i].vector.copy() for i in face.loop_indices}
    normals = []
    for face in obj.data.polygons:
        key = tuple(sorted(tuple(round(c,5) for c in obj.data.vertices[i].co) for i in face.vertices))
        previous = old_faces.get(key)
        for loop in face.loop_indices:
            point = obj.data.vertices[obj.data.loops[loop].vertex_index].co
            normals.append(previous[tuple(round(c,5) for c in point)] if previous else face.normal.copy())
        face.use_smooth = True
    obj.data.normals_split_custom_set(normals)


def build(name):
    folder = ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'original/source.blend'))
    rig, reference = legacy.preserve_original()
    original = next(iter(reference.objects))
    bounds = legacy.mesh_bounds([original])
    obj = legacy.create_export(folder, original, rig, original_geometry(original))
    edges = chamfer(obj,name)
    restore_normals(obj,original)
    dress_chamfer_uvs(obj,original)
    after = legacy.mesh_bounds([obj])
    assert max(abs(a-b) for aa,bb in zip(bounds,after) for a,b in zip(aa,bb)) < .001, (name,bounds,after)
    legacy.retain_high_poly(obj)
    legacy.validate_source(folder,obj,rig,original,bounds,edges)
    report_path = folder / 'validation/blender.json'
    report = json.loads(report_path.read_text())
    report.pop('original_vertices_retained',None)
    report['protected'] = 'Original bind bounds and structural body contacts preserved; selected upper c_wall04 edges dressed inward.'
    report_path.write_text(json.dumps(report,indent=2))
    legacy.set_camera(bounds)
    legacy.set_lighting()
    for image in bpy.data.images:
        if image.source=='FILE':image.pack()
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    print(name,len(obj.data.polygons),'triangles; exposed cap edges',len(edges))


if __name__=='__main__':
    for name in NAMES:build(name)
