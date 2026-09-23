# Current environment remake handoff — 2026-09-23

The overall goal remains active. Lorencia passed its independent final artistic/offline gate at `bb641d04`; this closed its 22-replacement/84-retention assessment, while new client verification is pending. Main includes Dungeon replacements Object28, Object29, Object01 and Object48. Retention PRs #18, #19 and #20 have merged. PRs #22 and #23 remain open for review; new client verification is pending.

## Current pull requests

- PR #18 (merged): retain Object51 after the rejected bevel study.
- PR #19 (merged): retain Object37 after six actual corridor placements.
- PR #20 (merged): retain unchanged Object42/43 fire props after independent review.
- PR #22: Object45–47 baseline retention after individual and actual-cluster review; ready for review.
- PR #23: reject the proposed Object06/13/15 replacements and retain current BMDs; independently reviewed and ready for review.

## Active studies and next queue

Architecture's Object06/13/15 support/collar study found too little reduced-scale improvement to justify its candidates; independent review agreed, and the original assets remain unchanged. Furniture's merged Objects42/43 package preserves fire meshes, texture and runtime anchors. The independent static triage of Objects05/07/08/09/10/11/14 did not identify a coherent 3–5 asset production batch: Object05's isolated surface issue is small/partly occluded in context, Object09 overlaps animated Object12 and needs all-frame clearance before any geometry work, and the remaining models already read clearly at actual scale. Continue with documented context and branch evidence; do not claim client verification.

## Active checkout

Dungeon production branches start from owner-fork main `1256aeed`. Keep the primary checkout's unrelated user edits untouched. Use Blender's official Python importer/exporter for any future candidate; game assets and textures remain unchanged in the current retention PRs. Open focused PRs when each evidence package passes verification; never merge main for the user.

---

## Historical handoff snapshot before Object48 integration
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

# Current environment remake handoff — 2026-09-23

## 2026-09-23 — Lorencia artistic/offline gate passed

Independent final gate at bb641d04:22 accepted replacements plus84 individually justified retained assets cover all106 in-scope assets. This is not106 new remakes. Final Fountain02 integrates187208c2 as bb641d04: fuller chest, bowed wing membranes and broad charcoal stone painting;921 triangles,11 bones,21 keys. Its dedicated128² dragon atlas changes while all1694 basin/filter-margin texels remain exact through engine-equivalent decoding. All other textures remain unchanged.

Combined scope is22 BMDs plus1 atlas, all115 Object1 models resolve textures. Final reviewer verified accepted-report hashes,254 retained dependency references and five mixed neighborhood comparisons (62 source records,30 images), plus fountain actual placement/effect views. No unresolved known offline production defect remains. Generated JPEG patcher executable is rebuilt from retained source, intentionally untracked. Reports: lorencia-final-gate.json, fountain-integration-validation.json, retained-current-hash-check.json and final-accepted-source-manifest.json.

New client checks remain pending. Prior baseline7b808473 client success is user-reported only. Offline views sample neighborhoods/assemblies and approximate effects; they do not claim a full-map terrain render or live client test. Overall goal remains unfinished: continue Dungeon static assets in a separate focused branch/PR, potteryObject28/29/30 and coffinObject21/22 with shared textures initially frozen. Read-only name-byte round trips pass through official Blender import/export; see dungeon-name-handling.md.

## Current operations

Lorencia integration: MuMain-environment-remake / codex/environment-remake-continuation; draft PR12 https://github.com/vaskodagamo/MuMain/pull/12. Accepted exports are listed exactly in accepted-exports.json. Source and export hashes are in final-accepted-source-manifest.json. Fountain final independent review is QualityPass02/Fountain02/independent-review.json.

