# House01 / House03 / House04 production handoff
Read-only independent assessment. Geometry-only first; freeze every resolved texture container. Four actual placements total, but high scene coverage. No claim of client verification.
## Exact baseline and evidence
Start from CURRENT game BMDs or Architecture03 exports, which I byte-compared identical for all three. Architecture03/source.blend is useful editable reference; reimport current game bytes as REF_BASELINE. Do not mistake prior notes describing recesses for accepted geometry: Architecture03 reports208/313/358 and game bytes are unchanged exports. Existing final-inspection images have hashes matching every game dependency today.

### House01
- Game BMD SHA256 `498f0224e6c7f0f32d228b8a7ffbeb051e9796f7749ffb1e340149a632da737e`.
- Source `/Users/lukasmac/Documents/claude-test-mumain/MuMain/assets-work/World1/Architecture03/House01/source.blend`; final image `/Users/lukasmac/Documents/claude-test-mumain/MuMain/assets-work/World1/coordination/final-inspection/House01/final-offline.png`.
- Type 115; 208 triangles; bounds [[-222.34219360351562, -301.1827087402344, -0.618399977684021], [213.05020141601562, 302.2629089355469, 314.6015930175781]].
- Exact placements: `[{"position": [14300.0, 14700.0, 165.00001525878906], "rotation": [0.0, 0.0, 2070.0], "scale": 1.0, "tile": [143.0, 147.0]}, {"position": [15231.8671875, 14697.2021484375, 165.0000457763672], "rotation": [0.0, 0.0, 2070.0], "scale": 1.0, "tile": [152.318671875, 146.972021484375]}]`.

### House03
- Game BMD SHA256 `a8015a4a6155d5dc2cef379bebb6c054ec6a31c428329163a1a696424ea1b7f7`.
- Source `/Users/lukasmac/Documents/claude-test-mumain/MuMain/assets-work/World1/Architecture03/House03/source.blend`; final image `/Users/lukasmac/Documents/claude-test-mumain/MuMain/assets-work/World1/coordination/final-inspection/House03/final-offline.png`.
- Type 117; 313 triangles; bounds [[-330.6037902832031, -463.4543762207031, -1.374400019645691], [302.3160095214844, 450.86981201171875, 381.3833923339844]].
- Exact placements: `[{"position": [11750.0, 14500.0, 164.99996948242188], "rotation": [0.0, 0.0, 2070.0], "scale": 0.8600001335144043, "tile": [117.5, 145.0]}]`.

### House04
- Game BMD SHA256 `a1c4950b46c6a4c466a680d0be3d8ade35a4982b0665b1ce53b68e41e62219fa`.
- Source `/Users/lukasmac/Documents/claude-test-mumain/MuMain/assets-work/World1/Architecture03/House04/source.blend`; final image `/Users/lukasmac/Documents/claude-test-mumain/MuMain/assets-work/World1/coordination/final-inspection/House04/final-offline.png`.
- Type 118; 358 triangles; bounds [[-250.01089477539062, -240.25079345703125, -0.8140000104904175], [252.6663055419922, 199.92037963867188, 413.9349060058594]].
- Exact placements: `[{"position": [11550.0, 11300.0, 165.00003051757812], "rotation": [0.0, 0.0, 2160.0], "scale": 1.0, "tile": [115.5, 113.0]}]`.

## Visible problems and useful bounded intervention
- House01: broad parapet/roof rim and rafter ends remain coarse straight blocks; windows/door read as painted planes. Add readable inward window jamb/sill depth and structural rafter collars/corbels inside envelope; shape exposed chimney mouth/rim and inner parapet lip while freezing exterior mating profiles. Retain low flat-roof stone-house identity; no blanket generic Tudor framing. Paired structural interventions should read at300px, not only close-up microscopic grooves.
- House03: dominant awning is a nearly planar sheet of repeated boards with thin uniformly straight supports; chimney tops are faceted and flat. Highest payoff is purposeful bowed/thickened board courses and their underside support/bracket geometry within original awning footprint, with anchored original perimeter, post endpoints and house junction. Preserve torn/transparent awning outline rather than filling holes. Give static chimney mouths useful depth within current silhouette. Freeze mesh4 light cards.
- House04: most valuable isolated landmark. Dome has large angular shingle facets and flattened painted row transitions; barrel wall reads as broad cylindrical panels and ladder has thin straight rails. Candidate roof-course laps aligned to existing tile_wood03 painting can work like accepted HouseWall05/06, but preserve dome base ring, window cutouts, roof perimeter, ladder contacts and animated ornament clearance. Avoid smoothing through dormer openings. Improve static timber/stone panel framing/jamb depth within existing footprint. Do not rebuild animated blue-orb assembly or its rig as part of first static roof/wall pass.

