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
