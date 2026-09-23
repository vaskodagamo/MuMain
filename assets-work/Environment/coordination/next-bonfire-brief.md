# Bonfire01 bounded wood study (dispatched to furniture, 2026-09-23)

Current FireProps01 comparison shows six square-section logs with sharp long edges; updated bark is stronger but square cut silhouettes remain. Refine opaque fire_01 wood mesh only: intentional softened/chipped longitudinal profiles and believable end faces while preserving six-log layout, original lowest contacts and full effect-clearance envelope. Do not merely subdivide.

Protect mesh1 fire_02 geometry, UVs, raw bindings and complete skeleton/action unchanged; no flame/effect redesign, engine/placement/texture changes. Since the retained additive shell follows old rectangular logs, inspect both wood-only and paired effect approximation for exposed shell or hollow-looking gaps before accepting any shape change. A valid wood rebuild that makes the shell look worse is rejected.

BMD SHA256 a5a5ea2bc310df59f6bca0245cb7b427c42491473c328f8f584d9ce07aca9373
```
model /Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake/src/bin/Data/Object1/Bonfire01.bmd
  name field: Bonfire01.smd  version: 12
  meshes: 2  bones: 4  actions: 1  triangles: 110
  bounds (bind pose): min -57.61 -54.83 -6.95  max 62.04 55.02 55.95  size 119.65 109.85 62.90
  mesh 0: triangles=72 vertices=48 normals=48 uvs=60 texture=fire_01.jpg
  mesh 1: triangles=38 vertices=43 normals=43 uvs=43 texture=fire_02.jpg
  bone 0: "Bone01" parent=-1
  bone 1: "Bone_fire" parent=0
  bone 2: "Cylinder03" parent=0
  bone 3: "Cylinder04" parent=0
  action 0: keys=1 lock=0
```
Placements: [{"position": [3450.0, 5150.0, 177.40838623046875], "rotation": [-5.0, 0.0, 1110.0], "scale": 0.6000003814697266, "tile": [34.5, 51.5]}, {"position": [6333.28466796875, 12301.3984375, 165.00006103515625], "rotation": [0.0, 0.0, 3240.0], "scale": 0.7200002670288086, "tile": [63.3328466796875, 123.013984375]}, {"position": [7550.16162109375, 10031.35546875, 185.0000457763672], "rotation": [5.0, 0.0, 120.0], "scale": 0.7200002670288086, "tile": [75.5016162109375, 100.3135546875]}, {"position": [18022.939453125, 4156.69287109375, 164.99993896484375], "rotation": [-20.0, 0.0, -990.0], "scale": 1.0, "tile": [180.22939453125, 41.5669287109375]}, {"position": [18014.50390625, 4085.201904296875, 164.99998474121094], "rotation": [-20.0, 0.0, -990.0], "scale": 1.0, "tile": [180.1450390625, 40.85201904296875]}, {"position": [18400.0, 13450.0, 164.99998474121094], "rotation": [0.0, 0.0, 90.0], "scale": 1.0, "tile": [184.0, 134.5]}, {"position": [22150.0, 7750.0, 302.0836181640625], "rotation": [0.0, 0.0, 0.0], "scale": 1.0, "tile": [221.5, 77.5]}, {"position": [21153.810546875, 14653.3330078125, 146.1830291748047], "rotation": [0.0, 0.0, 1620.0], "scale": 1.0, "tile": [211.53810546875, 146.533330078125]}, {"position": [22650.0, 7750.0, 281.20513916015625], "rotation": [0.0, 0.0, 90.0], "scale": 1.0, "tile": [226.5, 77.5]}]

Frozen src/bin/Data/Object1/fire_01.OZJ SHA256 1b5f30547a9edd7877005a40c09946294c5772ac2b5953d3ffa92161c9a98294; consumers ["Bonfire01"].
Frozen src/bin/Data/Object1/fire_02.OZJ SHA256 e84366780b0047b3ad4232b114703835b4bff3728aa5ff9872cfd8fa170c9086; consumers ["Bonfire01"].

Sources/originals/current and effect-aware review script: assets-work/World1/FireProps01. Follow full official import/export, packed REF_ORIGINAL+baseline, exact actions/meshorder, UV/alpha/material checks, raw normal and every authored triangle matching proof. Match front/reverse/reduced views and actual placed scales. Geometry-only change stays within existing budget; source painting stays frozen. No new client verification claimed.
