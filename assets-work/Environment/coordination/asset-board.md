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
