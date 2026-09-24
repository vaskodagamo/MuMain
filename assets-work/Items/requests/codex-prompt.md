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

Change only the request's owned files, deliver into its delivery/ folder with all review images and validation output, run validate_request.py and set the status to delivered. Push and open a PR on vaskodagamo/MuMain only if handoff.push_allowed in request.json is true; never merge. Record your session in docs/agents/WORKLOG.md. If the request is not assigned to you, the validator says it is stale, or anything is unclear, stop and tell the owner.
