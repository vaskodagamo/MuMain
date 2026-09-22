"""Compare current merged BMDs against official candidate reimports, identically lit."""
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_source import check
from audit_packed import check as check_packed
from audit_raw_normals import check as check_raw_normals

ROOT = Path(__file__).resolve().parent
WORLD = ROOT.parents[1]
sys.path.insert(0, str(WORLD / 'StaticBatch01'))
from review_scene import mesh_bounds, set_camera, set_lighting, render
spec = importlib.util.spec_from_file_location('audit', WORLD / 'coordination/audit_authored_vertices.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
NAMES = ('HouseEtc01',)


def open_scene(path, bounds):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(0)
    set_camera(bounds)
    set_lighting()
    bpy.context.scene.cycles.samples = 16


def extra_views(folder,stage,bounds):
    scene=bpy.context.scene;scene.render.resolution_percentage=100
    center=(Vector(bounds[0])+Vector(bounds[1]))/2;camera=scene.camera
    camera.location=center+Vector((0,-2,.28))*max(Vector(bounds[1])-Vector(bounds[0]))
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    render(folder/'review'/(stage+'-front.png'))
    scene.render.resolution_percentage=25;render(folder/'review'/(stage+'-front-small.png'))
    clay=bpy.data.materials.new('REVIEW_ONLY_CLAY');clay.use_nodes=True
    bsdf=clay.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Base Color'].default_value=(.44,.41,.36,1)
    bsdf.inputs['Roughness'].default_value=1;bsdf.inputs['Specular IOR Level'].default_value=0
    for obj in scene.objects:
        if obj.type=='MESH' and not obj.get('mu_reference') and not obj.get('mu_helper'):
            for i in range(len(obj.data.materials)):obj.data.materials[i]=clay
    scene.render.resolution_percentage=100;render(folder/'review'/(stage+'-clay-front.png'))
    set_camera(bounds);render(folder/'review'/(stage+'-clay.png'))


def review_asset(name):
    folder = ROOT / name
    check_packed(folder)
    check(folder)
    check_raw_normals(folder)
    authored = audit.points(folder / 'source.blend')
    exported = audit.points(folder / 'validation/reimported.blend')
    deviations = [audit.distances(authored, exported), audit.distances(exported, authored)]
    assert max(deviations) < .001, deviations
    (folder / 'validation/authored-export.json').write_text(json.dumps(dict(
        result='PASS', max_distances=deviations, space='bone-matched bind/world coordinates'), indent=2))
    bounds = json.loads((folder / 'validation/source.json').read_text())['bounds_before']
    stages = {'original': folder / 'original/source.blend', 'current': folder / 'baseline/source.blend',
              'candidate': folder / 'validation/reimported.blend'}
    for stage, path in stages.items():
        open_scene(path, bounds)
        render(folder / 'review' / (stage + '.png'))
        camera = bpy.context.scene.camera
        old = camera.location.copy()
        center = (Vector(bounds[0])+Vector(bounds[1]))/2
        camera.location = center + Vector((-1.35, 2, 1.3))*max(Vector(bounds[1])-Vector(bounds[0]))
        camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
        render(folder / 'review' / (stage + '-reverse.png'))
        camera.location = old
        camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.render.resolution_percentage = 25
        render(folder / 'review' / (stage + '-small.png'))
        extra_views(folder,stage,bounds)
    open_scene(folder/'validation/reimported.blend',bounds)
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or obj.get('mu_helper'):
            continue
        wire = obj.modifiers.new('ReviewWire', 'WIREFRAME')
        wire.thickness = .20
    bpy.context.scene.render.resolution_percentage = 100
    render(folder / 'review/wireframe.png')


if __name__ == '__main__':
    for name in (sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else NAMES):
        review_asset(name)
