# Current environment remake handoff — 2026-09-23

Lorencia is not artistically complete. Historical 106/106 means first-pass coverage, not final artistic acceptance. Continue autonomously through Lorencia gate, then other maps per objective.

## Accepted integrated work
- Architecture01: HouseWall01/04/05/06, integration `9abdf7e8`. Structural timber bays and shingle courses.
- Scrub01: Tree09/10, Grass03/04, `4b0ccd02`. Continuous growth and rounded crowns. Reproduction baseline fix `1259e4f8`.
- Broadleaf01: Grass05/06, `072f77f6`. Smooth drooping leaf arcs.
- Houses01: House01/03/04, `7be0eaa3` from `32a1a24b`. Stone surrounds, structural awning supports and domed shingle laps. House04 protected rig/motion exact all40frames; authored source normals corrected to final preserved protected normals.
- Statues02: StoneStatue01/03 and SteelStatue01, `cc43c901` from `10d1e765`. Carved niche, layered feather wings and memorial profiles.
- Bonfire02: Bonfire01, `38292593` from `79ed83a5`. Quarter-split wood profiles, curved bark faces and longitudinal grain with unchanged effect shell.
- Wells02: Well01–04, `5a0ad5b5` from `c1296be9`. Open curved pottery mouths, thick roof boards; all actual-placement groups accepted.

All21 replacements independently accepted offline with packed sources, official exports, matching images and technical evidence. Ledger accepted-exports.json; latest statues-integration-validation.json passes exact21 game paths and115model texture resolution. All textures unchanged. Existing client success is user-reported only for baseline7b808473; new changes client pending.

## Active ownership and next actions
Architecture worktree MuMain-q02-architecture / codex/lorencia-q02-architecture: owns ONLY Waterspout01 BMD and QualityPass02/Fountain02. Frozen textures. Edit actual dragon geometry in slot1; additional34 basin triangles there are protected. Preserve other3meshes, all21frames and water/particle interface. Follow next-statues-brief.md/evidence fountain section.

Furniture worktree MuMain-q02-furniture / codex/lorencia-q02-furniture: rejected masonry studies are committed and published as fb734c0c, with no game-file changes. Read-only Dungeon readiness is complete; production waits for Lorencia acceptance.

Reviewer: fountain technical and visual gate. Root alone maintains coordination and integrates accepted owned commits.

Only Waterspout01 remains unresolved. Twenty-one replacements and84 individually justified retained assets resolve105/106 Lorencia assets. Retention evidence is in companion-review.json, retention-gate.json and masonry-retention-review.json. Rejected masonry candidates are not counted as remakes. Coherence01 samples five mixed neighborhoods; fountain and final integrated acceptance remain pending. No later-map production has started.

Fountain normal diagnosis isolates three corners sharing a protected water normal. Preserving original connectivity fixes the rock normals; a read-only test with sharp water boundaries reduces the water error below the existing tolerance. The full export still needs to reproduce that result. Provisional art renders do not constitute acceptance.

## Operations
Root executes stable worker bpy scripts using toolchain.json because inherited worker context Blender execution fails. Full access/approval never: do not pass sandbox_permissions. Set BLENDER and MU_BLENDER, MU_BMDCONV, BLENDER_USER_SCRIPTS and BLENDER_USER_CONFIG explicitly. Pure assembly processes use --factory-startup; do not mix final_evidence and library append in one process. Never interrupt user GUI Blender/client or alter shared runtime. Primary MuMain checkout unrelated edits untouched.

Draft PR12: https://github.com/vaskodagamo/MuMain/pull/12. Verified owner origin git@github.com:vaskodagamo/MuMain.git; focused pushes authorized, never force/upstream/merge. Sources under QualityPass02, independent reports inside accepted batches. Require actual exported visual gain, one-to-one authored triangle position/bone/UV/winding, raw world normals and full motion/contact proof. Protect Beer01/terrain and Grass02/Tree12/Tree13 original BMDs.

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
