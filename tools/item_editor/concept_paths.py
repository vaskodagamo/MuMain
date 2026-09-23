"""Where the concept tool's files are: every path resolves from the repository root.

The item editor may start concepts.py from inside Main.app (a build output inside the checkout),
with any working directory, so no path depends on the working directory. The root is the nearest
folder at or above this script that holds the item catalog and this tool, or --repo-root.
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL_DIR = Path('tools') / 'item_editor'
ITEMS_DIR = Path('assets-work') / 'Items'
CATALOG = ITEMS_DIR / 'catalog.json'
ROOT_MARKERS = (CATALOG, TOOL_DIR / 'concepts.py')


class PathError(RuntimeError):
    pass


def is_repo_root(path):
    return all((Path(path) / marker).is_file() for marker in ROOT_MARKERS)


def find_repo_root(start=HERE):
    start = Path(start).resolve()
    for candidate in (start,) + tuple(start.parents):
        if is_repo_root(candidate):
            return candidate
    raise PathError(f'no repository root at or above {start} (a folder with {CATALOG.as_posix()}); '
                    'pass --repo-root')


class RepoPaths:
    """The tool's inputs and outputs under one repository root (all absolute)."""

    def __init__(self, root):
        root = Path(root).expanduser().resolve()
        if not is_repo_root(root):
            raise PathError(f'--repo-root {root} is not a MuMain checkout (no {CATALOG.as_posix()})')
        self.root = root
        self.tool_dir = root / TOOL_DIR
        self.catalog = root / CATALOG
        self.baseline = root / ITEMS_DIR / 'study' / 'baseline.json'
        self.concepts_dir = root / ITEMS_DIR / 'concepts'
        self.out_dir = root / 'out' / 'item-concepts'
        self.prompt_template = self.tool_dir / 'concept_prompt.md'
        self.prices = self.tool_dir / 'image_prices.json'


def repo_paths(explicit_root=None):
    return RepoPaths(explicit_root if explicit_root else find_repo_root())