Architecture artist branch187208c2 and furniture study branchfb734c0c are published to the verified owner fork. Dungeon production is now active: integration MuMain-dungeon-remake / codex/dungeon-static-remake at114d26cd; furniture worktree reused on codex/dungeon-pottery for Object28/29/30; architecture worktree reused on codex/dungeon-coffins for Object21/22. All start from verified main7b808473, with no Dungeon accepted replacement yet. Untracked caches were preserved. Continue from the Dungeon integration handoff; this Lorencia checkout remains stable for source and review references. Primary MuMain checkout unrelated edits and runtime remain untouched. Full access, no routine permission prompts. Root executes stable Blender Python scripts using toolchain.json when worker environments cannot launch Blender.

## Historical checkpoints

## 2026-09-23 bonfire and retention checkpoint
Bonfire01 accepted/integrated `38292593` from `79ed83a5`: quarter-split wood with curved bark faces and longitudinal grain;38 effect triangles and exact hot-end interface protected. Offline additive approximation/actual tilted placements accepted; client pending. Total18 new replacements. Independent individualized retention decision for76 baselines in retention-gate.json plus earlier4companions yields80 defensible retained assets; not80new remakes and not client verification. Root rechecked all217retention dependencyreferences/76imagehashes.

Coherence01 `ec35b6f4` contains5actual neighborhood regions,121instances,30matching images with complete source/placement/hash provenance; sampled material coherence accepted by reviewer with no-terrain/effect limits. Remaining8production:3statues atfinalreview,4masonry nowownedbyfurniture(Masonry02 firstHouseEtcfaceprototype),1fountain unclaimed. OverallLorencia incomplete; latermaps notstarted.

## 2026-09-23 statues checkpoint
Statues02 accepted/integrated `cc43c901` from `10d1e765`: StoneStatue01 carved niche around figure(212tris), StoneStatue03 layered curved feather wings and continuous robe/arm forms(1353), SteelStatue01 stepped capital and raised plaque(274). Root independently verified3BMD/source hashes and62reviewed evidence hashes. Full authored triangle/UV/bone/normal and exactcontact proofs pass; all frozen materials match integration. Corrected context omits engine-hidden PoseBox01 and frames entire pillar.

Total21 accepted new replacements +80 individually defensible retained assets; remaining5production: HouseEtc01/StoneMuWall02/03/04 owned furniture(Masonry02), Waterspout01 owned architecture(Fountain02). Fountain only actualdragon geometry; protect additional34 basin/floor triangles inside slot1 as well as entireother3meshes, all21motionframes/particles/waterinterface. Masonry onefaceprototype must earn gain before expansion; exact8unitborder surface and fullcoverage/nonoverlap proofs mandatory. Lorencia incomplete, latermaps notstarted, allnewclientchecks pending.

## 2026-09-23 contextual masonry retention decision
Independent final reassessment retains unchanged HouseEtc01/StoneMuWall02/03/04. In actual stacks, fortification rows and siege context, the hooked nose/eyes, narrow crest and projecting shields provide readable distinct roles and coherent ornament. The earlier isolated boxiness concern over-weighted coarse clay topology for modular masonry; close-up limits remain explicit. No rejected candidate is counted as a remake. Masonry02 deep734tri first study stretched the painting; shallow810tri second study cured that but added negligible visible gain over50tri baseline. Hash-bound individual rationale and evidence: masonry-retention-review.json. Root verified all4 BMD/texture groups; originals unchanged.

Current Lorencia gate:21 accepted new replacements +84 defensible retained assets =105/106 resolved. Waterspout01 is the last unresolved asset, not yet accepted; full21frame/protected-surface/source-normal checks and actual diffuse/effect views remain. Overallgoal incomplete; no latermap production. Furniture is preparing read-only Dungeon queue after committing rejected studies on its own branch.

Masonry02 rejected studies retained in furniture study-onlycommit fb734c0c (do not cherry-pick into accepted integration). Read-only Dungeon inventory/brief retained separately;63BMD,4488placements with static/effect exclusions. Candidate first family pottery Object28/29/30,45placements; start Object29 only after Lorencia gate. No Dungeonproduction has started.
