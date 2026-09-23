"""Keep an editable, unchanged baseline with a hidden original reference."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
ROOT=Path(__file__).resolve().parent
for name in ('Object42','Object43'):
    folder=ROOT/name
    bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    collection=bpy.data.collections.new('REF_ORIGINAL')
    bpy.context.scene.collection.children.link(collection)
    for obj in list(bpy.context.scene.objects):
        if obj.type!='MESH' or obj.get('mu_helper'):continue
        ref=obj.copy()
        ref.data=obj.data.copy()
        ref['mu_reference']=True
        collection.objects.link(ref)
    collection.hide_render=True
    collection.hide_viewport=True
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
