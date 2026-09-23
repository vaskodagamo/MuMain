#!/usr/bin/env python3
"""Collect BMD structure and texture facts for the item art baseline study."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import re
import statistics
import subprocess
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT))

from tools import mu_texture

DATA_ROOT = REPO_ROOT / "src" / "bin" / "Data"
ITEM_ROOT = DATA_ROOT / "Item"
PLAYER_ROOT = DATA_ROOT / "Player"
OUTPUT_PATH = Path(__file__).with_name("baseline.json")

ITEM_MODEL_PATTERN = re.compile(
    r"^(?:"
    r"sword(?:[lr].*|_.*|\d+.*)?|axe\d+|mace(?:_.*|\d+.*)?|spear\d+|"
    r"staff(?:_.*|\d+.*)?|bow(?:_.*|\d+.*)?|crossbow\d+.*|"
    r"shield(?:_.*|\d+.*)?|wing\d+.*|darklordrobe\d*|"
    r"(?:alice1wing|elf_wing|angel_wing|devil_wing)|crosssheild|"
    r"archangelus|hd[k]_(?:sword.*|mace|bow|staff)|cw_(?:sword.*|mace|bow|staff)|"
    r"gamble(?:bow|_(?:bowx01|safter01|safterx01|scyder01|scyderx01|stick|stickx01|wand|wand01))|"
    r"sword36wing|arrows?0[12]|book\d+|book_of_(?:neil|rargle|sahamutt)"
    r")$",
    re.IGNORECASE,
)
ARMOUR_PART_PATTERN = re.compile(
    r"^(?P<part>helm|armor|pant|glove|boot)(?P<class>male|elfc?|class|monk|maletest)"
    r"(?P<number>\d+)(?P<variant>_(?:inventory|inven))?$|"
    r"^(?P<special>maskhelmmale|t_pantmale)(?P<special_number>\d+)$|"
    r"^(?P<new>new_(?:helm|armor|pant|glove|boot))(?P<new_number>\d+)$|"
    r"^(?P<high>hdk|cw)_(?P<high_part>helm|armor|pant|glove|boot)male(?P<high_number>\d+)$",
    re.IGNORECASE,
)

TEXTURE_CONTAINERS = {".jpg": ".ozj", ".tga": ".ozt"}
MODEL_HEADLINE = re.compile(
    r"meshes:\s*(\d+)\s+bones:\s*(\d+)\s+actions:\s*(\d+)\s+triangles:\s*(\d+)"
)
MESH_LINE = re.compile(
    r"mesh\s+(\d+):\s+triangles=(\d+)\s+vertices=(\d+)\s+normals=(\d+)\s+uvs=(\d+)\s+texture=(.+)$"
)
BOUNDS_LINE = re.compile(r"bounds \(bind pose\): min (.+?)  max (.+?)  size (.+)$")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", type=Path, default=DATA_ROOT)
    parser.add_argument("--bmdconv", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=OUTPUT_PATH)
    parser.add_argument("--jobs", type=int, default=8)
    return parser.parse_args()


def item_family(stem: str) -> str:
    lowered = stem.lower()
    if (
        lowered.startswith(("wing", "darklordrobe"))
        or lowered in {"alice1wing", "elf_wing", "angel_wing", "devil_wing"}
    ):
        return wing_generation(stem)
    if lowered.startswith("shield") or lowered == "crosssheild":
        return "shield"
    if lowered.startswith(("bow", "crossbow", "gamblebow", "gamble_bow", "hdk_bow", "cw_bow")):
        return "bow"
    if lowered.startswith(("staff", "archangelus", "gamble_wand", "gamble_stick", "hdk_staff", "cw_staff")):
        return "staff"
    if lowered.startswith(("axe",)):
        return "axe"
    if lowered.startswith(("mace", "gamble_safter", "gamble_scyder", "hdk_mace", "cw_mace")):
        return "mace"
    if lowered.startswith(("spear",)):
        return "spear"
    if lowered.startswith(("book",)):
        return "spellbook"
    if lowered.startswith(("arrow",)):
        return "bow_ammunition"
    return "sword"


def wing_generation(stem: str) -> str:
    match = re.fullmatch(r"wing(\d+)", stem, re.IGNORECASE)
    if not match:
        return "wing_other"
    number = int(match.group(1))
    if number <= 3:
        return "wing_gen1"
    if number <= 7:
        return "wing_gen2"
    if number <= 11:
        return "wing_gen3"
    return "wing_other"


def armour_identity(path: Path, player_root: Path) -> dict | None:
    match = ARMOUR_PART_PATTERN.fullmatch(path.stem)
    if not match:
        return None

    relative_parent = path.parent.relative_to(player_root).as_posix()
    if match.group("part"):
        part = match.group("part").lower()
        class_name = match.group("class").lower()
        number = int(match.group("number"))
        variant = match.group("variant")
    elif match.group("special"):
        part = "helm" if match.group("special").lower().startswith("maskhelm") else "pant"
        class_name = "male_special"
        number = int(match.group("special_number"))
        variant = None
    elif match.group("new"):
        part = match.group("new").split("_", 1)[1].lower()
        class_name = f"lucky_{relative_parent.rsplit('/', 1)[-1]}"
        number = int(match.group("new_number"))
        variant = None
    else:
        part = match.group("high_part").lower()
        class_name = match.group("high").lower()
        number = int(match.group("high_number"))
        variant = None

    return {
        "part": part,
        "class_or_line": class_name,
        "set_number": number,
        "variant": variant.removeprefix("_") if variant else None,
        "set_key": f"{relative_parent}/{class_name}_{number:02d}".strip("/"),
    }


def discover_models(data_root: Path) -> tuple[list[tuple[Path, str]], list[tuple[Path, dict]]]:
    item_root = data_root / "Item"
    player_root = data_root / "Player"
    items = [
        (path, item_family(path.stem))
        for path in sorted(item_root.rglob("*.bmd"), key=lambda value: value.as_posix().casefold())
        if ITEM_MODEL_PATTERN.fullmatch(path.stem)
    ]
    armours = []
    for path in sorted(player_root.rglob("*.bmd"), key=lambda value: value.as_posix().casefold()):
        identity = armour_identity(path, player_root)
        if identity:
            armours.append((path, identity))
    return items, armours


def query_model(path: Path, bmdconv: Path) -> dict:
    result = subprocess.run(
        [str(bmdconv), "info", str(path)], capture_output=True, text=True,
        encoding="utf-8", errors="replace", check=False
    )
    info = {
        "ok": result.returncode == 0,
        "model_name": None,
        "triangles": None,
        "meshes": None,
        "bones": None,
        "actions": None,
        "bounds": None,
        "mesh_details": [],
        "error": result.stderr.strip() or None,
    }
    for line in result.stdout.splitlines():
        if line.startswith("  name field:"):
            info["model_name"] = line.partition("name field:")[2].strip()
        headline = MODEL_HEADLINE.search(line)
        if headline:
            info["meshes"], info["bones"], info["actions"], info["triangles"] = map(
                int, headline.groups()
            )
        mesh = MESH_LINE.search(line)
        if mesh:
            index, triangles, vertices, normals, uvs = map(int, mesh.groups()[:5])
            texture_label = mesh.group(6).strip()
            flags = []
            flags_match = re.search(r"\s+flags:\s*(.*?)\s*$", texture_label)
            if flags_match:
                flags = [value.strip() for value in flags_match.group(1).split(",") if value.strip()]
                texture_label = texture_label[: flags_match.start()].strip()
            info["mesh_details"].append(
                {
                    "mesh_index": index,
                    "triangles": triangles,
                    "vertices": vertices,
                    "normals": normals,
                    "uvs": uvs,
                    "texture_name": texture_label,
                    "render_flags": flags,
                }
            )
        bounds = BOUNDS_LINE.search(line.strip())
        if bounds:
            info["bounds"] = {
                "min": parse_vector(bounds.group(1)),
                "max": parse_vector(bounds.group(2)),
                "size": parse_vector(bounds.group(3)),
            }
    if result.returncode != 0 and not info["error"]:
        info["error"] = result.stdout.strip() or f"bmdconv exited {result.returncode}"
    return info


def parse_vector(text: str) -> list[float]:
    return [float(component) for component in text.split()]


def texture_index(data_root: Path) -> dict[str, list[Path]]:
    index: dict[str, list[Path]] = defaultdict(list)
    for path in data_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in {".ozj", ".ozt", ".jpg", ".tga"}:
            index[path.name.casefold()].append(path)
    for paths in index.values():
        paths.sort(key=lambda value: value.as_posix().casefold())
    return index


def resolve_texture(
    material_name: str, model_path: Path, data_root: Path, image_index: dict[str, list[Path]]
) -> tuple[Path | None, list[Path]]:
    stem, extension = Path(material_name).stem, Path(material_name).suffix.lower()
    candidate_names = [material_name.casefold()]
    container_extension = TEXTURE_CONTAINERS.get(extension)
    if container_extension:
        candidate_names.insert(0, f"{stem}{container_extension}".casefold())

    candidates = []
    for candidate_name in candidate_names:
        candidates.extend(image_index.get(candidate_name, []))
    candidates = list(dict.fromkeys(candidates))
    if not candidates:
        return None, []

    local = [path for path in candidates if path.parent == model_path.parent]
    if len(local) == 1:
        return local[0], candidates
    if local:
        return sorted(local, key=lambda value: value.as_posix().casefold())[0], candidates
    preferred_root = [path for path in candidates if path.parent == data_root]
    if len(preferred_root) == 1:
        return preferred_root[0], candidates
    return candidates[0], candidates


def image_dimensions(path: Path) -> list[int] | None:
    data = path.read_bytes()
    extension = path.suffix.lower()
    if extension == ".ozj":
        width, height, _components = mu_texture.read_jpeg_dimensions(data[24:])
    elif extension == ".ozt":
        _kind, width, height, *_rest = mu_texture.parse_tga(data[4:])
    elif extension in {".jpg", ".jpeg"}:
        width, height, _components = mu_texture.read_jpeg_dimensions(data)
    elif extension == ".tga":
        _kind, width, height, *_rest = mu_texture.parse_tga(data)
    else:
        return None
    return [width, height]


def enrich_textures(
    model: dict, model_path: Path, data_root: Path, image_index: dict[str, list[Path]]
) -> None:
    for mesh in model["mesh_details"]:
        chosen, alternatives = resolve_texture(mesh["texture_name"], model_path, data_root, image_index)
        if chosen is None:
            mesh["texture"] = {"name": mesh["texture_name"], "path": None, "size_px": None, "missing": True}
            continue
        relative_path = chosen.relative_to(data_root).as_posix()
        error_message = None
        try:
            size = image_dimensions(chosen)
        except (OSError, ValueError, mu_texture.TextureError) as caught_error:
            size = None
            error_message = str(caught_error)
        mesh["texture"] = {
            "name": mesh["texture_name"],
            "path": relative_path,
            "size_px": size,
            "missing": False,
            "alternate_matches": [
                path.relative_to(data_root).as_posix() for path in alternatives if path != chosen
            ],
        }
        if size is None:
            mesh["texture"]["size_error"] = error_message or "unsupported image format"


def model_record(
    path: Path,
    label: str,
    info: dict,
    data_root: Path,
    image_index: dict[str, list[Path]],
    armour: dict | None = None,
) -> dict:
    enrich_textures(info, path, data_root, image_index)
    texture_records: dict[str, dict] = {}
    for mesh in info["mesh_details"]:
        texture = mesh["texture"]
        key = texture["path"] or texture["name"]
        texture_records.setdefault(key, texture.copy())
    record = {
        "path": path.relative_to(data_root).as_posix(),
        "family": label,
        "model_name_field": info["model_name"],
        "triangles": info["triangles"],
        "meshes": info["meshes"],
        "bones": info["bones"],
        "actions": info["actions"],
        "bounds_bind_pose": info["bounds"],
        "textures": list(texture_records.values()),
        "mesh_order": info["mesh_details"],
        "bmdconv_ok": info["ok"],
    }
    if armour is not None:
        record["armour_identity"] = armour
    if info["error"]:
        record["bmdconv_error"] = info["error"]
    return record


def index_shared_textures(records: list[dict]) -> list[dict]:
    usage: dict[str, set[str]] = defaultdict(set)
    for record in records:
        for texture in record["textures"]:
            if texture["path"]:
                usage[texture["path"]].add(record["path"])
    shared = []
    for texture_path, model_paths in sorted(usage.items()):
        if len(model_paths) < 2:
            continue
        example = next(
            texture
            for record in records
            if record["path"] in model_paths
            for texture in record["textures"]
            if texture["path"] == texture_path
        )
        shared.append(
            {
                "path": texture_path,
                "size_px": example["size_px"],
                "model_count": len(model_paths),
                "models": sorted(model_paths),
            }
        )
        for record in records:
            if record["path"] in model_paths:
                for texture in record["textures"]:
                    if texture["path"] == texture_path:
                        texture["shared_with"] = sorted(model_paths - {record["path"]})
    return shared


def family_summary(records: list[dict]) -> list[dict]:
    families: dict[str, list[dict]] = defaultdict(list)
    for record in records:
        families[record["family"]].append(record)
    return [
        {
            "family": name,
            "model_count": len(models),
            "triangle_range": [
                min(m["triangles"] or 0 for m in models),
                max(m["triangles"] or 0 for m in models),
            ],
            "median_triangles": statistics.median(m["triangles"] or 0 for m in models),
            "textures_max_side_le_128px": sum(
                1
                for model in models
                for texture in model["textures"]
                if texture["size_px"] and max(texture["size_px"]) <= 128
            ),
            "textures_with_short_side_lt_128px": sum(
                1
                for model in models
                for texture in model["textures"]
                if texture["size_px"] and min(texture["size_px"]) < 128
            ),
            "models_with_missing_textures": [m["path"] for m in models if any(t["missing"] for t in m["textures"])],
        }
        for name, models in sorted(families.items())
    ]


def main() -> None:
    args = parse_arguments()
    data_root = args.data_root.resolve()
    item_root, player_root = data_root / "Item", data_root / "Player"
    item_models, armour_models = discover_models(data_root)
    jobs = [(path, "item", family) for path, family in item_models]
    jobs.extend((path, "armour", identity) for path, identity in armour_models)

    print(f"Running bmdconv info for {len(jobs)} models with {args.jobs} workers")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        info_by_path = dict(executor.map(lambda job: (job[0], query_model(job[0], args.bmdconv)), jobs))

    images = texture_index(data_root)
    item_records = [
        model_record(path, family, info_by_path[path], data_root, images)
        for path, family in item_models
    ]
    armour_records = [
        model_record(path, identity["part"], info_by_path[path], data_root, images, identity)
        for path, identity in armour_models
    ]
    all_records = item_records + armour_records
    shared_textures = index_shared_textures(all_records)
    baseline = {
        "schema": "mu-items-art-baseline/1",
        "baseline_revision": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
        ).strip(),
        "generated_at_utc": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "source": "src/bin/Data/Item and armor-part models in src/bin/Data/Player",
        "scope": {
            "item_model_count": len(item_records),
            "armour_model_count": len(armour_records),
            "selection": (
                "Core weapon, shield, bow, staff, spellbook and wing filename families plus named "
                "special models from OpenItems; player armor component filename families, including "
                "class base parts, variants and nested LuckyItem set models."
            ),
            "item_data_folder": item_root.relative_to(REPO_ROOT).as_posix(),
            "player_data_folder": player_root.relative_to(REPO_ROOT).as_posix(),
            "source_mapping": "src/source/Engine/Object/ZzzOpenData.cpp:OpenItems and player part loaders",
        },
        "item_family_summary": family_summary(item_records),
        "armour_part_summary": family_summary(armour_records),
        "items": item_records,
        "armour_models": armour_records,
        "shared_textures": shared_textures,
        "quality_scores": [],
        "rework_candidates": [],
        "style_guide": {},
        "risks": [],
        "previews": {},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(baseline, indent=2) + "\n", encoding="utf-8")
    failures = [record["path"] for record in all_records if not record["bmdconv_ok"]]
    missing = [
        f"{record['path']}:{texture['name']}"
        for record in all_records
        for texture in record["textures"]
        if texture["missing"]
    ]
    print(
        f"Saved {args.output}: {len(item_records)} item models, {len(armour_records)} armor models, "
        f"{len(shared_textures)} shared texture files, {len(failures)} bmdconv failures, "
        f"{len(missing)} unresolved texture references"
    )


if __name__ == "__main__":
    main()
