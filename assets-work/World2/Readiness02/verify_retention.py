#!/usr/bin/env python3
"""Verify the read-only Object45–47 baseline retention evidence."""

from hashlib import sha256
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = Path(__file__).resolve().parent
REVIEW = json.loads((PACKAGE / "retention-review.json").read_text())


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def check(path, expected, label):
    path = Path(path)
    if not path.is_file():
        raise SystemExit(f"MISSING {label}: {path}")
    actual = digest(path)
    if actual != expected:
        raise SystemExit(f"HASH MISMATCH {label}: expected {expected}, got {actual}")
    print(f"OK {label}")


def main():
    manifest_path = ROOT / REVIEW["source_placement_manifest"]
    check(manifest_path, REVIEW["source_placement_manifest_sha256"], "Dungeon placement manifest")
    manifest = json.loads(manifest_path.read_text())

    total = 0
    for name, decision in REVIEW["assets"].items():
        source = manifest["models"][name]["placements"]
        total += len(source)
        if len(source) != decision["placement_count"]:
            raise SystemExit(f"PLACEMENT COUNT MISMATCH {name}: {len(source)}")

        provenance_path = PACKAGE / name / "provenance.json"
        provenance = json.loads(provenance_path.read_text())
        for relative, expected in provenance["dependencies"].items():
            check(ROOT / relative, expected, f"{name} dependency {relative}")
        for key in ("evidence", "supplemental_evidence"):
            for relative, expected in provenance.get(key, {}).items():
                check(PACKAGE / name / relative, expected, f"{name} evidence {relative}")

    if total != REVIEW["family"]["total_placements"]:
        raise SystemExit(f"FAMILY PLACEMENT COUNT MISMATCH: {total}")

    for reviewed in REVIEW["reviewed_context_records"]:
        name = reviewed["asset"]
        expected = {key: value for key, value in reviewed.items() if key != "asset"}
        actual = next(
            (placement for placement in manifest["models"][name]["placements"]
             if placement["index"] == expected["index"]),
            None,
        )
        if actual != expected:
            raise SystemExit(f"PLACEMENT RECORD MISMATCH: {name} index {expected['index']}")
    print(f"OK {len(REVIEW['reviewed_context_records'])} exact placed records; total={total}")

    for relative, expected in REVIEW["context_evidence"].items():
        check(ROOT / "assets-work/World2/Remains48/review-assemblies/remains-cluster" / relative,
              expected, f"Object47/Object48 context {relative}")
    check(ROOT / "src/bin/Data/Object2/Object48.bmd",
          REVIEW["context_object48_bmd_sha256"], "accepted Object48 context model")

    print("PASS Object45–47 baseline-retention evidence is internally consistent.")


if __name__ == "__main__":
    main()
