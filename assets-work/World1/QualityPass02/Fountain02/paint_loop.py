"""Apply selected packaged art, official export/reimport, then matched direction images."""
from pathlib import Path
import sys,os,shutil
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
import pipeline
paint=sys.argv[1] if len(sys.argv)>1 else 'paint02'
assert paint in ('paint01','paint02')
pipeline.ENV['FOUNTAIN_PAINT']=paint
shutil.copy2(ROOT/'Waterspout01/textures'/paint/'reagon_waterspout.OZJ',ROOT/'Waterspout01/exports/reagon_waterspout.OZJ')
pipeline.blender(['--python',ROOT/'apply_paint.py'],ROOT/f'apply-{paint}.txt')
pipeline.export('Waterspout01')
pipeline.blender(['--python',ROOT/'review.py'],ROOT/f'review-{paint}.txt')
pipeline.blender(['--python',ROOT/'diffuse_diagnostic.py'],ROOT/f'diffuse-{paint}.txt')
