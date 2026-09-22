"""Rebuild only roof caps after discarding zero-area terminal lap triangles."""
import json
from pathlib import Path
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from prepare import run_blender
import pipeline

ROOFS=('HouseWall05','HouseWall06')
expression=f"import sys; sys.path.insert(0, {str(ROOT)!r}); import build_source; [build_source.build(name) for name in {ROOFS!r}]"
run_blender(['--python-expr',expression],ROOT/'roof-build.log')
pipeline.NAMES=ROOFS
pipeline.export_assets()
pipeline.validate()
expression=f"import sys; sys.path.insert(0, {str(ROOT)!r}); import review; [review.asset_review(name) for name in {ROOFS!r}]; review.assemblies()"
run_blender(['--python-expr',expression],ROOT/'roof-review.log')
run_blender(['--python',str(ROOT/'audit_source.py')],ROOT/'source-audit.log')
