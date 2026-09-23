# Current checkpoint — 2026-09-23

Current owner-fork main is `1256aeed`. The Lorencia independent artistic/offline gate is complete at `bb641d04` (22 replacements plus 84 justified retention decisions); new client verification remains pending. Four Dungeon replacements are merged. Retention PRs #17–#20 have also merged: Objects49/50, Object51, Object37 and Object42/43 remain unchanged. PR #22 records Objects45–47; PR #23 documents rejected support studies for Objects06/13/15 and passed independent review. PR #21, editor documentation, also merged. Continue Dungeon's actual-placement inventory before selecting another production batch.

# Environment production progress

2026-09-22: verified owner origin git@github.com:vaskodagamo/MuMain.git and remote main
7b808473. Preserved unrelated primary WORKLOG.md and docs/build/macos/console.md edits.
Created isolated integration and two worker branches/worktrees. Independent reviewer verified
106 historical preview manifests; first two geometry-only production batches dispatched.
Blender 5.2.2 downloaded from official release and SHA-256 verified; isolated Source Tools 3.4.3 setup complete. Blender must run outside the sandbox because sandboxed Metal GPU detection crashes before Python starts. All production modeling uses Blender Python API.
No new game files integrated; no runtime writes/client launch; no later maps begun.

2026-09-22 continuation: user enabled full access. Coordinator executes Blender pipelines
without approval prompts; existing worker contexts retained sandbox behavior. Masonry v1
rejected artistically (shallow tray-like top and weak reduced-scale gain); revised motif
relief and mouldings under review. Architecture imports and bpy source construction now
complete. No candidate integrated or client verified yet. Next scrub dependency brief ready.

Independent review rejected masonry motif-relief study (762/476/580 triangles):
side dressing improves, but assembly/reduced-scale silhouette gain insufficient for added
geometry. Baseline BMDs retained; artist revising exposed edge/cap geometry and reducing
low-payoff tessellation. Architecture revised exports pass converter/rig/raw-normal/UV/bounds
checks; individual review ready, assembly render script repair pending. No accepted game changes.

Accepted first checkpoint: four modular assets integrated9abdf7e8; final reviewer BMD/image hashes match. Combined check115/115 resolve and exact4-path game scope. Dispatched530-placement scrub family next; masonry revision remains unaccepted. No runtime/client or other-map work.

