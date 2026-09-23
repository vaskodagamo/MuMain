"""Separate clean Blender processes for reference/topology, placed contacts and water."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
import pipeline
for name in ('final_evidence','context_review','effect_review','diffuse_diagnostic'):
 pipeline.blender(['--python',ROOT/(name+'.py')],ROOT/(name+'.txt'))
