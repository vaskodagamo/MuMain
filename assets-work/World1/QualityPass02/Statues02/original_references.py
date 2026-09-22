"""Reimport retained original models with original paintings, separately from merged baseline."""
from pathlib import Path
import pipeline
for name in pipeline.NAMES:
 folder=pipeline.ROOT/name/'original'
 pipeline.blender(['--python',pipeline.REPO/'tools/blender/mu_bmd_import.py','--','--bmd',folder/f'{name}.bmd','--data',folder,'--out',folder/'source.blend','--bmdconv',pipeline.CONVERTER],folder/'import.txt')
