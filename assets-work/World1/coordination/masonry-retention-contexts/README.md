# Portable Lorencia masonry retention contexts

This package replaces three stale workstation-only image references in
`../masonry-retention-review.json`. It replays the exact house-stack, south-gate,
and siege-wall placement selections against the current merged BMDs, using the
official Blender importer and the shared diffuse review renderer.

The original placement selections are included under `legacy/`; the current
selection manifests, import scenes, render proofs, source/dependency hashes, and
images are included beside them. `manifest.json` hashes every package file except
itself. `independent-review.json` records the decision and limits.

To replay on the configured Mac, run `python3 replay.py` from this directory. Set
`MU_REVIEW_REPO`, `MU_BLENDER`, and `MU_BMDCONV` if the checkout or tools are in
different locations. The review was generated from owner-fork main
`0cc611bc9165a8f856db88e2bd0f4fe049bcef90`; it remains an offline, selective
assembly review. It includes no terrain, collision, client lighting, or live
client observation.

The regenerated scenes preserve the earlier retention finding: HouseEtc01's
stack seam and repeated heraldry remain legible, the south-gate modules retain
their defensive rhythm, and siege timber/iron remain distinct from stone. The
close-view painted-relief limitation remains explicit; no replacement study is
counted as an accepted remake.
