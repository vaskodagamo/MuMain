"""Keep baseline split corner normals on every untouched House04 mesh."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from posed_source import actual_triangles,match


def apply(obj,folder):
    baseline=[(material,rows) for material,rows in actual_triangles(folder/'baseline/smd/House04.smd') if material!='tile_wood03.jpg']
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');order={name:i for i,name in enumerate(rig['mu_bone_order'])}
    normals=[Vector((0,0,0)) for n in obj.data.corner_normals]  # Zero retains automatic flat normals on authored roof faces.
    world_to_normal_local=obj.matrix_world.to_3x3().transposed()
    restored=0
    for face in obj.data.polygons:
        material=obj.data.materials[face.material_index].name
        if material=='tile_wood03.jpg':continue
        rows=[]
        for loop in face.loop_indices:
            vertex=obj.data.vertices[obj.data.loops[loop].vertex_index]
            bone=order[obj.vertex_groups[vertex.groups[0].group].name]
            rows.append([bone,*(obj.matrix_world@vertex.co),*obj.data.uv_layers[0].data[loop].uv])
        original=match(material,rows,baseline)
        for loop,row in zip(face.loop_indices,original):normals[loop]=(world_to_normal_local@Vector(row[4:7])).normalized()
        restored+=1
    assert not baseline
    for face in obj.data.polygons:face.use_smooth=True
    obj.data.normals_split_custom_set(normals)
    obj.data.update()
    print('Preserved authored baseline split normals for',restored,'nonroof triangles')


if __name__=='__main__':
    folder=ROOT/'House04';bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'))
    apply(obj,folder)
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
