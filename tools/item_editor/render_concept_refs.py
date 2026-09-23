"""Blender side of `concepts.py refs`: render reference images with the art study's preview code.

Runs inside Blender (background mode). It reuses assets-work/Items/study/render_previews.py
unchanged (same importer output, lights, orthographic camera direction, colour management) and
changes three things through its setup_render hook, for a cleaner reference:
- framing: the study's fixed 360-unit orthographic scale leaves a sword at a fifth of the frame,
  so the scale is set to the subject's bounding-box diagonal;
- centring: the study shifts parent and child objects alike, so skinned meshes end up off
  centre; the root objects are moved again so the visible meshes are centred on the target;
- background: the floor is hidden, leaving the study's plain world colour (no horizon, no shadow).

    blender -b --python tools/item_editor/render_concept_refs.py -- \
        --imports-dir <dir of .blend imports> --output-dir <refs dir> --jobs <jobs.json>

jobs.json: [{"name": "<item key>", "files": ["Item/Sword01.blend", ...]}, ...]; each job writes
<output-dir>/<name>.png and its render facts to <output-dir>/<name>.render.json.
"""

from pathlib import Path
import argparse
import json
import sys

STUDY_DIR = Path(__file__).resolve().parents[2] / 'assets-work' / 'Items' / 'study'
sys.path.insert(0, str(STUDY_DIR))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
import render_previews as study  # noqa: E402

FRAME_MARGIN = 1.1
FLOOR_NAME = 'Neutral preview floor'
FLOOR_GAP = 4.0


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--imports-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--jobs', type=Path, required=True)
    return parser.parse_args(sys.argv[sys.argv.index('--') + 1:])


def visible_meshes():
    return [obj for obj in bpy.context.scene.objects if obj.type == 'MESH' and not obj.get('mu_helper')]


def centre_on_target():
    """Move the root objects so the visible meshes' bounds are centred on the camera target."""
    bpy.context.view_layer.update()
    minimum, maximum = study.world_bounds(visible_meshes())
    offset = (minimum + maximum) * 0.5 - Vector(study.CAMERA_TARGET)
    for obj in bpy.context.scene.objects:
        if obj.parent is None:
            obj.location -= offset
    bpy.context.view_layer.update()
    return study.world_bounds(visible_meshes())


def framed_setup(study_setup):
    """The study's setup_render, centred and framed on the visible meshes, without the floor."""
    def setup(output_path, floor_z):
        minimum, maximum = centre_on_target()
        study.ORTHO_SCALE = (maximum - minimum).length * FRAME_MARGIN
        study_setup(output_path, minimum.z - FLOOR_GAP)
        bpy.data.objects[FLOOR_NAME].hide_render = True
    return setup


def main():
    args = parse_arguments()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    study.setup_render = framed_setup(study.setup_render)
    for job in json.loads(args.jobs.read_text(encoding='utf-8')):
        facts = study.render_preview(job, args.imports_dir, args.output_dir)
        facts['orthographic_scale_units'] = round(study.ORTHO_SCALE, 3)
        facts['camera_location_units'] = list(study.CAMERA_LOCATION)
        (args.output_dir / f'{job["name"]}.render.json').write_text(json.dumps(facts, indent=1) + '\n',
                                                                   encoding='utf-8')


if __name__ == '__main__':
    main()
