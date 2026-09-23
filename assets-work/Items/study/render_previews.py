"""Render shared-camera Blender previews for the item art baseline study.

Run in Blender background mode after importing source BMDs with mu_bmd_import.py:

    blender -b --python render_previews.py -- --imports-dir /tmp/mu-item-study/imports \
        --output-dir assets-work/Items/study/previews
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

RENDER_SIZE = 1024
ORTHO_SCALE = 360.0
CAMERA_LOCATION = (260.0, -420.0, 210.0)
CAMERA_TARGET = (0.0, 0.0, 0.0)
WORLD_COLOR = (0.24, 0.27, 0.32, 1.0)

ITEM_PREVIEWS = [
    {"name": "sword", "files": ["Item/Sword01.blend"]},
    {"name": "axe", "files": ["Item/Axe01.blend"]},
    {"name": "mace", "files": ["Item/Mace01.blend"]},
    {"name": "spear", "files": ["Item/Spear01.blend"]},
    {"name": "bow", "files": ["Item/Bow01.blend"]},
    {"name": "staff", "files": ["Item/Staff01.blend"]},
    {"name": "shield", "files": ["Item/Shield01.blend"]},
    {"name": "wing-gen1", "files": ["Item/Wing01.blend"]},
    {"name": "wing-gen2", "files": ["Item/Wing04.blend"]},
    {"name": "wing-gen3", "files": ["Item/Wing08.blend"]},
]
ARMOUR_PREVIEWS = [
    {
        "name": "armour-set-male-01",
        "files": [
            f"Player/{part}Male01.blend"
            for part in ("Helm", "Armor", "Pant", "Glove", "Boot")
        ],
    },
    {
        "name": "armour-set-male-20",
        "files": [
            f"Player/{part}MaleTest20.blend"
            for part in ("Helm", "Armor", "Pant", "Glove", "Boot")
        ],
    },
    {
        "name": "armour-set-lucky-62",
        "files": [
            f"Player/LuckyItem/62/new_{part}01.blend"
            for part in ("Helm", "Armor", "Pant", "Glove", "Boot")
        ],
    },
]


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--imports-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])


def clear_scene() -> None:
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for collection_name in ("meshes", "armatures", "materials", "images", "actions", "cameras", "lights", "curves"):
        collection = getattr(bpy.data, collection_name)
        for block in list(collection):
            collection.remove(block)
    for scene in bpy.data.scenes:
        for block in list(scene.collection.children):
            scene.collection.children.unlink(block)


def append_model_blend(path: Path) -> int:
    if not path.is_file():
        raise FileNotFoundError(f"Missing imported Blender file: {path}")
    with bpy.data.libraries.load(str(path), link=False) as (source, target):
        target.objects = [name for name in source.objects if name != "smd_bone_vis"]
    scene = bpy.context.scene
    appended = [obj for obj in target.objects if obj is not None]
    for obj in appended:
        scene.collection.objects.link(obj)
        if obj.get("mu_helper"):
            obj.hide_render = True
    return sum(1 for obj in appended if obj.type == "MESH" and not obj.get("mu_helper"))


def world_bounds(objects: list) -> tuple[Vector, Vector]:
    corners = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    if not corners:
        raise RuntimeError("Imported model has no visible mesh bounds")
    minimum = Vector(tuple(min(point[axis] for point in corners) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in corners) for axis in range(3)))
    return minimum, maximum


def recenter_model(objects: list) -> tuple[Vector, Vector, Vector]:
    meshes = [obj for obj in objects if obj.type == "MESH" and not obj.get("mu_helper")]
    minimum, maximum = world_bounds(meshes)
    center = (minimum + maximum) * 0.5
    for obj in objects:
        obj.location -= center
    return minimum - center, maximum - center, center


def aim_at(obj, point: tuple[float, float, float]) -> None:
    direction = Vector(point) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def material(name: str, color: tuple[float, float, float, float], roughness: float):
    result = bpy.data.materials.new(name)
    result.diffuse_color = color
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = roughness
    return result


def add_floor(z: float) -> None:
    bpy.ops.mesh.primitive_plane_add(size=700.0, location=(0.0, 0.0, z))
    floor = bpy.context.object
    floor.name = "Neutral preview floor"
    floor.data.materials.append(material("Preview floor", (0.16, 0.18, 0.22, 1.0), 0.92))


def add_area_light(name: str, location: tuple[float, float, float], energy: float, size: float) -> None:
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    aim_at(obj, CAMERA_TARGET)


def setup_render(output_path: Path, floor_z: float) -> None:
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new("Shared item baseline camera")
    camera = bpy.data.objects.new("Shared item baseline camera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = RENDER_SIZE
    scene.render.resolution_y = RENDER_SIZE
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    scene.render.filepath = str(output_path)
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = 1.0
    scene.view_settings.gamma = 1.0
    scene.camera.data.type = "ORTHO"
    scene.camera.data.ortho_scale = ORTHO_SCALE
    scene.camera.location = CAMERA_LOCATION
    aim_at(scene.camera, CAMERA_TARGET)

    world = bpy.data.worlds.new("Neutral studio") if bpy.data.worlds.get("Neutral studio") is None else bpy.data.worlds["Neutral studio"]
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = WORLD_COLOR
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    scene.world = world

    add_floor(floor_z)
    add_area_light("Key", (180.0, -240.0, 280.0), 9000.0, 180.0)
    add_area_light("Fill", (-220.0, -100.0, 160.0), 6500.0, 210.0)
    add_area_light("Rim", (80.0, 240.0, 240.0), 7000.0, 140.0)


def render_preview(entry: dict, imports_dir: Path, output_dir: Path) -> dict:
    clear_scene()
    mesh_count = 0
    source_files = []
    for relative_file in entry["files"]:
        source = imports_dir / relative_file
        mesh_count += append_model_blend(source)
        source_files.append(relative_file)

    model_objects = list(bpy.context.scene.objects)
    minimum, maximum, recenter = recenter_model(model_objects)
    output_path = output_dir / f"{entry['name']}.png"
    setup_render(output_path, minimum.z - 4.0)
    bpy.ops.render.render(write_still=True)
    print(f"Rendered {output_path}: {mesh_count} mesh object(s), {len(source_files)} source file(s)")
    return {
        "name": entry["name"],
        "path": output_path.name,
        "source_blends": source_files,
        "mesh_object_count": mesh_count,
        "display_recenter_translation_units": [-round(value, 4) for value in recenter],
        "geometry_bounds_after_recenter": {
            "min": [round(value, 4) for value in minimum],
            "max": [round(value, 4) for value in maximum],
        },
    }


def main() -> None:
    args = parse_arguments()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = [
        render_preview(entry, args.imports_dir, args.output_dir)
        for entry in ITEM_PREVIEWS + ARMOUR_PREVIEWS
    ]
    metadata = {
        "camera": {
            "projection": "orthographic",
            "orthographic_scale_units": ORTHO_SCALE,
            "location_units": CAMERA_LOCATION,
            "target_units": CAMERA_TARGET,
            "render_px": [RENDER_SIZE, RENDER_SIZE],
        },
        "object_scale": 1.0,
        "display_recentered_per_preview": True,
        "lighting": "shared 3-area-light neutral studio with fixed positions and energies",
        "previews": results,
    }
    (args.output_dir / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
