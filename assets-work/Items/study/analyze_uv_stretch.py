"""Measure per-triangle UV anisotropy from bmdconv's SMD export."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import math
import statistics
import subprocess
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_BASELINE = Path(__file__).with_name("baseline.json")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--bmdconv", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=8)
    return parser.parse_args()


def read_triangles(path: Path) -> list[tuple[str, list[tuple[list[float], list[float]]]]]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    triangles = []
    in_triangles = False
    index = 0
    while index < len(lines):
        line = lines[index].strip()
        if line == "triangles":
            in_triangles = True
            index += 1
            continue
        if in_triangles and line == "end":
            break
        if not in_triangles or not line:
            index += 1
            continue

        material_name = line
        vertices = []
        for vertex_line in lines[index + 1 : index + 4]:
            columns = vertex_line.split()
            if len(columns) < 9:
                vertices = []
                break
            vertices.append(
                (
                    [float(value) for value in columns[1:4]],
                    [float(value) for value in columns[7:9]],
                )
            )
        if len(vertices) == 3:
            triangles.append((material_name, vertices))
            index += 4
        else:
            index += 1
    return triangles


def uv_stretch_ratio(
    vertices: list[tuple[list[float], list[float]]], texture_size: list[int]
) -> float | None:
    positions = [vertex[0] for vertex in vertices]
    uv = [
        [vertex[1][0] * texture_size[0], vertex[1][1] * texture_size[1]]
        for vertex in vertices
    ]
    edge1 = [positions[1][axis] - positions[0][axis] for axis in range(3)]
    edge2 = [positions[2][axis] - positions[0][axis] for axis in range(3)]
    length1 = math.sqrt(sum(value * value for value in edge1))
    if length1 < 1e-8:
        return None
    projection = sum(edge1[axis] * edge2[axis] for axis in range(3)) / length1
    height = math.sqrt(max(0.0, sum(value * value for value in edge2) - projection * projection))
    if height < 1e-8:
        return None

    du1 = uv[1][0] - uv[0][0]
    dv1 = uv[1][1] - uv[0][1]
    du2 = uv[2][0] - uv[0][0]
    dv2 = uv[2][1] - uv[0][1]
    m00 = du1 / length1
    m10 = dv1 / length1
    m01 = (du2 - m00 * projection) / height
    m11 = (dv2 - m10 * projection) / height

    gram00 = m00 * m00 + m10 * m10
    gram01 = m00 * m01 + m10 * m11
    gram11 = m01 * m01 + m11 * m11
    discriminant = math.sqrt(max(0.0, (gram00 - gram11) ** 2 + 4.0 * gram01**2))
    major = (gram00 + gram11 + discriminant) * 0.5
    minor = (gram00 + gram11 - discriminant) * 0.5
    if minor <= 1e-16:
        return None
    return math.sqrt(major / minor)


def percentile(values: list[float], portion: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * portion)]


def analyze_record(record: dict, data_root: Path, bmdconv: Path) -> tuple[str, dict]:
    model_path = data_root / record["path"]
    texture_sizes = {
        mesh["texture_name"].casefold(): mesh["texture"]["size_px"]
        for mesh in record["mesh_order"]
        if mesh["texture"]["size_px"]
    }
    with tempfile.TemporaryDirectory(prefix="mu_uv_baseline_") as temporary:
        result = subprocess.run(
            [str(bmdconv), "bmd2smd", str(model_path), temporary],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode != 0:
            return record["path"], {"error": result.stderr.strip() or result.stdout.strip()}
        smd_path = Path(temporary) / f"{model_path.stem}.smd"
        ratios = []
        skipped = 0
        analyzed = 0
        for material_name, vertices in read_triangles(smd_path):
            size = texture_sizes.get(material_name.casefold())
            if size is None:
                skipped += 1
                continue
            analyzed += 1
            ratio = uv_stretch_ratio(vertices, size)
            if ratio is None:
                skipped += 1
                continue
            ratios.append(ratio)

    return record["path"], {
        "triangles_with_texture_size": analyzed,
        "triangles_analyzed": len(ratios),
        "triangles_skipped": skipped,
        "anisotropy_ratio_p50": round(statistics.median(ratios), 3) if ratios else None,
        "anisotropy_ratio_p95": round(percentile(ratios, 0.95), 3) if ratios else None,
        "anisotropy_ratio_max": round(max(ratios), 3) if ratios else None,
        "triangles_ratio_over_8": sum(value > 8.0 for value in ratios),
        "triangles_ratio_over_16": sum(value > 16.0 for value in ratios),
    }


def main() -> None:
    args = parse_arguments()
    baseline_path = args.baseline.resolve()
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    data_root = REPO_ROOT / "src/bin/Data"
    records = baseline["items"] + baseline["armour_models"]
    print(f"Analyzing UV stretch in {len(records)} models")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = dict(
            executor.map(
                lambda record: analyze_record(record, data_root, args.bmdconv),
                records,
            )
        )
    for record in records:
        record["uv_stretch"] = results[record["path"]]
    baseline["uv_stretch_method"] = (
        "Per-triangle SMD tangent-space Jacobian singular-value ratio after scaling UVs to texture pixels; "
        "ratios describe anisotropy, not a standalone art-quality verdict."
    )
    baseline_path.write_text(json.dumps(baseline, indent=2) + "\n", encoding="utf-8")
    failures = sum("error" in value for value in results.values())
    severe = sum(
        value.get("triangles_ratio_over_8", 0) > 0
        for value in results.values()
    )
    print(f"Saved UV metrics; {failures} conversion failures, {severe} models contain at least one ratio > 8")


if __name__ == "__main__":
    main()
