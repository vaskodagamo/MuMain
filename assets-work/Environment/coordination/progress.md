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
