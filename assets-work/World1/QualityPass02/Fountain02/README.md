# Fountain02

Waterspout01: broader carved chest and bowed bat-wing membranes, a rounded proximal wing arc, and a registered charcoal stone painting with broad chest/neck planes. The historical angular head, wing tips, existing footprint and water outlet remain recognizable. This is an independently accepted offline candidate; new client verification remains pending.

Scope: `src/bin/Data/Object1/Waterspout01.bmd`, `src/bin/Data/Object1/reagon_waterspout.OZJ`, and this deliverable directory. The three other textures, basin/rock/water geometry and full21-key animation contract remain protected. Slot1 includes34 basin faces, so its whole texture is not freely repaintable.

Baseline639 triangles; candidate921,11 bones,1 action with21 keys,4 material slots,1 actual placement. The original and merged baseline BMDs, paintings and official imports remain alongside packed `source.blend` with `REF_ORIGINAL` and `REF_BASELINE`. Sources and game exports use the same native128² JPEG painting. High-resolution generated masters are editable provenance, not the shipped resolution or proof of quality.

Reproduce from this worktree with `BLENDER` pointing to official Blender5.2.2, enabled SourceTools3.4.3 via `BLENDER_USER_SCRIPTS`/`BLENDER_USER_CONFIG`, and `MU_BMDCONV` pointing to the existing converter. `MU_ART_PYTHON` should point to Python with Pillow and NumPy; current default is the bundled Codex runtime. The lossless JPEG helper compiles with the host C compiler and `pkg-config libjpeg` (jpeg-turbo).

1. Use `package_texture.py artwork/dragon-draft02-128.png Waterspout01/textures/paint02` from this batch with the artwork Python. It retains original JPEG quantization/4:4:4 and untouched DCT blocks. The128² draft is a Lanczos resample of `artwork/dragon-atlas-master02.png`; retained prompts and input references explain generation.
2. Copy the packaged `patched.jpg` to `Waterspout01/textures/paint02/reagon_waterspout.jpg`. Run `engine_decode_proof.py Waterspout01/textures/paint02`.
3. From repository root, `python3 assets-work/World1/QualityPass02/Fountain02/pipeline.py experiment` prepares hash-verified references, runs `build_curve.py` and `apply_paint.py`, officially exports/reimports, validates all contracts and renders matched direction views. Default `FOUNTAIN_PAINT=paint02`.
4. Run `python3 assets-work/World1/QualityPass02/Fountain02/final_review.py` for separate-process original/wireframe, actual placement front/reverse/reduced, scrolling-water keyframes and additional neutral/unlit diagnostic comparisons.

The pipeline never installs into Data. Final installation requires the independent review decision at the exact export/source/texture hashes.

Technical evidence covers each authored corner's bone/position/UV/material/winding, zero-area checks, protected triangle correspondence, raw normal ownership and directions over all21 poses, motion component/matrix equivalence, source geometry over all21 poses, and mouth/water/particle attachments. Strict tolerances were retained. Raw motion is numerically equivalent, not byte-identical: the official export has float rounding at about4.58e-5; no motion substitution is used.

The custom-normal codec investigation is retained. Protected rock topology uses original vertex identities, and water-only sharp fan boundaries allow Blender to encode the intended original smooth normals accurately. This is an authoring representation correction, not a change to desired water shading. Independent study01 measured maximum protected normal difference0.000144803 under the unchanged0.001 gate.

Texture packaging protects1694 basin/filter-margin pixels and all129 unselected8×8 blocks. Both Pillow decoding and the engine-equivalent TurboJPEG FASTDCT check require exact zero RGB difference. Engine texture loading uses one mip level; the two-pixel guard also covers bilinear filtering. Three shared textures remain byte-identical. The complete consumer analysis and source code lookup establish the dedicated atlas's only resolved consumer as Waterspout01, including its protected basin faces.

`experiments/study01` is technically accepted but artistically rejected; `study02` and `paint01` retain subsequent art revisions. Their decision files distinguish completed checks from inherited reports. Final current proof must be used for acceptance.

All images are offline. Emission/unlit images are diagnostic only. Water additive/UV scrolling is explicitly approximate; particles are not simulated, and their anchor envelopes are checked numerically. Runtime performance and new client verification remain pending. The user's earlier successful Lorencia client result applies to the baseline, not this candidate.
