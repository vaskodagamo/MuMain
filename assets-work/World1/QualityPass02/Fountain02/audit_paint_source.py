"""Check authored packaged image against final export and immutable reference painting."""
from pathlib import Path
import bpy,json,hashlib,sys,os
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent;folder=ROOT/'Waterspout01'
bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
def image_for(obj):
 nodes=obj.data.materials[1].node_tree.nodes
 return next(node.image for node in nodes if node.type=='TEX_IMAGE')
obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'))
image=image_for(obj);assert image.packed_file
expected=(folder/'exports/reagon_waterspout.OZJ').read_bytes()[24:]
assert bytes(image.packed_file.data)==expected
baseline=next(o for o in bpy.data.collections['REF_BASELINE'].objects if o.type=='MESH')
reference=image_for(baseline);assert reference.packed_file
assert bytes(reference.packed_file.data)==(ROOT/'artwork/baseline-reagon_waterspout.OZJ').read_bytes()[24:]
report=dict(status='PASS',export_material=obj.data.materials[1].name,authored_packed_JPEG_sha256=hashlib.sha256(expected).hexdigest(),baseline_reference_painting='Exact original baseline JPEG payload retained',dimensions=list(image.size),method='Packed authored bytes equal actual final OZJ payload; reference packed bytes equal frozen baseline payload')
(folder/'validation/paint-source-equivalence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
