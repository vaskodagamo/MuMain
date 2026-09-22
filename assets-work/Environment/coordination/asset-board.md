# Environment current remake board

Lorencia artistic gate: **incomplete**. Other-map production has not started.
Baseline `7b808473` is user-reported working in game. Current candidate client checks pending.
All 106 baseline render manifests and their 204 unique game-file dependencies were verified
against merged baseline by the independent reviewer. Source dependency-map triangle counts
are original counts; use current bmdconv info or final provenance for current geometry.

[Full current candidate records, identities, placements and exact dependencies](assessment.json).
All shared texture containers are frozen for the first two batches. All unowned BMDs,
terrain, placements, collision, engine, runtime, excluded content and historical evidence
are protected. Only ASTRA edits this board and shared handoffs.

| Batch | Owned game files (under src/bin/Data/Object1) | Owner / branch / worktree | Status | Purpose |
|---|---|---|---|---|
| QualityPass02/Architecture01 | HouseWall01.bmd, HouseWall04.bmd, HouseWall05.bmd, HouseWall06.bmd | architecture; codex/lorencia-q02-architecture; MuMain-q02-architecture | in progress | Improve modular profiles and shingle thickness; preserve edges and roof fade |
| QualityPass02/Masonry01 | HouseEtc01.bmd, StoneMuWall02.bmd, StoneMuWall03.bmd | furniture (redirected masonry artist); codex/lorencia-q02-furniture; MuMain-q02-furniture | in progress | Improve 129 placed masonry silhouettes and dressed profiles |

Furniture03/04/05 already have substantial rebuilds; reassessment places them behind these
architecture groups. No candidate is accepted yet. Commits/review evidence follow actual review.

## Baseline individual visual triage

[Independent 106-asset assessment](baseline-art-review.json): 73 defensible retain decisions,
24 production candidates, five companion consistency checks, four effect-preview checks.
These are baseline decisions; integrated coherence and new-export acceptance remain separate.
Retain entries stay in review until coordinator confirms their rationale.

Next independent groups: scrub Tree09/10 + Grass03/04; broad leaves Grass05/06;
large buildings House01/03/04; monuments StoneStatue01/03 + SteelStatue01;
Well01–04 pottery/rims; Waterspout01 dragon shape. Protected original Tree12/13 bindings
remain untouched. Resolve companion/effect reviews before final Lorencia gate.

Effect-preview follow-up: reviewer retains FireLight01/02 and DoungeonGate01 after
inspecting existing effect approximations and source renderer. Bonfire01 enters P2 queue:
rectangular logs need shaped split-log sections; mesh1 fire shell/UVs must remain exact.
Updated tally: 25 production, 76 defensible retain, five companion consistency checks.

## Accepted checkpoint: modular architecture

HouseWall01/04/05/06 accepted independently and integrated at `9abdf7e8` from worker
`594b6db2`. Structural timber bays and modeled shingle laps improve reduced-scale reading.
Counts 444/872/478/262. Exact bounds, original corners, roof boundary planes, material order,
rig/actions and all texture bytes preserved. Zero-area roof faces removed before final acceptance.
[Independent review](../../World1/QualityPass02/Architecture01/independent-review.json),
[assembly comparison](../../World1/QualityPass02/Architecture01/review/town-comparison.jpg),
[combined resolution/scope check](architecture-integration-validation.json).
All115 Object1 models resolve; exactly4 BMDs changed; no texture/protected-file changes.
New client checks pending. Lorencia gate remains incomplete:21 production candidates and
five companion consistency reviews remain beyond these four accepted replacements.

Next dispatched: Tree09/10 + Grass03/04, architecture artist in same worker branch/worktree,
owned new batch QualityPass02/Scrub01 only; all three alpha texture containers frozen.

## Masonry study deferred; baseline protected

No Masonry01 candidate accepted for integration. Motif relief50→762 (HouseEtc01) inflated
triangles without sufficient assembly/readability gain. Cheaper54/132/194 cap study improved
HouseEtc01 in isolation but introduced a V-shaped side notch in actual stacked records30/31;
strict modular connection-profile preservation takes precedence.02/03 cap changes still lack
sufficient visual gain. All three merged BMDs retained; all three remain artistically unresolved.
Worker will retain rejected source/evidence in its branch, then produce Grass05/06 broadleaf
arcs in QualityPass02/Broadleaf01. Exact tree_09.OZT remains frozen;399 placements.