## Rig, mesh and engine controls
House01:2 roots Box28/Bone01; one action,1key,lock0. Materials in exact order tile_ston05,tile_ston04,tile_house01,tile_ston06.
House03:7 independent roots Box08/Box29/Mesh01/Mesh02/Bone01/Bone02/Bone03; one action1key lock0. Mesh order tile_ston05,tile_ston04,tile_ston06,tile_ston07.tga,light_02.jpg. Mesh3 awning is OZT alpha; mesh4 is additive/light-controlled.
House04:9 bones: Mesh02(-1),Box02(-1),boness_01(-1),boness_02(2),Object02(2),boness_03(-1),boness_04(5),Object01(5),Object03(5).40 keys in action0 lock0; hierarchy and all sampled transforms must remain. Mesh order tile_02,tile_wood01,tile_house01,tile_ston04,tile_wood03,tile_wood02,tile_windows01,bridge_01,tile_space01. Mesh8 is scrolling blend material.
Engine refs: Core/Globals/_enum.h817 defines115/117/118. World/MapInfra/MapManager.cpp1078 loads numbered House assets; Object1 texture loading1097. Engine/Object/ZzzObject.cpp4617 sets House03 BlendMesh4;4620 sets House04 BlendMesh8.3810–3818 sets House04 V-scroll -(int)WorldTime%1000*.001 and House03 flicker BlendMeshLight .4–.7. Preserve exact material order, controlled-mesh UVs and geometry. House01 has no special case in these Lorencia controls.
Prior Architecture03 raw-binding audits show zero vertex/normal bone mismatch for all3. Fresh exports MUST repeat raw-normal and authored full triangle material/bone/position/UV checks—Tree10 experiment demonstrated exact global bounds and normal audit alone can miss cross-root vertex deduplication. Preserve static-vs-animated bone assignments.

## Contacts: strict protection, not only global bounds
Do not repeat rejected HouseEtc cap mistake: a plausible shallow seam notch is still an unauthorized connection-profile change. Mark actual joining edges before remodeling and freeze their coordinates, bone ownership, UVs where inherited.
House01 first placement: nearby Stair01 center[14431.868164,14461.727539,164.999985] and Sign01[14099.103516,14466.434570,330.000031]; rear walls within255–291units. Second placement has elevated Grass01 at[15315.206055,14623.691406,440] and Straw01[14913.196289,14675.004883,305], plus rear/side wall modules. Protect roof support surface, foundation/door/stair interface, sign mounting plane and nearby wall contacts.
House03 placement overlaps nearby House02 at[11750,14350,165] only150units away and another House02[11400,14300,165]; these are read-only neighbors, not scope expansion. Awning post-ground endpoints, attachment line and opening/approach clearance must be retained. Include nearby Light03 fixtures/effect anchors in contextual review.
House04 has Sign01[11647.581055,11376.637695,315.000031] only124units away. Keep sign mounting interface, footprint step/platform, doorway, ladder ground/top anchors and dome window/ornament openings. Motion review must include all40frames and swept envelope, not bind pose only.
These are candidate contact risks derived from exact placement proximity and images; NOT a completed geometric intersection proof. Production must render actual placements with readonly neighbors and record selected exact contact vertices/edge profiles before accepting changed geometry. Bounds alone insufficient.

