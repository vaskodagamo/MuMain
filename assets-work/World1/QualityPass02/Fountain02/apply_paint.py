"""Assign reviewed packaged dragon artwork only to the authored export material."""
from pathlib import Path
import sys,json,hashlib,os
sys.dont_write_bytecode=True
import bpy
ROOT=Path(__file__).resolve().parent;folder=ROOT/'Waterspout01'
paint=os.environ.get('FOUNTAIN_PAINT','paint02')
path=folder/'textures'/paint/'reagon_waterspout.jpg'
assert path.exists()
bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.get('mu_helper') and not o.get('mu_reference'))
old=obj.data.materials[1];assert old.name=='reagon_waterspout.jpg'
old.name='REF_BASELINE_reagon_waterspout.jpg'
material=old.copy();material.name='reagon_waterspout.jpg';obj.data.materials[1]=material
nodes=[n for n in material.node_tree.nodes if n.type=='TEX_IMAGE'];assert len(nodes)==1
image=bpy.data.images.load(str(path),check_existing=False);nodes[0].image=image
bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
(folder/'validation/paint-source.json').write_text(json.dumps(dict(status='ASSIGNED_NOT_ART_ACCEPTED',source=str(path.relative_to(ROOT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),slot=1,reference_materials='REF_BASELINE and REF_ORIGINAL retain their previous packed images'),indent=2)+'\n')
