# Frequent Dungeon architecture — baseline inspection

Read-only previews at main7b808473. No replacement, final art decision or client verification is implied. Each provenance.json binds original BMD and wrapped textures to the packed import and diffuse image. The official importer is unchanged; the small adapter parses only ASCII action records around legacy CP949 comment bytes.

| Model | Placements | Observed form | Dependencies |
|---|---:|---|---|
| Object01 | 1262 | Stepped dungeon wall with recessed dragon panel and square side piers | deep_wall01.OZJ, deep_wall02.OZJ |
| Object04 | 588 | Tall stone wall pier with projecting creature-face relief | deep_wall01.OZJ, deep_wall03.OZJ |
| Object06 | 179 | Downward-rooted stone bridge support with open upper trim | deep_wall04.OZJ |
| Object13 | 50 | Related capped stone support | deep_wall04.OZJ, deep_wall01.OZJ |
| Object15 | 217 | Thin open stone bridge trim | deep_wall04.OZJ |

These forms require actual modular assemblies before choosing useful changes. Do not mistake Object06 for a brazier from its isolated open top: raw source name identifies a stone bridge, and its geometry extends down from its origin. Object06/13 share229 placements in total, not229 each. Verify surface correspondence rather than assuming identical-looking supports are interchangeable.

Potential priorities are the highly repeated wall/pier family and its carved face, but preserving connections and readable motifs matters more than adding subdivisions. The five snapshots are baseline evidence for the next production decision; current artists remain assigned to pottery and coffins.

Shared texture scope: deep_wall01 also serves excluded falling stone/trap consumers, and deep_wall04 includes pottery interiors and trap consumers. Keep both frozen. deep_wall02 serves Object01/03; deep_wall03 serves Object04/16/17. Those complete consumer groups need inspection before any painting assignment. No texture changes are authorized by this preview.

Run prepare.py using MU_BLENDER, MU_BMDCONV and the isolated Blender script/config environment. No game files are written. It rejects changed baseline inputs when prior evidence exists. Existing root-only source paths are resolved relative to this worktree.