## Full resolved Object1 texture consumers (World1 dependency-map scope)
Every path below is frozen for this first group; a later texture repaint requires explicit ownership/consumer expansion and all affected models reviewed. .OZJ versus .OZT spelling is deliberate.
- `src/bin/Data/Object1/bridge_01.OZJ` → Bridge01, House04, StoneMuWall01, StoneWall01, StoneWall02. SHA256 `0447ac5d335b4fd530ac58fb7fce35a3123055f7107d0813c749d36385c5b1bc`.
- `src/bin/Data/Object1/light_02.OZJ` → House03, HouseWall02. SHA256 `4a0211f098896fb68d11429da31482b70bc9d62b1130d50004412a2ddd3ca4e9`.
- `src/bin/Data/Object1/tile_02.OZJ` → FireLight02, House04, StoneMuWall01, StoneWall01, StoneWall02. SHA256 `428d88339fd1c04b6d153e33a1ed5a4a03d753130271e11fbf62f68564dfadcf`.
- `src/bin/Data/Object1/tile_house01.OZJ` → House01, House04, HouseEtc02, Tent01. SHA256 `c06449901b08eafba82b4d035972740cc3e454d730343065417644f3fa8a1a03`.
- `src/bin/Data/Object1/tile_space01.OZJ` → House04. SHA256 `3ec6d88e703a4ade939bbb96ae538d5ac71bb00ff5aeac0b3e9d11bcc26a6e82`.
- `src/bin/Data/Object1/tile_ston04.OZJ` → House01, House03, House04, HouseEtc02, HouseWall01, HouseWall02, HouseWall04. SHA256 `a1a75b6fbf236d8747cb2e27555d871253cff7d2e48304226f0b68dbc83fa2f9`.
- `src/bin/Data/Object1/tile_ston05.OZJ` → House01, House03. SHA256 `738fb32a62ece9f970cc822477361fb1105cbdde32dd4d475edda77a7d1fbc76`.
- `src/bin/Data/Object1/tile_ston06.OZJ` → BridgeStone01, House01, House03, HouseEtc02, HouseWall02, Tent01. SHA256 `c0eb1cde0172be5b4f1a1e7a19e274c6891d1e97d91479cdb11424a464847a3b`.
- `src/bin/Data/Object1/tile_ston07.OZT` → House03. SHA256 `af1fd6419d0c0dfa43228eea2c18cecb3b6d47502ecb0e7b2b70f4ecb8609093`.
- `src/bin/Data/Object1/tile_windows01.OZJ` → House04. SHA256 `ac20b7b25abeda1fb515b55c9d8d9e2bb0064f6bc1652f6862a7ac1466969367`.
- `src/bin/Data/Object1/tile_wood01.OZJ` → House04, House05, HouseWall01, HouseWall02, HouseWall04, Stair01. SHA256 `51537840533e1c755569d3b57d9bea468353208c669daa2baeb2a421c41bf192`.
- `src/bin/Data/Object1/tile_wood02.OZJ` → BridgeStone01, Fence01, House04, House05, HouseEtc02, HouseWall01, HouseWall02, HouseWall03, HouseWall04, HouseWall05, HouseWall06. SHA256 `1e829e6bc9af60739335ec85fc307d2c17806700e6d95f8845b7da78c4532eda`.
- `src/bin/Data/Object1/tile_wood03.OZJ` → House04, HouseEtc02, HouseWall05, HouseWall06. SHA256 `080c832a287c14bc571e5f6bee02a3e1f5e668cd36ec2fe45622c682f12d245b`.

## Acceptance and dispatch
Recommend House01+House03 static structural pass first; House04 can follow as bounded static roof pass with40-frame protection. If one3-asset batch, validate independently per model so a failed animated/export contract does not hold good static work.
Require current-v-actual exported/reimported candidate at matched camera/light; front/reverse/reduced and every actual placement with nearby readonly modules. House03 alpha view on both light/dark backgrounds and separate effect approximation. House04 frames0/10/20/30/39 minimum visual review plus all40 mathematical transform/posed bounds checks. Single finite UV, triangles, rigid bone weights, skeleton/manifest exact, material order exact, texture hashes exact. Source packed; converter validates model and actions with --animation. Strong visible structural gain at game scale required; a successful round trip or higher triangle count is not artistic acceptance.
Evidence JSON: `/tmp/astra-houses-evidence.json`.
