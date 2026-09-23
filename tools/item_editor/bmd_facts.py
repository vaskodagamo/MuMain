"""BMD structure facts through `bmdconv info` (tools/bmdconv), and texture container lookup.

bmdconv decrypts every BMD version the client reads; parsing its `info` report keeps this tool free
of a second BMD decoder. Only the lines below are used:

    name field: Data2\\Item\\Sword\\Sword02.smd  version: 10
    meshes: 1  bones: 1  actions: 1  triangles: 68
    bounds (bind pose): min -1.67 -64.73 -9.25  max 1.77 10.32 8.24  size 3.44 75.04 17.50
    mesh 0: triangles=68 vertices=42 normals=68 uvs=42 texture=sword02.jpg
    mesh 1: triangles=12 vertices=8 normals=8 uvs=8 texture=EPotion_R.jpg flags: bright
    action 0: keys=1 lock=0

The "flags:" suffix is bmdconv's reading of the name flags (_R bright, ...); texture names may
contain spaces ("head helmet Luck 40.jpg").
"""

from pathlib import Path
import re
import subprocess

VERSION = re.compile(r'version:\s*(\d+)')
COUNTS = re.compile(r'meshes:\s*(\d+)\s+bones:\s*(\d+)\s+actions:\s*(\d+)\s+triangles:\s*(\d+)')
BOUNDS = re.compile(r'bounds \(bind pose\): min (\S+) (\S+) (\S+)\s+max (\S+) (\S+) (\S+)')
MESH = re.compile(r'^\s*mesh (\d+): triangles=(\d+) .*?texture=(.*?)(?: flags:.*)?$')
ACTION = re.compile(r'^\s*action (\d+): keys=(\d+) lock=(\d+)')

# The client swaps the extension for the wrapped container (ZzzTexture.cpp, OpenJpegBuffer / OpenTga).
CONTAINER_EXTENSIONS = {'j': '.OZJ', 't': '.OZT'}
HIDDEN_PREFIX = 'hid'  # CLoadData::OpenTexture: names starting with "hid" become BITMAP_HIDE, no file


class BmdError(RuntimeError):
    pass


def parse_info(text):
    """bmdconv info report -> structure dict."""
    counts = COUNTS.search(text)
    if not counts:
        raise BmdError('bmdconv info: no "meshes: ... triangles:" line')
    meshes, bones, actions, triangles = (int(value) for value in counts.groups())
    version = VERSION.search(text)
    bounds = BOUNDS.search(text)
    mesh_textures, action_keys = [], []
    for line in text.splitlines():
        mesh = MESH.match(line)
        if mesh:
            mesh_textures.append(mesh.group(3).strip())
        action = ACTION.match(line)
        if action:
            action_keys.append(int(action.group(2)))
    return {
        'version': int(version.group(1)) if version else None,
        'meshes': meshes,
        'bones': bones,
        'actions': actions,
        'action_keys': action_keys,
        'triangles': triangles,
        'mesh_textures': mesh_textures,
        'bounds': ({'min': [float(v) for v in bounds.groups()[:3]], 'max': [float(v) for v in bounds.groups()[3:]]}
                   if bounds else None),
    }


def bmd_info(bmdconv, path):
    done = subprocess.run([str(bmdconv), 'info', str(path)], capture_output=True, text=True, errors='replace', check=False)
    if done.returncode != 0:
        raise BmdError(f'bmdconv info {path}: {done.stderr.strip() or done.stdout.strip()}')
    return parse_info(done.stdout)


def container_name(texture):
    """sword02.jpg -> sword02.OZJ; None for names the client does not load from a file."""
    stem, dot, extension = texture.rpartition('.')
    if not dot or texture.lower().startswith(HIDDEN_PREFIX):
        return None
    wrapped = CONTAINER_EXTENSIONS.get(extension[:1].lower())
    return stem + wrapped if wrapped else None


class FolderIndex:
    """Case-insensitive file lookup in repository folders, keeping the on-disk spelling."""

    def __init__(self, root):
        self.root = Path(root)
        self.cache = {}

    def names(self, folder):
        if folder not in self.cache:
            path = self.root / folder
            self.cache[folder] = {p.name.lower(): p.name for p in path.iterdir() if p.is_file()} if path.is_dir() else {}
        return self.cache[folder]

    def find(self, folder, name):
        found = self.names(folder).get(name.lower())
        return f'{folder}/{found}' if found else None


def resolve_texture(index, texture, folders):
    """Container path of a BMD texture name: the last folder of `folders` that has it wins, the way a
    later OpenTexture call replaces an earlier one. Returns (path or None, status)."""
    container = container_name(texture)
    if container is None:
        return None, 'hidden' if texture.lower().startswith(HIDDEN_PREFIX) else 'not-a-file'
    for folder in reversed(folders):
        found = index.find(folder, container)
        if found:
            return found, 'found'
    return None, 'missing'
