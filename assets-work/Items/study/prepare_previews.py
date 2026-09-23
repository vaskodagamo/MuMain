"""Import the baseline preview BMDs through the repository Blender importer."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
IMPORT_SCRIPT = REPO_ROOT / "tools/blender/mu_bmd_import.py"
MODEL_PATHS = [
    "Item/Sword01.bmd",
    "Item/Axe01.bmd",
    "Item/Mace01.bmd",
    "Item/Spear01.bmd",
    "Item/Bow01.bmd",
    "Item/Staff01.bmd",
    "Item/Shield01.bmd",
    "Item/Wing01.bmd",
    "Item/Wing04.bmd",
    "Item/Wing08.bmd",
    *[
        f"Player/{part}Male01.bmd"
        for part in ("Helm", "Armor", "Pant", "Glove", "Boot")
    ],
    *[
        f"Player/{part}MaleTest20.bmd"
        for part in ("Helm", "Armor", "Pant", "Glove", "Boot")
    ],
    *[
        f"Player/LuckyItem/62/new_{part}01.bmd"
        for part in ("Helm", "Armor", "Pant", "Glove", "Boot")
    ],
]
DEFAULT_BLENDER = Path("/Applications/Blender.app/Contents/MacOS/Blender")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blender", type=Path, default=DEFAULT_BLENDER)
    parser.add_argument("--bmdconv", type=Path, required=True)
    parser.add_argument("--imports-dir", type=Path, required=True)
    return parser.parse_args()


def import_model(model_path: str, args: argparse.Namespace, data_root: Path) -> None:
    source = data_root / model_path
    if not source.is_file():
        raise FileNotFoundError(f"Representative model does not exist: {source}")
    output = args.imports_dir / Path(model_path).with_suffix(".blend")
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(args.blender),
        "-b",
        "--python",
        str(IMPORT_SCRIPT),
        "--",
        "--bmd",
        str(source),
        "--out",
        str(output),
        "--no-anims",
        "--data",
        str(source.parent),
        "--bmdconv",
        str(args.bmdconv),
    ]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        raise RuntimeError(
            f"Blender import failed for {model_path} (exit {result.returncode}):\n"
            f"{result.stdout}\n{result.stderr}"
        )
    print(f"Imported {model_path} -> {output}")


def main() -> None:
    args = parse_arguments()
    data_root = REPO_ROOT / "src/bin/Data"
    args.imports_dir.mkdir(parents=True, exist_ok=True)
    for model_path in MODEL_PATHS:
        import_model(model_path, args, data_root)
    print(f"Prepared {len(MODEL_PATHS)} Blender imports in {args.imports_dir}")


if __name__ == "__main__":
    main()
