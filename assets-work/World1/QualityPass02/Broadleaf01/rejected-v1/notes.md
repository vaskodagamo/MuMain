# Broadleaf curve study — in review

Scope: Grass05 and Grass06, 399 placements in Lorencia. tree_09.OZT is frozen byte-for-byte. The prior Groundcover01 work improved the painted leaf but retained the original folded ribbon geometry.

The candidate replaces each of sixteen ribbons with a smooth longitudinal arc and a shallow center rib. Monotone cubic interpolation preserves the original edge control points and prevents overshoot. Every original edge control, including roots, tips, and ground contacts, occurs exactly in the authored mesh. Four original roots, their action, original texture coordinates at control rows, and overall bounds remain. The alpha image supplies the tapered leaf outline. Triangle count is 448 per asset (112 before), within the foliage budget.

Official Blender 5.2.2 and SourceTools 3.4.3 import/export both models. The pipeline checks local skeletal motion as rotation matrices (equivalent Euler branches differ), action metadata, engine bounds, raw normal-to-vertex bone identity, frozen texture validation and exact cyclic triangle/material/bone/UV correspondence after reimport. Position comparison permits 0.0003 world units for six-decimal SMD Euler and float32 rounding; UV comparison remains within 0.000001. Grass05's measured first export position drift is 0.0001415 with zero UV drift. This is distinct from the authored control-point proof, whose error is exactly zero.

Actual candidate exports are reimported before matching-camera diffuse, reverse, reduced-size and wireframe renders. No game-file installation or acceptance is implied by a technical PASS. Independent visual review is pending. No client was opened.

Reproduce with the isolated official toolchain environment (MU_BLENDER, MU_BMDCONV, BLENDER_USER_SCRIPTS, BLENDER_USER_CONFIG), then run pipeline.py prepare followed by pipeline.py build. make_comparisons.py requires Pillow. Baselines and original packed source files are retained beside each asset; authored source.blend includes hidden REF_ORIGINAL and packed frozen texture.
