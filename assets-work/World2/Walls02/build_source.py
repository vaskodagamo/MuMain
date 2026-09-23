"""Dress three exposed boss edges; preserve every module interface face."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode = True
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
FOLDER = ROOT / 'Object51'
BEVEL_WIDTH = 7.0


def main():
    bpy.ops.wm.open_mainfile(filepath=str(FOLDER / 'baseline/source.blend'))
    obj = bpy.data.objects['Object51']
    original = obj.data.copy()
    reference = bpy.data.collections.new('REF_ORIGINAL')
    bpy.context.scene.collection.children.link(reference)
    ref = obj.copy()
    ref.data = original
    ref['mu_reference'] = True
    reference.objects.link(ref)
    reference.hide_render = True
    reference.hide_viewport = True
    original_normals = {
        tuple(tuple(round(c, 5) for c in obj.data.vertices[obj.data.loops[l].vertex_index].co)
              for l in poly.loop_indices):
        [tuple(obj.data.corner_normals[l].vector) for l in poly.loop_indices]
        for poly in obj.data.polygons}
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    mesh.edges.ensure_lookup_table()
    # The three front silhouette edges terminate at y=-108.8003. The lower
    # perimeter is intentionally open in the shipped model and remains open.
    front_edges = [edge for edge in mesh.edges
                   if all(abs(v.co.y + 108.8003) < .001 for v in edge.verts)
                   and abs(edge.verts[0].co.z-edge.verts[1].co.z) +
                   abs(edge.verts[0].co.x-edge.verts[1].co.x) > 1]
    selected = [edge for edge in front_edges if len(edge.link_faces) == 2
                and edge.link_faces[0].normal.dot(edge.link_faces[1].normal) < .99]
    assert len(selected) == 3, len(selected)
    bmesh.ops.bevel(mesh, geom=selected, offset=BEVEL_WIDTH, segments=1,
                    affect='EDGES', clamp_overlap=True)
    bmesh.ops.triangulate(mesh, faces=[face for face in mesh.faces if len(face.verts) > 3])
    mesh.normal_update()
    mesh.to_mesh(obj.data)
    mesh.free()
    obj.data.update()
    normals = []
    exact = 0
    for poly in obj.data.polygons:
        key = tuple(tuple(round(c,5) for c in obj.data.vertices[obj.data.loops[l].vertex_index].co)
                    for l in poly.loop_indices)
        old = original_normals.get(key)
        if old:
            exact += 1
        normals.extend(old or [tuple(poly.normal)] * len(poly.loop_indices))
        poly.use_smooth = True
    obj.data.normals_split_custom_set(normals)
    # BMesh interpolates deform weights and face-corner UVs; verify ownership.
    for vertex in obj.data.vertices:
        assert len(vertex.groups) == 1 and abs(vertex.groups[0].weight-1) < 1e-6
    assert len(obj.data.uv_layers) == 1
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(FOLDER / 'source.blend'))
    report = dict(triangles=len(obj.data.polygons), exact_original_triangles=exact,
                  bevel_width=BEVEL_WIDTH, design='Three broad dressed edges on the central projecting boss only')
    (FOLDER/'validation/source-build.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)


if __name__ == '__main__':
    main()
