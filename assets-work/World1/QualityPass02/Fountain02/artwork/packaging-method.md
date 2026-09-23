# Minimal safe JPEG block packaging

No dependency installation required: /opt/homebrew libjpeg (jpeg-turbo), clang, bundled Pillow+numpy exist. This is format packaging only; input artwork remains untouched.

Compile /tmp/astra-jpeg-block-patch.c:
clang -O2 -I/opt/homebrew/include /tmp/astra-jpeg-block-patch.c -L/opt/homebrew/lib -ljpeg -o /tmp/astra-jpeg-block-patch

Run bundled Python /tmp/astra-fountain-package.py INPUT128.png OUTPUTDIR [--baseline ORIGINAL.OZJ] [--masks MASKDIR] [--patcher EXECUTABLE]. It currently imports official mu_texture from the furniture worktree tools; when retaining inside Fountain02 use the architecture repository's unchanged tools path. The three input masks required are body-mask.png, wings-mask.png and protected-basin-plus2px.png from /tmp/astra-fountain-texture; retain uv-coverage.json and mask-generation script as provenance. Masks come from unchanged current baseline UV topology and must be checked against any UV edits (currently none).

Input must be128²opaqueRGB/RGBA; caller may explicitly downsample generated master once using Lanczos. Candidate encoding uses original quantization tables and4:4:4. The C helper reads original and candidate DCT arrays and copies only selected8×8 blocks from candidate to original coefficients. Selection: any dragon body/wing coverage in block AND no protected basin/filter-margin coverage. All other blocks remain original. Original ICC and application metadata retained; component mapping/color space/quant tables/sampling/dimensions are strict assertions. Default encoding optimizations do not affect retained coefficient values. No changes to game files, tools or exporter.

After writing and wrapping, Python decodes the actual packed output through official mu_texture. It requires exact decoded RGB equality across all1694 protected/filter-margin texels AND all pixels in every unselected block, exact original quant tables/subsampling/ICC, plus OZJ roundtrip payload equality. Run mu_texture.py check too. Actual original atlas has128²RGB and4:4:4,127 selected spatial blocks,129 retained. Existing baseline quantization is intentionally preserved; no silent new chroma subsampling.

Tests completed:
- Synthetic solid-color candidate, confirming drastic editable-region changes cannot alter protected pixels.
- Generated master01 resized128²; final packed hash ee64c7b4595055431ccb0ec838e307528d187e7157b5fe7a24bd650ae0fc3055, protected and all-unselected max RGB error0; ICC preserved; official container checkPASS.

Output /tmp/astra-fountain-packaged01/reagon_waterspout.OZJ is a candidate only. Artistic review on actual exports still required; numerical protection is not artistic acceptance. The packager protects entire sampled blocks; it does not guarantee draft boundary continuity, so inspect body/wing edges in textured model views.
