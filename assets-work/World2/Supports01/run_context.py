"""Render contexts in a fresh Blender process to avoid current-file library loads."""
from pathlib import Path
from pipeline import blender

ROOT=Path(__file__).resolve().parent
blender(ROOT/'context-review.txt','--python',ROOT/'review.py','--','context')
