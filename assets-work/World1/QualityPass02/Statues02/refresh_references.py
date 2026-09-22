"""Refresh packed hidden original-painted reference only; authored geometry is untouched."""
import hashlib
import os
from pathlib import Path
import bpy
ROOT=Path(__file__).resolve().parent
for name in os.environ.get('STATUE_NAMES','StoneStatue01,StoneStatue03,SteelStatue01').split(','):
 folder=ROOT/name
 before=hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest()
 bpy.ops.wm.open_mainfile(filepath=str(folder/'source.blend'))
 collection=bpy.data.collections.get('REF_ORIGINAL')
 if collection:
  for obj in list(collection.objects):bpy.data.objects.remove(obj,do_unlink=True)
 else:
  collection=bpy.data.collections.new('REF_ORIGINAL');bpy.context.scene.collection.children.link(collection)
 collection.hide_viewport=collection.hide_render=True
 with bpy.data.libraries.load(str(folder/'original/source.blend')) as (source,loaded):loaded.objects=source.objects
 for obj in loaded.objects:
  if obj.type=='MESH' and not obj.get('mu_helper'):
   obj.parent=None;obj.modifiers.clear();obj['mu_reference']=True;collection.objects.link(obj)
 bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
 assert hashlib.sha256((folder/'exports'/f'{name}.bmd').read_bytes()).hexdigest()==before