2026-09-22: Scrub01 accepted offline and integrated4b0ccd02 from worker51679ab1. Tree09/10 continuous growth replaces pinched broom waists; Grass03/04 rounded connected crowns replace angular layered shells. Exactly8 game BMDs now differ from baseline; all115Object1 models resolve, frozen textures/protected paths unchanged. 530 scrub placements unchanged. Houses01 assigned to architecture (House01/03/04 only); Broadleaf01 final review pending. No new client verification or later-map work.

2026-09-22: Broadleaf01 integrated072f77f6 from worker0f78f88d. Grass05/06 long smooth arcs replace folded elbows across399placements. Independent full/reverse/reduced/light-dark and bothsix-placement assemblies accepted; technical/rawnormal/rig/UV/source-export checks passed. Combined10changedBMDs/115modelsresolved. Alltextures unchanged. AssignedWell01–04 to furniture underWells02; Houses01/03/04 active witharchitecture.15productioncandidates remain plus5companions/coherence review. No latermaps/clientclaims.

2026-09-23: Houses01 accepted/integrated7be0eaa3 (worker32a1a24b): House01 stone surrounds,House03 supported canopy/flue rims,House04 stepped dome courses. All13gamechanges passcombinedscope/exporthash/115modelresolution. House04original40frame motiontailbyteidentical; fullposed/source/worldnormalproof passes. AssignedStatues02 (StoneStatue01/03,SteelStatue01) toarchitecture; Wells02 finalreviewpending. No client/runtimechanges.

## 2026-09-23 wells checkpoint
Wells02 accepted and integrated `5a0ad5b5` from worker `c1296be9`: Well01–04 open rounded pottery mouths/throats and physical roof boards. Frozen textures, exact structural roof contacts, shared parts and UVs; independently reviewed all four actual-placement groups. Root confirmed BMD/source/image hashes. Combined validation passes 17 changed BMDs and 115 Object1 texture resolutions. New client checks pending.

Architecture owns StoneStatue01/03 and SteelStatue01 (Statues02). Furniture now owns Bonfire01 only (Bonfire02), opaque wood refinement with exact protected additive mesh and frozen textures. Reviewer sharpens remaining masonry direction. Nine production assets remain: four masonry (HouseEtc01, StoneMuWall02/03/04), three statues, Waterspout01 and Bonfire01. Retained-asset/coherence acceptance remains outstanding. No later maps started.

## 2026-09-23 bonfire and retention checkpoint
Bonfire01 accepted/integrated `38292593` from `79ed83a5`: quarter-split wood with curved bark faces and longitudinal grain;38 effect triangles and exact hot-end interface protected. Offline additive approximation/actual tilted placements accepted; client pending. Total18 new replacements. Independent individualized retention decision for76 baselines in retention-gate.json plus earlier4companions yields80 defensible retained assets; not80new remakes and not client verification. Root rechecked all217retention dependencyreferences/76imagehashes.

Coherence01 `ec35b6f4` contains5actual neighborhood regions,121instances,30matching images with complete source/placement/hash provenance; sampled material coherence accepted by reviewer with no-terrain/effect limits. Remaining8production:3statues atfinalreview,4masonry nowownedbyfurniture(Masonry02 firstHouseEtcfaceprototype),1fountain unclaimed. OverallLorencia incomplete; latermaps notstarted.

## 2026-09-23 statues checkpoint
Statues02 accepted/integrated `cc43c901` from `10d1e765`: StoneStatue01 carved niche around figure(212tris), StoneStatue03 layered curved feather wings and continuous robe/arm forms(1353), SteelStatue01 stepped capital and raised plaque(274). Root independently verified3BMD/source hashes and62reviewed evidence hashes. Full authored triangle/UV/bone/normal and exactcontact proofs pass; all frozen materials match integration. Corrected context omits engine-hidden PoseBox01 and frames entire pillar.

Total21 accepted new replacements +80 individually defensible retained assets; remaining5production: HouseEtc01/StoneMuWall02/03/04 owned furniture(Masonry02), Waterspout01 owned architecture(Fountain02). Fountain only actualdragon geometry; protect additional34 basin/floor triangles inside slot1 as well as entireother3meshes, all21motionframes/particles/waterinterface. Masonry onefaceprototype must earn gain before expansion; exact8unitborder surface and fullcoverage/nonoverlap proofs mandatory. Lorencia incomplete, latermaps notstarted, allnewclientchecks pending.
