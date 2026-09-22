"""Finite UVs, triangle winding, exact frozen textures and current baseline checks."""
import hashlib
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[3]
NAMES = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')


def subtract(a, b):
    return [x-y for x, y in zip(a, b)]


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def anomaly_count(path):
    lines = path.read_text().split('triangles\n')[1].splitlines()[:-1]
    count = 0
    for offset in range(0, len(lines), 4):
        rows = [list(map(float, x.split())) for x in lines[offset+1:offset+4]]
        normal = cross(subtract(rows[1][1:4], rows[0][1:4]), subtract(rows[2][1:4], rows[0][1:4]))
        count += not all(sum(a*b for a,b in zip(normal,row[4:7])) > 0 for row in rows)
    return count


def inspect_mesh(folder):
    text = (folder / 'validation/new' / (folder.name+'.smd')).read_text()
    lines = text.split('triangles\n')[1].splitlines()[:-1]
    min_uv_area, min_area = float('inf'), float('inf')
    original = (folder / 'validation/original' / (folder.name+'.smd')).read_text().split('triangles\n')[1].splitlines()[:-1]
    inherited = []
    for offset in range(0, len(lines), 4):
        rows = [list(map(float, x.split())) for x in lines[offset+1:offset+4]]
        assert all(len(row)==9 and row[0]==0 and all(math.isfinite(v) for v in row) for row in rows)
        positions = [r[1:4] for r in rows]
        normal = cross(subtract(positions[1], positions[0]), subtract(positions[2], positions[0]))
        area = math.sqrt(sum(v*v for v in normal))/2
        min_area = min(min_area, area)
        uv = [r[7:9] for r in rows]
        u, v = subtract(uv[1], uv[0]), subtract(uv[2], uv[0])
        min_uv_area = min(min_uv_area, abs(u[0]*v[1]-u[1]*v[0])/2)
        if not all(sum(a*b for a,b in zip(normal,row[4:7])) > 0 for row in rows):
            assert lines[offset] == 'c_wall05.jpg' and folder.name == 'StoneMuWall02'
            matches = []
            for old_offset in range(0, len(original), 4):
                if original[old_offset] != lines[offset]:
                    continue
                old = [list(map(float, x.split())) for x in original[old_offset+1:old_offset+4]]
                position_uv_error = max(abs(row[k]-previous[k]) for row,previous in zip(rows,old) for k in (0,1,2,3,7,8))
                errors = [max(abs(a-b) for a,b in zip(row[4:7], previous[4:7])) for row,previous in zip(rows,old)]
                if position_uv_error < .000002 and max(errors) < .002:
                    matches.append((old_offset//4, max(errors)))
            assert len(matches) == 1, (offset, matches)
            inherited.append(dict(candidate_triangle=offset//4, original_triangle=matches[0][0],
                maximum_corner_component_error=matches[0][1], material='c_wall05.jpg'))
    assert min_uv_area > 1e-10 and min_area > 1e-8
    texture_hashes = {}
    for path in (folder/'exports').glob('*.OZJ'):
        game = REPOSITORY/'src/bin/Data/Object1'/path.name
        assert path.read_bytes()==game.read_bytes()
        texture_hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    result = dict(status='PASS', triangles=len(lines)//4, minimum_uv_area=min_uv_area,
        minimum_triangle_area=min_area, texture_hashes=texture_hashes,
        inherited_original_corner_normal_anomalies=inherited,
        normal_anomaly_incidence={stage: anomaly_count(folder / 'validation' / stage / (folder.name+'.smd')) for stage in ('original', 'current', 'new')},
        normals='All authored corners point with winding. Explicit unchanged original c_wall05 exceptions retain positions, UVs and custom normals within 0.002' , uv='One finite noncollapsed UV set')
    (folder/'validation/uv-winding-textures.json').write_text(json.dumps(result,indent=2))


if __name__ == '__main__':
    for name in NAMES:
        inspect_mesh(ROOT/name)
