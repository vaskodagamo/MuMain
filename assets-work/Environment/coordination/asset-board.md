# Current environment remake board — 2026-09-23

The overall goal remains in progress. PR #12 is merged with 22 accepted Lorencia replacements and 84 first-pass retention decisions. These records do not establish that Lorencia has reached consistent art quality; further assessment and production remain the top priority. The user reports that the prior Lorencia baseline worked in game. New changes have not been checked in the client.

PR #13 is merged with three independently accepted Dungeon replacements: Object28, Object29 and Object01. Object48 is a bounded one-component refinement with independent offline acceptance; this branch contains its integrated export and focused review evidence. Client verification is pending. The next production work returns to Lorencia before expanding Dungeon work.

See current-remake-assessment.json for the active goal status, dungeon-assessment.json for asset-level status, and progress.md for dated checkpoints.

## Accepted and proposed checkpoints

| Map | State | Count | Scope |
|---|---|---:|---|
| Lorencia | PR #12 merged; quality work remains open | 22 replacements, 84 first-pass retention decisions | World1 static environment |
| Dungeon | PR #13 merged | 3 replacements | Object28, Object29 and Object01 |
| Dungeon | Independently accepted; focused PR in progress | 1 replacement | Object48; bounds and texture contract preserved |

New client verification remains pending. The accepted exports and their source packages are indexed in accepted-exports.json. The prior coordination snapshot follows as historical record.

---

## Historical board snapshot before Object48 integration

# Dungeon environment production — current status, 2026-09-23

Lorencia remains separately accepted offline at d3ce9acf in draft PR12. The user reports that earlier Lorencia baseline working in game; newly changed assets have no client verification.

