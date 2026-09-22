# HouseWall04 — QualityPass02

8 World1 placements; 88 → 872 triangles. Baseline `7b808473`. Offline validation PASS; independent artistic and technical review ACCEPTED offline. Client check pending.

## Improvement

Chamfered timber uprights, grain-aligned corbels, stepped foot collars and a lower rail articulate the existing stone bay.

## Contract

Current and candidate bind bounds: `[[-54.81, -50.9, 0.28], [246.23, 249.87, 264.48]]` → `[[-54.81, -50.9, 0.28], [246.23, 249.87, 264.48]]`. Original bounds are retained in `original/info.txt`. Mesh/material order, named bone and parent, one action with one frame and lock=0, filename, orientation and placements are preserved. Every baseline corner survives within 0.002 units. Roof HeroTile 4 object fade remains driven by the original model ID in `ZzzObject.cpp:3710`.

Every triangle corner matches the packed authored source to actual re-extracted BMD by material, bone, position and UV. Raw BMD normals all bind to the intended single root bone. One UV layer, packed diffuse images and original/reference exclusions pass. Full converter comparison is intentionally DIFFERENT; skeleton/actions compare EQUIVALENT.

## Frozen textures

- `src/bin/Data/Object1/tile_wood01.OZJ` — SHA-256 `51537840533e1c755569d3b57d9bea468353208c669daa2baeb2a421c41bf192`
- `src/bin/Data/Object1/tile_wood02.OZJ` — SHA-256 `1e829e6bc9af60739335ec85fc307d2c17806700e6d95f8845b7da78c4532eda`
- `src/bin/Data/Object1/tile_ston04.OZJ` — SHA-256 `a1a75b6fbf236d8747cb2e27555d871253cff7d2e48304226f0b68dbc83fa2f9`

Packed image dimensions: `[('tile_ston04.jpg', [512, 512]), ('tile_ston04.jpg.001', [512, 512]), ('tile_wood01.jpg', [128, 128]), ('tile_wood01.jpg.001', [128, 128]), ('tile_wood02.jpg', [512, 512]), ('tile_wood02.jpg.001', [512, 512])]`.

Original BMD is byte-identical to the retained first-pass original; merged baseline is byte-identical to current main. Original preview uses current unchanged diffuse paintings to isolate geometry. Historical original paintings remain under `assets-work/World1/Architecture02/`. No artwork was generated or repainted in this batch.

Export SHA-256: `2f593ea20c0ac5de7e8662608a8c9375bf6894d46cb7b22aa74d75d24b5084b5`.

## Evidence

See `review/comparison.jpg`, `comparison-reverse.jpg`, `comparison-small.png`, `wireframe.png`, original references, and batch actual-placement assemblies. These are offline diffuse-only Blender renders, never client screenshots. Original source.blend, merged baseline source.blend, packed editable candidate source.blend and all converter outputs are retained.

The user reported the merged Lorencia baseline working in game. This worker has not launched the client or installed shared runtime files. New candidate client acceptance is pending.
