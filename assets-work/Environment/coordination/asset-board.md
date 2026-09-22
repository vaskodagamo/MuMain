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
