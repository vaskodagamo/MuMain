"""Actual motion with explicitly illustrative additive water and V scrolling."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from review import setup
from review_scene import render
folder=ROOT/'Waterspout01'
bounds=json.loads((folder/'validation/authored.json').read_text())['bounds_before']
for stage,path in [('baseline','baseline/source.blend'),('candidate','validation/reimported.blend')]:
 bpy.ops.wm.open_mainfile(filepath=str(folder/path));setup(bounds)
 material=bpy.data.materials['ston02.jpg'];nodes=material.node_tree.nodes;links=material.node_tree.links
 texture=next(n for n in nodes if n.type=='TEX_IMAGE')
 transparent=nodes.new('ShaderNodeBsdfTransparent');emission=nodes.new('ShaderNodeEmission');emission.inputs['Strength'].default_value=1
 add=nodes.new('ShaderNodeAddShader');links.new(texture.outputs['Color'],emission.inputs['Color']);links.new(transparent.outputs[0],add.inputs[0]);links.new(emission.outputs[0],add.inputs[1]);links.new(add.outputs[0],nodes.get('Material Output').inputs['Surface'])
 coordinates=nodes.new('ShaderNodeTexCoord');offset=nodes.new('ShaderNodeVectorMath');offset.operation='ADD';links.new(coordinates.outputs['UV'],offset.inputs[0]);links.new(offset.outputs[0],texture.inputs['Vector'])
 for frame,scroll in ((0,0),(10,-.5),(20,-.95)):
  bpy.context.scene.frame_set(frame);offset.inputs[1].default_value[1]=scroll
  render(folder/'review'/f'{stage}-water-action-{frame}.png')
(folder/'review/effect-context.json').write_text(json.dumps(dict(kind='OFFLINE APPROXIMATION, NOT CLIENT',frames=[0,10,20],water_material='ston02.jpg',mesh=3,additive_strength=1,V_offsets=[0,-.5,-.95],limitations='Illustrative UV offsets; engine scroll uses WorldTime independently from animation. Engine particles are not simulated; their actual bone anchors and offset envelopes are validated across21frames in attachments.json.'),indent=2)+'\n')
