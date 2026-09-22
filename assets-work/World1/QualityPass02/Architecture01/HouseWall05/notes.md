# HouseWall05 — QualityPass02

8 World1 placements; 54 → 478 triangles. Baseline `7b808473`. Offline validation PASS; independent artistic and technical review ACCEPTED offline. Client check pending.

## Improvement

Actual shingle-course laps follow the existing painted rows. Relief is recessed within the original roof envelope and vanishes along the modular outer perimeter.

## Contract

Current and candidate bind bounds: `[[-259.78, -259.68, -45.47], [200.3, 200.29, 44.57]]` → `[[-259.78, -259.68, -45.47], [200.3, 200.29, 44.57]]`. Original bounds are retained in `original/info.txt`. Mesh/material order, named bone and parent, one action with one frame and lock=0, filename, orientation and placements are preserved. Every baseline corner survives within 0.002 units. Roof HeroTile 4 object fade remains driven by the original model ID in `ZzzObject.cpp:3710`.

Every triangle corner matches the packed authored source to actual re-extracted BMD by material, bone, position and UV. Raw BMD normals all bind to the intended single root bone. One UV layer, packed diffuse images and original/reference exclusions pass. Full converter comparison is intentionally DIFFERENT; skeleton/actions compare EQUIVALENT.

## Frozen textures

- `src/bin/Data/Object1/tile_wood02.OZJ` — SHA-256 `1e829e6bc9af60739335ec85fc307d2c17806700e6d95f8845b7da78c4532eda`
- `src/bin/Data/Object1/tile_wood03.OZJ` — SHA-256 `080c832a287c14bc571e5f6bee02a3e1f5e668cd36ec2fe45622c682f12d245b`

Packed image dimensions: `[('tile_wood02.jpg', [512, 512]), ('tile_wood02.jpg.001', [512, 512]), ('tile_wood03.jpg', [512, 512]), ('tile_wood03.jpg.001', [512, 512])]`.

Original BMD is byte-identical to the retained first-pass original; merged baseline is byte-identical to current main. Original preview uses current unchanged diffuse paintings to isolate geometry. Historical original paintings remain under `assets-work/World1/Architecture02/`. No artwork was generated or repainted in this batch.

Export SHA-256: `2f2de7207c416cf8bf3ee6b4d67ea5bc77390dc9332bab4d692014b78f4a8b5d`.

## Evidence

See `review/comparison.jpg`, `comparison-reverse.jpg`, `comparison-small.png`, `wireframe.png`, original references, and batch actual-placement assemblies. These are offline diffuse-only Blender renders, never client screenshots. Original source.blend, merged baseline source.blend, packed editable candidate source.blend and all converter outputs are retained.

The user reported the merged Lorencia baseline working in game. This worker has not launched the client or installed shared runtime files. New candidate client acceptance is pending.
