"""Official export, exact motion packaging, complete final audits and image review."""
import os
import sys
from pathlib import Path
os.environ['HOUSE_NAMES']='House04'
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
import pipeline
pipeline.export('House04')
pipeline.subprocess.run([sys.executable,str(ROOT/'validate.py')],env=pipeline.ENV,check=True)
pipeline.blender(['--python',ROOT/'audit_source.py'],ROOT/'source-audit-dome.txt')
pipeline.blender(['--python',ROOT/'all_dome_audits.py'],ROOT/'dome-motion-audit.txt')
pipeline.blender(['--python',ROOT/'review.py'],ROOT/'review-dome.txt')
