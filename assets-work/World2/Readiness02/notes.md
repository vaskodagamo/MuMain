# Dungeon bones Objects45–47: baseline retention review

Read-only review against `7c25cce6911a0e3f8737e3214dcd12c982c18728`. No BMDs or textures changed. The official Blender importer produced packed individual source scenes and diffuse-only front, reverse and reduced previews for each model. The reviewer recommends retaining all three baselines; this is a visual decision, not an art replacement or client verification.

| Model | Placements | Decision | Evidence and limits |
|---|---:|---|---|
| Object45 | 21 | Retain baseline | Tall collapsed pose, skull and separated limbs remain legible at reduced scale. No actual placed-neighbor view was established; a game-scale correction is unproven. |
| Object46 | 25 | Retain baseline | Seated arm-and-knee silhouette reads distinctly. Extra rounding or ribs showed no reduced-scale benefit; no actual placed-neighbor view was established. |
| Object47 | 79 | Retain baseline | The loose skull cluster remains coherent alongside accepted Object48 in two actual Object47 placements and four neighboring Object48 placements. The skull crowns are angular in close views, with no demonstrated in-game-scale payoff from smoothing. |

All three use the frozen `bons.OZJ` dependency. Their baseline BMD and texture hashes, packed-scene hashes, and preview hashes are in each model's `provenance.json`. Object47's contextual review references the exact source placement records and accepted Object48 assembly evidence; `retention-review.json` and `verify_retention.py` bind those references to the main checkout.

This review sampled each model individually and used a real placement cluster only for Object47. It does not claim inspection of every placement, a terrain render, a live game client, or artistic completion of the Dungeon. Object45 and Object46 remain eligible for reassessment if a visible placed-scale defect is later demonstrated.