Dungeon checkpoint `c9a34462` is on `codex/dungeon-static-remake` in draft [PR #13](https://github.com/vaskodagamo/MuMain/pull/13). Three replacements are integrated: Object28 and Object29 rounded pottery with thick ceramic lips, and Object01’s broad dressed-stone crown chamfer. The wall change adds six triangles, keeps the dragon plaque and complete modular end faces, uses the original texture at even scale and preserves all 1,262 placed bounds. Independent art and technical review passed. The full texture dependency check resolves all 63 Object2 models, and the combined scope check reports exactly three game paths with exports byte-matching integration.

Object03’s distinct four-niche pillar, Object04’s carved pier, Object30’s broken vase and coffin Objects21/22 are retained after individual review. Five BMDs have a visual rationale; candidate studies are not counted as replacements. Object04’s complete source, rejected studies, full consumer analysis and reviewer report are in `assets-work/World2/Pier01`; its game BMD and both shared textures remain unchanged. Object03 and Object04 frozen texture hashes are in the retention reports. All new client checks remain pending.

Baseline previews and provenance now cover the next static Dungeon bone-remains family Objects45–48 (510 placements) plus an actual six-placement Object47/48 cluster. `bons.OZJ` remains frozen across nine model consumers. No owner or replacement is assigned until these baseline scenes receive a closer visual review. Other Dungeon models remain unassessed; effects, animals, markers and interactive content stay out of scope.

Worker branches `codex/dungeon-walls` (81562d69) and `codex/dungeon-pier` (4199d62f) are pushed to the verified `vaskodagamo/MuMain` fork; root integrated only the accepted Object01 BMD and pier evidence. Earlier pottery and coffin branches are also published. Primary checkout and shared runtime remain untouched.

Next: finish the Object45–48 baseline gate and assign the next independent static batches. See this coordination folder, `assets-work/World2/Readiness02/notes.md`, the accepted source manifest and the individual Wall/Pottery source trees. The overall environment goal is still in progress.


## Historical phase-opening record

# Environment production — Dungeon phase, 2026-09-23

Lorencia passed independent artistic/offline acceptance at d3ce9acf on codex/environment-remake-continuation, draft PR12 https://github.com/vaskodagamo/MuMain/pull/12. That separate checkpoint retains22 accepted replacements,84 justified retained assets and all sources. New client checks are pending. Its stable checkout is ../MuMain-environment-remake; this branch starts from main7b808473 to keep the Dungeon PR focused and does not supersede or revert PR12.

Current integration: MuMain-dungeon-remake, codex/dungeon-static-remake. Root alone edits shared coordination and installs/operates shared runtime; runtime and primary MuMain checkout remain untouched.

| Family | Owned BMDs under Data/Object2 | Owner branch / worktree | Deliverables | Status |
|---|---|---|---|---|
| Pottery | Object28, Object29, Object30 | furniture; codex/dungeon-pottery; MuMain-q02-furniture | assets-work/World2/Pottery01 | Object29 prototype in progress |
| Coffins | Object21, Object22 | architecture; codex/dungeon-coffins; MuMain-q02-architecture | assets-work/World2/Coffins01 | Object22 prototype in progress |

All textures are frozen initially. Pottery uses exact Object2/flower_vase.OZJ and deep_wall04.OZJ; the latter has14 consumers. Coffins use Object2/wood01.OZJ, shared by eight models including effect hybrids42/43. No overlapping writable files. Existing older Lorencia branches remain published; worker untracked caches were preserved when switching their owned worktrees.

Pottery objective: purposeful shoulder curvature and real lip thickness, with exact original contacts and eight protected interior faces on Object29. Prove composite/shared parts before expanding28/30. Coffin objective: believable timber thickness, board joins and readable profiles within original dimensions; establish21/22 relationship before modeling. Use original diffuse art; geometry must earn visible benefit at actual placement scale.

Dungeon inventory contains63 BMDs and4488 placements. Sixty numbered models plus Bat, Rat and DungeonStone must not be treated as63 static replacements. Animals, falling stones, traps, hidden markers and interactive/effect objects are excluded or pending explicit scope classification in next-dungeon-brief.md. Object61 is absent for hidden type60; do not create it. No completed Dungeon asset is claimed yet. All other inventory entries await actual visual/scope assessment.

All original filenames, raw internal model-name bytes, rig/actions, mesh order, UVs, raw normals, bounds, contacts and placements remain protected. Source wrappers may pass original CP949 name bytes to the converter's existing --name option and parse ASCII action records without corrupting comments; do not edit engine/converter code. Retain official importer/exporter provenance and byte checks. Read docs/CODING_RULES.md before scripts.

Per replacement require untouched references, packed source.blend with REF_ORIGINAL, official exports and reimports, converter and authored/exported triangle/bone/UV/winding/normal proofs, matching original/current/candidate/reverse/reduced/wire views and actual placement context. Independent reviewer accepts before root sequential integration. No routine permission questions; user explicitly authorized sustained production, focused commits, owner-fork pushes and draft PRs. Never push upstream, force or merge main.

# Environment current remake board

Lorencia artistic/offline gate: **passed**, 2026-09-23. Next phase: Dungeon static assets; new client checks pending.

The entries below preserve chronological production history; the latest checkpoint supersedes earlier status labels.
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

## 2026-09-23 contextual masonry retention decision
Independent final reassessment retains unchanged HouseEtc01/StoneMuWall02/03/04. In actual stacks, fortification rows and siege context, the hooked nose/eyes, narrow crest and projecting shields provide readable distinct roles and coherent ornament. The earlier isolated boxiness concern over-weighted coarse clay topology for modular masonry; close-up limits remain explicit. No rejected candidate is counted as a remake. Masonry02 deep734tri first study stretched the painting; shallow810tri second study cured that but added negligible visible gain over50tri baseline. Hash-bound individual rationale and evidence: masonry-retention-review.json. Root verified all4 BMD/texture groups; originals unchanged.

Current Lorencia gate:21 accepted new replacements +84 defensible retained assets =105/106 resolved. Waterspout01 is the last unresolved asset, not yet accepted; full21frame/protected-surface/source-normal checks and actual diffuse/effect views remain. Overallgoal incomplete; no latermap production. Furniture is preparing read-only Dungeon queue after committing rejected studies on its own branch.


## 2026-09-23 — Lorencia artistic/offline gate passed

Independent final gate at bb641d04:22 accepted replacements plus84 individually justified retained assets cover all106 in-scope assets. This is not106 new remakes. Final Fountain02 integrates187208c2 as bb641d04: fuller chest, bowed wing membranes and broad charcoal stone painting;921 triangles,11 bones,21 keys. Its dedicated128² dragon atlas changes while all1694 basin/filter-margin texels remain exact through engine-equivalent decoding. All other textures remain unchanged.

Combined scope is22 BMDs plus1 atlas, all115 Object1 models resolve textures. Final reviewer verified accepted-report hashes,254 retained dependency references and five mixed neighborhood comparisons (62 source records,30 images), plus fountain actual placement/effect views. No unresolved known offline production defect remains. Generated JPEG patcher executable is rebuilt from retained source, intentionally untracked. Reports: lorencia-final-gate.json, fountain-integration-validation.json, retained-current-hash-check.json and final-accepted-source-manifest.json.

New client checks remain pending. Prior baseline7b808473 client success is user-reported only. Offline views sample neighborhoods/assemblies and approximate effects; they do not claim a full-map terrain render or live client test. Overall goal remains unfinished: continue Dungeon static assets in a separate focused branch/PR, potteryObject28/29/30 and coffinObject21/22 with shared textures initially frozen. Read-only name-byte round trips pass through official Blender import/export; see dungeon-name-handling.md.
