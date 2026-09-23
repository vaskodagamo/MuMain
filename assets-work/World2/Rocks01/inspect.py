"""Read-only Blender topology and rig inspection of the frozen rock pair."""
from pathlib import Path
import json
import hashlib
import sys
import bpy

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
NAMES = ('Object49', 'Object50')


def inspect_mesh(obj):
    mesh = obj.data
    points = [obj.matrix_world @ vertex.co for vertex in mesh.vertices]
    bounds = [[fn(point[axis] for point in points) for axis in range(3)] for fn in (min, max)]
    edges = {}
    for face in mesh.polygons:
        for edge in face.edge_keys:
            edges[edge] = edges.get(edge, 0) + 1
    ground = [index for index, point in enumerate(points) if abs(point.z - bounds[0][2]) < .001]
    return dict(name=obj.name, vertices=len(mesh.vertices), triangles=sum(len(face.vertices)-2 for face in mesh.polygons),
                bounds=bounds, dimensions=[b-a for a,b in zip(*bounds)], ground_vertices=ground,
                boundary_edges=sum(value == 1 for value in edges.values()),
                nonmanifold_edges=sum(value > 2 for value in edges.values()),
                materials=[slot.material.name for slot in obj.material_slots], uv_layers=len(mesh.uv_layers),
                vertex_influence_counts=sorted(set(len(vertex.groups) for vertex in mesh.vertices)),
                groups=[group.name for group in obj.vertex_groups],
                transform=[list(row) for row in obj.matrix_world],
                packed_images=[image.name for image in bpy.data.images if image.packed_file])


def inspect_asset(name):
    source = ROOT / 'baseline' / name / 'baseline.blend'
    bpy.ops.wm.open_mainfile(filepath=str(source))
    objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH' and obj.name != 'smd_bone_vis']
    rigs = [obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE']
    return dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                meshes=[inspect_mesh(obj) for obj in objects],
                rigs=[dict(name=rig.name, bones=[(bone.name, bone.parent.name if bone.parent else None) for bone in rig.data.bones],
                           metadata={key:(rig[key].to_dict() if hasattr(rig[key], 'to_dict') else rig[key]) for key in rig.keys() if key.startswith('mu_')}) for rig in rigs])


report = {name:inspect_asset(name) for name in NAMES}
(ROOT / 'validation' / 'blender-inspection.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
