"""Actual unchanged World1 placement assemblies with baseline and candidate exports."""
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Euler, Matrix, Vector
import math

ROOT = Path(__file__).resolve().parent
OLD = ROOT.parents[1] / 'Masonry01'
spec = importlib.util.spec_from_file_location('joins', OLD / 'render_joins.py')
joins = importlib.util.module_from_spec(spec)
spec.loader.exec_module(joins)
OWNED = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')


def append_asset(record, anchor, stage):
    name = record['name']
    if name in OWNED:
        folder = ROOT / name
        path = folder / ('baseline/source.blend' if stage == 'before' else 'validation/reimported.blend')
    else:
        path = OLD / name / 'validation/reimported.blend'
    with bpy.data.libraries.load(str(path)) as (source, loaded):
        loaded.objects = source.objects
    meshes = []
    for obj in loaded.objects:
        if obj.type not in ('MESH', 'ARMATURE') or obj.get('mu_helper'):
            continue
        bpy.context.scene.collection.objects.link(obj)
        if obj.type == 'MESH':
            meshes.append(obj)
        else:
            rotation = Euler([math.radians(v) for v in record['rotation']], 'XYZ').to_matrix().to_4x4()
            obj.matrix_world = Matrix.Translation(Vector(record['position']) - anchor) @ rotation @ Matrix.Scale(record['scale'], 4)
    return meshes


if __name__ == '__main__':
    joins.append_asset = append_asset
    joins.REVIEW = ROOT / 'review-assemblies'
    joins.REVIEW.mkdir(exist_ok=True)
    joins.review_group('south-gate', 'StoneMuWall01', 1050, 1)
    joins.review_group('siege-wall', 'StoneMuWall04', 650)
