# Dungeon environment production — current status, 2026-09-23

Lorencia is separately accepted offline at d3ce9acf in draft PR12. The user reports the earlier Lorencia baseline working in game; new changes still need their own client verification.

The focused Dungeon PR is [#13](https://github.com/vaskodagamo/MuMain/pull/13), branch `codex/dungeon-static-remake` at checkpoint 3e0196c3. Object28 and Object29 pottery are the only Dungeon game replacements integrated to date. They use more rounded shoulders and true ceramic lips, with interiors, broken fragments, materials and all placement contacts preserved. Independent technical/art review passed; offline dependency validation covers all63 Object2 models. New client verification is pending.

Both coffin models Object21/22, Object30 broken pot, Object03 niche pillar and Object04 carved pier are individually retained on their visual merits after candidate inspection. Their attempted replacements are not counted. Object03 and Object04 report distinct baselines and checks; `Pier01` contains packed original references, official exports, two fully audited rejected geometry studies, three rejected registration studies, consumer masks and matching placement views. Worker study commit4199d62f is integrated as evidence only; no Object04 game BMD or texture changed.

Object01’s 64-triangle central crown chamfer is independently accepted at exact exported BMD hash `aea82532ebb824c2aac1cbbe277135ad96a7379f3f9a615a6a1b9ccfcae1ee01`. It adds a coherent, correctly textured broad stone edge plane with6 triangles while preserving its plaque, all module ends, bounds, normal/action and1262 placement contracts. This is a bounded visual refinement, not a full wall remake. Artist is completing original/wire/source/provenance package before integration. Object03 is separately retained as its existing four-niche tapered variant.

Next family Object45–48 now has official-imported read-only renders, exact model/texture provenance and a six-placement Object47/48 cluster. The family covers510 placements and uses frozen `bons.OZJ` shared by nine consumers. No candidate or owner is assigned pending closer scene assessment.

Currently pending final delivery: Object01 source and wireframe completeness, worker commits/pushes for final wall package, focused integration validation and PR update. No runtime Data/game client is touched, and no new client result is claimed. See `assets-work/Environment/coordination/` and the matching `assets-work/World2/` evidence directories. The overall static environment goal remains in progress.


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
