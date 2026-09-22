"""Separate engine-effect approximation and sampled motion comparison."""
import importlib.util
import json
import os
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from review import setup
from review_scene import render
spec=importlib.util.spec_from_file_location('old_effects',ROOT.parents[1]/'Architecture03/render_exports.py')
effects=importlib.util.module_from_spec(spec);spec.loader.exec_module(effects)

for name in os.environ.get('HOUSE_NAMES','House03,House04').split(','):
    if name=='House01':continue
    folder=ROOT/name
    bounds=json.loads((folder/'validation/authored.json').read_text())['bounds_before']
    for stage,path in [('baseline','baseline/source.blend'),('candidate','validation/reimported.blend')]:
        bpy.ops.wm.open_mainfile(filepath=str(folder/path));setup(bounds)
        effects.approximate_effects(name)
        if name=='House03':
            for strength in (.4,.7):
                for material in bpy.data.materials:
                    if material.name=='light_02.jpg':
                        for node in material.node_tree.nodes:
                            if node.type=='EMISSION':node.inputs['Strength'].default_value=strength
                render(folder/'review'/f'{stage}-effect-{strength}.png')
        else:
            for frame in (0,10,20,30,39):
                bpy.context.scene.frame_set(frame)
                bpy.data.materials['tile_space01.jpg'].node_tree.nodes['OFFLINE_V_SCROLL'].inputs[1].default_value[1]=-frame/40
                render(folder/'review'/f'{stage}-action-{frame}.png')
    (folder/'review/effect-context.json').write_text(json.dumps(dict(kind='OFFLINE APPROXIMATION ONLY',geometry='Actual exported/reimported candidate versus baseline',engine_mesh='House03 mesh4 and House04 mesh8 unchanged',flicker=[.4,.7] if name=='House03' else None,frames=[0,10,20,30,39] if name=='House04' else None,scroll='Illustrative V offsets coupled to sample index; actual engine uses WorldTime'),indent=2)+'\n')
