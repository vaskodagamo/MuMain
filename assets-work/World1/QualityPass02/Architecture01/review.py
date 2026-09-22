"""Matching camera reviews of actual BMD reimports, including World1 assemblies."""
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode=True
import bpy
from mathutils import Euler,Matrix,Vector

ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')
sys.path.insert(0,str(REPO/'assets-work/World1/StaticBatch01'))
from review_scene import mesh_bounds,set_camera,set_lighting,render


def objects():
    return [o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference')]


def setup(bounds):
    set_camera(bounds,1.5)
    set_lighting()
    scene=bpy.context.scene
    scene.render.engine='CYCLES'
    scene.cycles.samples=12
    scene.render.film_transparent=False
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.055,.065,.075,1)
    scene.render.resolution_x=1000
    scene.render.resolution_y=850


def wire_material():
    material=bpy.data.materials.new('ReviewWire')
    material.use_nodes=True
    nodes,links=material.node_tree.nodes,material.node_tree.links
    wire=nodes.new('ShaderNodeWireframe'); wire.use_pixel_size=True
    wire.inputs['Size'].default_value=.8
    mix=nodes.new('ShaderNodeMixRGB')
    mix.inputs[1].default_value=(.35,.42,.48,1)
    mix.inputs[2].default_value=(.015,.02,.025,1)
    links.new(wire.outputs[0],mix.inputs[0])
    links.new(mix.outputs[0],nodes.get('Principled BSDF').inputs['Base Color'])
    return material


def audit_authored(name):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
    source=objects()[0]
    source.data.calc_loop_triangles()
    all_rows=[]
    for triangle in source.data.loop_triangles:
        rows=[]
        for loop in triangle.loops:
            vertex=source.data.vertices[source.data.loops[loop].vertex_index]
            assert len(vertex.groups)==1 and vertex.groups[0].weight==1
            p=source.matrix_world@vertex.co
            uv=source.data.uv_layers[0].data[loop].uv
            rows.append((tuple(p),tuple(uv)))
        all_rows.append((source.data.materials[triangle.material_index].name,rows))
    text=(folder/'validation/new'/f'{name}.smd').read_text().split('triangles\n')[1].splitlines()
    actual=[]
    for i in range(0,len(text)-1,4):
        rows=[list(map(float,line.split())) for line in text[i+1:i+4]]
        assert all(row[0]==0 for row in rows)
        actual.append((text[i],[(tuple(row[1:4]),tuple(row[7:9])) for row in rows]))
    maximum=0
    for texture,rows in all_rows:
        errors=[]
        for material,candidate in actual:
            if material!=texture: continue
            for offset in range(3):
                aligned=candidate[offset:]+candidate[:offset]
                if max(abs(a-b) for expected,got in zip(rows,aligned) for a,b in zip(expected[1],got[1]))>1e-5: continue
                errors.append(max(abs(a-b) for expected,got in zip(rows,aligned) for a,b in zip(expected[0],got[0])))
        assert errors and min(errors)<.002,(name,texture,min(errors) if errors else 'missing triangle')
        maximum=max(maximum,min(errors))
    assert len(all_rows)==len(actual)
    assert all(image.packed_file for image in bpy.data.images if image.source=='FILE')
    (folder/'validation/authored-match.json').write_text(json.dumps(dict(status='PASS',triangles=len(actual),every_corner_bone=0,position_tolerance=.002,uv_tolerance=1e-5,maximum_position_delta=maximum,packed_images=True),indent=2)+'\n')


def asset_review(name):
    folder=ROOT/name
    audit_authored(name)
    bounds=None
    for stage,path in [('original','original/source.blend'),('baseline','baseline/source.blend'),('candidate','validation/reimported.blend')]:
        bpy.ops.wm.open_mainfile(filepath=str(folder/path))
        bpy.context.scene.frame_set(0)
        if bounds is None: bounds=mesh_bounds(objects())
        setup(bounds)
        render(folder/'review'/f'{stage}.png')
        camera=bpy.context.scene.camera
        location=camera.location.copy()
        center=(Vector(bounds[0])+Vector(bounds[1]))/2
        camera.location=center+Vector((-1.35,2,1.3))*max(Vector(bounds[1])-Vector(bounds[0]))
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        render(folder/'review'/f'{stage}-reverse.png')
        camera.location=location
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        if stage=='candidate':
            bpy.context.view_layer.material_override=wire_material()
            render(folder/'review/wireframe.png')
            bpy.context.view_layer.material_override=None
        bpy.context.scene.render.resolution_percentage=30
        render(folder/'review'/f'{stage}-small.png')
    (folder/'review/context.json').write_text(json.dumps(dict(kind='OFFLINE BLENDER DIFFUSE; NOT CLIENT',bounds=bounds,camera='Identical orthographic camera per model',current='7b808473 merged BMD',candidate='Actual exported and reimported BMD',reduced_preview='300x255 pixels'),indent=2)+'\n')


def append_asset(name,stage,record,anchor):
    path=ROOT/name/('baseline/source.blend' if stage=='baseline' else 'validation/reimported.blend')
    with bpy.data.libraries.load(str(path)) as (data,loaded): loaded.objects=data.objects
    for obj in loaded.objects:
        if obj.type not in ('MESH','ARMATURE') or obj.get('mu_helper'): continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type=='ARMATURE':
            rotation=Euler([math.radians(v) for v in record['rotation']],'XYZ').to_matrix().to_4x4()
            obj.matrix_world=Matrix.Translation(Vector(record['position'])-anchor)@rotation@Matrix.Scale(record['scale'],4)


def assemblies():
    placements={name:json.loads((ROOT/name/'placements.json').read_text()) for name in NAMES}
    for label,index,radius in [('town',4,1600),('west',0,1100)]:
        anchor=Vector(placements['HouseWall05'][index]['position'])
        records=[dict(name=name,**record) for name in NAMES for record in placements[name] if math.dist(record['position'][:2],anchor[:2])<radius]
        bounds=None
        for stage in ('baseline','candidate'):
            bpy.ops.wm.read_factory_settings(use_empty=True)
            bpy.context.scene.world=bpy.data.worlds.new('AssemblyWorld')
            for record in records: append_asset(record['name'],stage,record,anchor)
            bpy.context.scene.frame_set(0); bpy.context.view_layer.update()
            if bounds is None:
                evaluated=[obj.evaluated_get(bpy.context.evaluated_depsgraph_get()) for obj in objects()]
                bounds=mesh_bounds(evaluated)
            setup(bounds)
            bpy.context.scene.render.resolution_x=1400
            bpy.context.scene.render.resolution_y=1000
            bpy.context.scene.render.resolution_percentage=100
            render(ROOT/'review'/f'{label}-{stage}.png')
        (ROOT/'review'/f'{label}-placements.json').write_text(json.dumps(dict(kind='OFFLINE ACTUAL WORLD1 TRANSFORMS; NOT CLIENT',anchor=list(anchor),placements=records,omitted='Terrain, other buildings, collision, baked lighting'),indent=2)+'\n')


if __name__=='__main__':
    (ROOT/'review').mkdir(exist_ok=True)
    if '--assemblies-only' not in sys.argv:
        for name in NAMES: asset_review(name)
    assemblies()