Published accepted architecture checkpoint as draft[PR12](https://github.com/vaskodagamo/MuMain/pull/12), branchcodex/environment-remake-continuation atda7f9bb3. Attached to current task. Continuing production; no merge.

Masonry study-only checkpoint d6e85282 retained on worker branch; all three game BMDs unchanged. House cap altered a stacked seam, other studies lacked clear visual gain. Broadleaf Grass05/06 production started with frozen tree_09.OZT. Scrub first experiment rejected; shape and cross-tuft export issues under focused revision. Four architecture assets remain the only accepted replacements.

2026-09-22: Scrub01 accepted offline and integrated4b0ccd02 from worker51679ab1. Tree09/10 continuous growth replaces pinched broom waists; Grass03/04 rounded connected crowns replace angular layered shells. Exactly8 game BMDs now differ from baseline; all115Object1 models resolve, frozen textures/protected paths unchanged. 530 scrub placements unchanged. Houses01 assigned to architecture (House01/03/04 only); Broadleaf01 final review pending. No new client verification or later-map work.

2026-09-22: Broadleaf01 integrated072f77f6 from worker0f78f88d. Grass05/06 long smooth arcs replace folded elbows across399placements. Independent full/reverse/reduced/light-dark and bothsix-placement assemblies accepted; technical/rawnormal/rig/UV/source-export checks passed. Combined10changedBMDs/115modelsresolved. Alltextures unchanged. AssignedWell01–04 to furniture underWells02; Houses01/03/04 active witharchitecture.15productioncandidates remain plus5companions/coherence review. No latermaps/clientclaims.

Companion review retained HouseWall02/03,HouseEtc02,StoneMuWall01 with explicit role/material/effect rationale and verified currenthashes. StoneMuWall04 masonry front joins production queue (cannon/platform stay protected), leaving16productioncandidates; whole-environment coherence review remains.

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

Current final-group work: Fountain02 first direct export shows raw/world translation precision4.5776367e-5 and unchanged posed bounds across21frames; full authored/protected/action/particle proofs in progress, no acceptance yet. Detailed independently parsed scope in fountain-contract-review.md protects256non-dragon triangles, including34inside slot1; baseline12averaged versus45per-corner normal incidents explicitly distinguished. Masonry02 first734tri prototype technically preserved8unitborder but was artistically rejected for deep striped extrusions. One shallow continuous-relief revision is running; no propagation or game-file installation.

## 2026-09-23 contextual masonry retention decision
Independent final reassessment retains unchanged HouseEtc01/StoneMuWall02/03/04. In actual stacks, fortification rows and siege context, the hooked nose/eyes, narrow crest and projecting shields provide readable distinct roles and coherent ornament. The earlier isolated boxiness concern over-weighted coarse clay topology for modular masonry; close-up limits remain explicit. No rejected candidate is counted as a remake. Masonry02 deep734tri first study stretched the painting; shallow810tri second study cured that but added negligible visible gain over50tri baseline. Hash-bound individual rationale and evidence: masonry-retention-review.json. Root verified all4 BMD/texture groups; originals unchanged.

Current Lorencia gate:21 accepted new replacements +84 defensible retained assets =105/106 resolved. Waterspout01 is the last unresolved asset, not yet accepted; full21frame/protected-surface/source-normal checks and actual diffuse/effect views remain. Overallgoal incomplete; no latermap production. Furniture is preparing read-only Dungeon queue after committing rejected studies on its own branch.

Masonry02 rejected studies retained in furniture study-onlycommit fb734c0c (do not cherry-pick into accepted integration). Read-only Dungeon inventory/brief retained separately;63BMD,4488placements with static/effect exclusions. Candidate first family pottery Object28/29/30,45placements; start Object29 only after Lorencia gate. No Dungeonproduction has started.

2026-09-23: Published checkpoint7929166c to owner fork and refreshed draft PR12 to21 replacements +84 retained, only fountain unresolved. Fountain water-only sharp normal fans fix the export encoding without changing protected geometry or tolerance; full pipeline passes. First sculpture direction remains rejected for insufficient front-view gain, with a focused second geometry study in progress. Dungeon readiness remains read-only.


## 2026-09-23 — Lorencia artistic/offline gate passed

Independent final gate at bb641d04:22 accepted replacements plus84 individually justified retained assets cover all106 in-scope assets. This is not106 new remakes. Final Fountain02 integrates187208c2 as bb641d04: fuller chest, bowed wing membranes and broad charcoal stone painting;921 triangles,11 bones,21 keys. Its dedicated128² dragon atlas changes while all1694 basin/filter-margin texels remain exact through engine-equivalent decoding. All other textures remain unchanged.

Combined scope is22 BMDs plus1 atlas, all115 Object1 models resolve textures. Final reviewer verified accepted-report hashes,254 retained dependency references and five mixed neighborhood comparisons (62 source records,30 images), plus fountain actual placement/effect views. No unresolved known offline production defect remains. Generated JPEG patcher executable is rebuilt from retained source, intentionally untracked. Reports: lorencia-final-gate.json, fountain-integration-validation.json, retained-current-hash-check.json and final-accepted-source-manifest.json.

New client checks remain pending. Prior baseline7b808473 client success is user-reported only. Offline views sample neighborhoods/assemblies and approximate effects; they do not claim a full-map terrain render or live client test. Overall goal remains unfinished: continue Dungeon static assets in a separate focused branch/PR, potteryObject28/29/30 and coffinObject21/22 with shared textures initially frozen. Read-only name-byte round trips pass through official Blender import/export; see dungeon-name-handling.md.

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

## First prototype directions

Object29 rounded shoulder and thick open ceramic lip passed independent direction review at782 triangles with exact original interior and all tilted support bounds; a lower-density16-side version is being checked before shared family propagation. The first Object22 lid passed its authored export audit but paired context showed weak artistic gain, and a separate tilted-placement audit caught up to1.635 units of horizontal expansion. That study is rejected for final acceptance. The artist is correcting its profile and focusing on tangible Object21 wall/rim construction, then reviewing the pair. No Dungeon game file is accepted or installed in integration yet.
