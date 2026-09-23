"""Check disconnected component count and the edited closed bone surface."""
from collections import Counter
from pathlib import Path
import json
from audit_exports import triangles
ROOT = Path(__file__).resolve().parent


def components(rows):
    keys = [{tuple(round(x,5) for x in corner[1:4]) for corner in corners} for _,corners in rows]
    unused = set(range(len(rows)))
    result = []
    while unused:
        selected = {unused.pop()}
        points = set(keys[next(iter(selected))])
        while True:
            attached = {index for index in unused if keys[index] & points}
            if not attached:
                break
            selected |= attached
            unused -= attached
            for index in attached:
                points |= keys[index]
        result.append(sorted(selected))
    return result


def main():
    folder = ROOT/'Object48'
    old = triangles(folder/'validation/baseline/Object48.smd')
    new = triangles(folder/'validation/new/Object48.smd')
    before, after = components(old), components(new)
    assert len(before) == len(after) == 8
    edited = [new[i] for i in after[0]]
    edges = Counter()
    for _, corners in edited:
        points = [tuple(round(x,5) for x in v[1:4]) for v in corners]
        for a,b in zip(points,points[1:]+points[:1]):
            edges[(a,b)] += 1
    assert all(count == 1 and edges[(b,a)] == 1 for (a,b),count in edges.items())
    vertices = {p for edge in edges for p in edge}
    characteristic = len(vertices)-len(edges)//2+len(edited)
    assert characteristic == 2
    result = dict(status='PASS',baseline_components=[len(v) for v in before],
                  candidate_components=[len(v) for v in after],
                  edited_component_euler_characteristic=characteristic,
                  edited_component_closed_oriented_manifold=True)
    (folder/'validation/topology.json').write_text(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()
