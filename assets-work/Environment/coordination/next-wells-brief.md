# Next Wells batch: geometry refinement

Prepared against merged7b808473. Not dispatched; game paths remain unclaimed. No accepted replacement yet.

Coordinator inspected final Well02 and Wells01 Well03 comparison: pot bodies improved previously, but hexagonal open lips/handles remain visibly coarse; well rim reads as an undifferentiated tube. Preserve current painting initially; focus on coherent rounded vessel mouths/wall thickness, restrained well cap profile and roof board thickness inside extents. Do not resubdivide already smooth bodies without specific gain.

Own only Well01–04 BMDs and QualityPass02/Wells02 when dispatched. All shared textures frozen. Well01 is composite with barrels already refined in Cannons01, so preserve those accepted components. Start with Well03/04 lip study then propagate by matched part geometry into composite, not guessed filename substitution.

## Well01
SHA256 c753f37c049f5716deb7db64186083fed58920b925da133abea7303a851977a3
Actual placements: []
```
model /Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake/src/bin/Data/Object1/Well01.bmd
  name field: preserved-bind.smd  version: 12
  meshes: 4  bones: 7  actions: 1  triangles: 915
  bounds (bind pose): min -154.76 -125.85 -0.07  max 121.57 93.46 270.82  size 276.33 219.31 270.89
  mesh 0: triangles=72 vertices=48 normals=174 uvs=28 texture=tub.jpg
  mesh 1: triangles=16 vertices=24 normals=2 uvs=6 texture=horse_drawn_01.jpg
  mesh 2: triangles=123 vertices=89 normals=253 uvs=101 texture=well.jpg
  mesh 3: triangles=704 vertices=391 normals=425 uvs=378 texture=jar_01.jpg
  bone 0: "Cylinder01" parent=-1
  bone 1: "Cylinder04" parent=-1
  bone 2: "Cylinder07" parent=-1
  bone 3: "Cylinder08" parent=-1
  bone 4: "Cylinder09" parent=-1
  bone 5: "Cylinder10" parent=-1
  bone 6: "Cylinder11" parent=-1
  action 0: keys=1 lock=0
```

## Well02
SHA256 1abd72cb18be477740863d1677592eda32277e94f70154e999754933854adf22
Actual placements: [{"position": [14700.0, 11700.0, 164.99996948242188], "rotation": [0.0, 0.0, 390.0], "scale": 1.0, "tile": [147.0, 117.0]}]
```
model /Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake/src/bin/Data/Object1/Well02.bmd
  name field: preserved-bind.smd  version: 12
  meshes: 1  bones: 1  actions: 1  triangles: 123
  bounds (bind pose): min -79.28 -93.46 0.00  max 78.00 93.46 270.82  size 157.27 186.92 270.82
  mesh 0: triangles=123 vertices=89 normals=83 uvs=101 texture=well.jpg
  bone 0: "Cylinder04" parent=-1
  action 0: keys=1 lock=0
```

## Well03
SHA256 6a5d07341847640c760c1a1f578eba6608391d876ab7b67024433be62c063955
Actual placements: [{"position": [14600.0, 11650.0, 164.99998474121094], "rotation": [-10.0, 0.0, 390.0], "scale": 0.44000044465065, "tile": [146.0, 116.5]}, {"position": [21650.0, 7450.0, 267.1992492675781], "rotation": [0.0, 0.0, 60.0], "scale": 0.7000002861022949, "tile": [216.5, 74.5]}]
```
model /Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake/src/bin/Data/Object1/Well03.bmd
  name field: preserved-bind.smd  version: 12
  meshes: 1  bones: 4  actions: 1  triangles: 704
  bounds (bind pose): min -51.57 -48.73 -0.07  max 63.52 53.38 100.97  size 115.08 102.11 101.04
  mesh 0: triangles=704 vertices=391 normals=424 uvs=378 texture=jar_01.jpg
  bone 0: "Cylinder07" parent=-1
  bone 1: "Cylinder08" parent=-1
  bone 2: "Cylinder09" parent=-1
  bone 3: "Cylinder10" parent=-1
  action 0: keys=1 lock=0
```

## Well04
SHA256 3090273ccc5e2bd86d8e7478ce6e3a6bc5ff9e46d851694010a46e8865a16da0
Actual placements: [{"position": [12972.2490234375, 12080.9892578125, 177.22799682617188], "rotation": [0.0, 0.0, -720.0], "scale": 0.5400004386901855, "tile": [129.722490234375, 120.809892578125]}, {"position": [21200.0, 15150.0, 164.99996948242188], "rotation": [0.0, 0.0, 690.0], "scale": 0.5200004577636719, "tile": [212.0, 151.5]}]
```
model /Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake/src/bin/Data/Object1/Well04.bmd
  name field: preserved-bind.smd  version: 12
  meshes: 1  bones: 8  actions: 1  triangles: 1408
  bounds (bind pose): min -43.64 -134.58 -1.26  max 58.29 126.73 99.26  size 101.93 261.31 100.52
  mesh 0: triangles=1408 vertices=782 normals=844 uvs=378 texture=jar_01.jpg
  bone 0: "Cylinder07" parent=-1
  bone 1: "Cylinder08" parent=-1
  bone 2: "Cylinder09" parent=-1
  bone 3: "Cylinder10" parent=-1
  bone 4: "Cylinder11" parent=-1
  bone 5: "Cylinder12" parent=-1
  bone 6: "Cylinder13" parent=-1
  bone 7: "Cylinder14" parent=-1
  action 0: keys=1 lock=0
```

Frozen src/bin/Data/Object1/jar_01.OZJ: ["Well01", "Well03", "Well04"]
Frozen src/bin/Data/Object1/well.OZJ: ["Well01", "Well02"]
Frozen src/bin/Data/Object1/tub.OZJ: ["Carriage03", "Well01"]
Frozen src/bin/Data/Object1/horse_drawn_01.OZJ: ["Cannon01", "Cannon02", "Cannon03", "Carriage01", "Carriage02", "Carriage03", "Carriage04", "Hanging01", "HouseEtc02", "StoneMuWall04", "StoneWall03", "Well01"]

Required evidence: original/current packed blends and exact actions/rig, per-root extents and ground contacts, full raw normal and authored triangle bone/material/UV correspondence, matching diffuse full/reverse/reduced wireframes, original-reference images, actual Well02+Well03 placement assembly. Render only after authored/export audit. No runtime changes or new client claim.
