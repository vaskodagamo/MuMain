# Object37 corridor retention review

**Decision:** retain the current Object37 baseline. This package records a read-only visual check of representative placements; it contains no game-asset replacement.

The placement manifest currently lists 112 Object37 instances. The Blender context render shows six of them at their recorded positions, alongside nearby Object01, Object04 and Object08 geometry. Those six instances retain the repeated column profile and green bands; no visible collision, broken contact, or placed-scale defect gives a concrete reason to remake this asset. The independent review reached the same retention decision. It found the shaft, broad capital, stepped foot and banding readable in normal and reduced views.

This is a sample of six placements, not a visual inspection of all 112, a terrain render, or a game-client test. The context includes only the listed static models. It is intended to support the baseline decision while leaving later map integration and client checks separate.

## Evidence

- `Object37/` contains the official-import baseline blend, import log, individual views and source/dependency provenance.
- `Object01-current/`, `Object04-current/` and `Object08-current/` contain the packed baseline imports used as neighboring context.
- `assemblies/object37-corridor/` contains the three Blender-rendered views and `evidence.json` with exact transforms, input hashes and output hashes.
- `object37_corridor.py` rebuilds the context from the packed imports and the tracked placement manifest. It uses Blender's Python API and the repository's `review_scene.py` helpers.
- `verify_retention.py` checks the package hashes and confirms that the six recorded transforms still match the placement manifest.

Run the integrity check from the repository root:

```sh
python3 assets-work/World2/Readiness03/verify_retention.py
```

To regenerate the context views, use Blender 5.2.2 and set `MU_BMDCONV` to the built `bmdconv` executable:

```sh
MU_BMDCONV=/path/to/bmdconv blender -b --python-exit-code 1 \
  --python assets-work/World2/Readiness03/object37_corridor.py
```

The render is offline and diffuse-lit. No terrain, baked lighting, runtime or client is included.
