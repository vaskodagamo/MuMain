# Codex prompt for an item request

The Item Editor's **Requests** tab copies this prompt for the selected request (**Copy Codex
prompt**); paste it into a new Codex session, one session per request. Edit the text freely: the
editor reads this file each time. Everything above the `## prompt` heading (this introduction) is
left out. `$id` is the request's folder name, `$branch` its worker branch and `$label` the worker
label (the branch without `codex/`).

## prompt

You are the item art builder for this repository. Read AGENTS.md, ASTRA.md and assets-work/Items/requests/README.md first and follow them exactly.

Your request is $id (assets-work/Items/requests/$id/). Your worker label is "$label" and your branch is $branch. Run git fetch origin, check that assets-work/Items/assignments.json on origin/main assigns $id to "$label", then follow "Worker rules (Codex)" in the requests README step by step: create your worktree and branch, claim the request, then build the item.

Read brief.md and request.json in the request folder and the targets' catalog entries before you start. When the request includes captures/ref-concept.jpg, the owner picked that concept: build the item as close to it as the game's limits allow. A set request covers every listed part; rebuild them together so they read as one set.

When "What to change" names a reference mesh (a .glb under ../item-sources/), build from it instead of modelling from scratch: import it into Blender, build a clean closed low-poly model over it within the request's triangle limit (retopology: simple closed shapes for each part; do not decimate the raw generator mesh, which tears it into loose triangles), bake its colour and detail onto one diffuse texture, and fit it to the original model's origin, grip, size and orientation. The mesh stays where it is: never copy it (or a .gltf export of it) into the repository; source.blend and exports/ hold only the reduced model (the validator refuses any delivery file over 8 MB).

Three checks before you deliver, because the offline previews hide them:
- Orientation: the model origin is where the game attaches the item (the hand holds a weapon there), so the grip (or the attachment point of armour, shields and wings) must sit at the origin exactly as in the original model, pointing the same way. Overlay the original model in Blender to check it, and make the hand-fit review show the hand on the grip, not on the tip.
- Closed mesh: the engine draws only the front of each triangle, so any hole shows through from the other side. Select > Select All by Trait > Non Manifold must select nothing, and normals must face outward.
- Texture: the engine shrinks textures (mipmaps), so black or empty areas around UV islands bleed into the item as dark blotches. Unwrap with few large islands (seams along edges and between parts), bake with a margin of at least 16 px using extend, fill the unused texture area with neighbouring colour instead of black, and save the JPG at high quality. Look at the texture itself before you export.

Change only the request's owned files, deliver into its delivery/ folder with all review images and validation output, run validate_request.py and set the status to delivered. Push and open a PR on vaskodagamo/MuMain only if handoff.push_allowed in request.json is true; never merge. Record your session in docs/agents/WORKLOG.md. If the request is not assigned to you, the validator says it is stale, or anything is unclear, stop and tell the owner.
