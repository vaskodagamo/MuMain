# Masonry face sculpture prototype

Status: v1 and v2 are rejected study-only prototypes. V2 passes technical validation but fails the artistic improvement gate. No game BMD was installed. StoneMuWall02/03/04 remain untouched and will not be worked until the first face earns independent visual acceptance.

The starting point is the current merged 50-triangle HouseEtc01, SHA-256 646273f66d753761d78bfc04c33ad9ba73adf4b87e69a2c376a8c530e8fff22c. Rejected QualityPass02/Masonry01 studies are retained separately and are not the integration baseline.

V1 used hand-traced feature plateaus above a recessed background; exported painting stretched across the steep walls and the carving read as detached plaques. It is rejected and archived under study-v1-rejected, including independent review. V2 removes that recessed bed and all inset ledges. Broad smooth eye, cheek and under-muzzle valleys carve 4–5 units over 17–27-unit model-space radii, with an 18-unit transition after the unchanged border. The existing nose maximum and full model envelope remain unchanged. No cap bevel, outer-side or placement change is permitted.

All 24 c_wall04 block/cap/side triangles stay unchanged. A continuous eight-unit band around the decorated face preserves its original piecewise-planar surface, UV registration and bone binding. This is eight model-axis units in X/Z, measured using the original UV registration; it is not geodesic distance along the angled relief. Subdivision of the old broad front triangles is authorized only with that continuous band surface unchanged. The original four outer perimeter edges survive exactly.

Protection is verified through one-to-one frozen block triangle matching, oriented front topology, each border triangle's baseline-plane/UV correspondence, explicit pairwise no-interior-overlap checks, and coverage area per original front triangle. Matching total area alone is not treated as coverage proof. Actual stack placements 30/31 and adjacent 32/38 are prepared as a subsequent seam-profile review only if the artistic direction passes. No placement render is claimed for this prototype.

The source uses Blender constrained triangulation of the baseline edges, border loops and authored feature loops. Carved stone shoulders use a 40-degree crease rule; unrestricted smoothing across steep opposing relief planes was rejected by the strict normal audit. The protected border retains interpolated baseline normals. V2 uses continuous compact smooth valley fields and retains the connected baseline forehead, nose and muzzle.

Textures remain frozen: Object1/c_wall04.OZJ and Object1/c_wall06.OZJ. The official importer/exporter, packed-source byte checks, raw normal world-direction checks, rig/action/mesh-order checks and complete authored triangle position/UV/bone/material/winding correspondence remain mandatory. Sources retain hidden REF_ORIGINAL. Texture payloads are decoded using mu_texture.py.

Review requires actual exported/reimported matching current/candidate diffuse, front, oblique, reduced and neutral-clay images, followed by actual placement assemblies. The preserved box silhouette is expected; a clearly carved face at game distance is the artistic criterion. If the frozen painting and protected envelope prevent a clear gain, retain the accepted baseline and stop the prototype rather than expanding to other models.

All images are offline evidence. New client verification remains pending.

Border audit precision correction: containment is measured as UV point-to-triangle distance, not a dimensionless barycentric weight. One six-decimal SMD corner had weight -1.10104e-6 but actual UV edge distance 2.28659e-7 and baseline-plane error 5.7373e-5 model units. The existing UV 1e-6 and position .0003 tolerances remain unchanged. Pairwise overlap and coverage per original triangle separately prove continuous band coverage. No asset bytes changed for this correction.

V2 owner assessment: the severe UV streaking is cured, but the painted and reduced views show too little improvement for 810 triangles versus the 50-triangle baseline. The clay view confirms only restrained eye/cheek depressions. Independent reviewer concurs: retain baseline rather than propagate this study. The current modular forms remain visually coherent in their architectural role; this retention judgment is separate from the rejected prototype and is documented by the independent reviewer.
