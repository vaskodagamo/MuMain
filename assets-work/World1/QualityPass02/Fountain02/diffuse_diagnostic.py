"""Matched texture-only and neutral-ambient diagnostics, not client evidence."""
import bpy,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from review import setup
from review_scene import render
folder=ROOT/'Waterspout01';bounds=json.loads((folder/'validation/authored.json').read_text())['bounds_before']
for stage,path in [('baseline','baseline/source.blend'),('candidate','validation/reimported.blend')]:
 bpy.ops.wm.open_mainfile(filepath=str(folder/path));setup(bounds)
 bpy.context.scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.32,.36,.40,1)
 render(folder/'review'/f'{stage}-neutral-ambient.png')
 for material in bpy.data.materials:
  if not material.use_nodes:continue
  nodes=material.node_tree.nodes;links=material.node_tree.links;textures=[n for n in nodes if n.type=='TEX_IMAGE']
  if not textures:continue
  emission=nodes.new('ShaderNodeEmission');emission.inputs['Strength'].default_value=1;links.new(textures[0].outputs['Color'],emission.inputs['Color']);links.new(emission.outputs[0],nodes.get('Material Output').inputs['Surface'])
 render(folder/'review'/f'{stage}-diffuse-unlit.png')
 bpy.context.scene.render.resolution_percentage=30;render(folder/'review'/f'{stage}-diffuse-unlit-small.png')
(folder/'review/diffuse-diagnostic-context.json').write_text(json.dumps(dict(kind='OFFLINE DIAGNOSTIC NOT CLIENT',unlit='Direct diffuse texels, no geometric shading or lighting',neutral='Same standard sun, original review_scene neutral ambient .32/.36/.40 vs customary dark-background ambient .055/.065/.075',intent='Separate painted readability from shadow. Baseline/candidate settings exactly matched; cannot substitute for original shaded and placement evidence.'),indent=2)+'\n')
