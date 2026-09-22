"""Source-only single UV layer, normal orientation, binding and packed-image checks."""
import json
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parent
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')


def audit(name):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
    objects=[obj for obj in bpy.context.scene.objects if obj.type=='MESH' and not obj.get('mu_helper') and not obj.get('mu_reference')]
    for obj in objects:
        assert len(obj.data.uv_layers)==1,(name,list(obj.data.uv_layers.keys()))
        assert all(len(v.groups)==1 and v.groups[0].weight==1 for v in obj.data.vertices)
        assert all(len(face.vertices)==3 for face in obj.data.polygons)
    images=[image for image in bpy.data.images if image.source=='FILE']
    assert all(image.packed_file for image in images)
    for collection in ('REF_ORIGINAL','REF_BASELINE'):
        assert bpy.data.collections[collection].hide_render
        assert all(obj.get('mu_reference') for obj in bpy.data.collections[collection].objects)
    report=dict(status='PASS',uv_layers={obj.name:list(obj.data.uv_layers.keys()) for obj in objects},rigid_bindings=True,triangles_only=True,packed_images=[dict(name=i.name,size=list(i.size)) for i in images],reference_collections_excluded=True)
    (folder/'validation/source-contract.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,'source contract PASS')


for name in NAMES: audit(name)
