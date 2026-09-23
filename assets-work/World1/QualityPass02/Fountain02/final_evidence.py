"""Matching original reference and actual exported wireframe views."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from review import setup
from review_scene import render

import os
for name in os.environ.get('FOUNTAIN_NAMES','Waterspout01').split(','):
    folder=ROOT/name
    bounds=json.loads((folder/'validation/authored.json').read_text())['bounds_before']
    bpy.ops.wm.open_mainfile(filepath=str(folder/'original/source.blend'))
    setup(bounds)
    render(folder/'review/original.png')
    bpy.ops.wm.open_mainfile(filepath=str(folder/'validation/reimported.blend'))
    setup(bounds)
    for obj in list(bpy.context.scene.objects):
        if obj.type!='MESH' or obj.get('mu_helper') or obj.get('mu_reference'): continue
        mat=bpy.data.materials.new('TopologyEdges'); mat.use_nodes=True
        nodes=mat.node_tree.nodes; nodes.clear()
        output=nodes.new('ShaderNodeOutputMaterial')
        shader=nodes.new('ShaderNodeEmission'); shader.inputs['Strength'].default_value=.8
        wire=nodes.new('ShaderNodeWireframe'); wire.use_pixel_size=True; wire.inputs['Size'].default_value=.75
        mix=nodes.new('ShaderNodeMixRGB'); mix.inputs[1].default_value=(.15,.24,.21,1); mix.inputs[2].default_value=(.015,.02,.02,1)
        mat.node_tree.links.new(wire.outputs[0],mix.inputs[0]); mat.node_tree.links.new(mix.outputs[0],shader.inputs['Color']); mat.node_tree.links.new(shader.outputs[0],output.inputs[0])
        obj.data.materials.clear();obj.data.materials.append(mat)
        for polygon in obj.data.polygons: polygon.material_index=0
    render(folder/'review/wireframe.png')
