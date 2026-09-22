# Environment production handoff

Goal active and incomplete. Integration worktree `/Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake`, branch `codex/environment-remake-continuation`, baseline `7b808473`. Primary checkout has unrelated edits; preserve it.

## Accepted replacements

- Architecture01: HouseWall01/04/05/06, integrated `9abdf7e8`, worker `594b6db2`. Structural timber bays and roof-course depth.
- Scrub01: Tree09/10 and Grass03/04, integrated `4b0ccd02`, worker `51679ab1`. Continuous tall growth and rounded connected crowns,530placements.
- Broadleaf01: Grass05/06, integrated `072f77f6`, worker `0f78f88d`. Smooth long leaf arcs,399placements.

All10BMDs passed independent artistic/technical review, reviewed export hashes and combined texture resolution (115Object1 models). Alltextures unchanged. Detailed reports/sources/comparisons under QualityPass02; ledger `accepted-exports.json`. These are offline accepted, client pending. Prior client success was user-reported for baseline only.

## Active owners

- architecture: isolated MuMain-q02-architecture, branch codex/lorencia-q02-architecture. Owns ONLY House01/03/04 BMDs + QualityPass02/Houses01.13textures frozen, neighbors read-only. Prepare imports complete. Start01/03 static work;04static roof later with all40frame rig/effect protection. Follow `next-houses-brief.md`/evidence.
- furniture: isolated MuMain-q02-furniture, branch codex/lorencia-q02-furniture. Owns ONLY Well01–04 BMDs + QualityPass02/Wells02. All4sharedtextures frozen. Start03/04pot rims/curves then composite01; preserve Cannons01barrels. Follow `next-wells-brief.md`.
- reviewer: preparing next statues/fountain brief and available for independent production gates. No production/shareddocwrites.

Masonry HouseEtc01/StoneMuWall02/03 remain unresolved; baseline retained. Study-only commit `d6e85282` pushed on furniture branch; NOT integrated. Cap changed actual stacked seam; otherstudies weakgain. New solution must preserve connectionprofile. No approvalexception requested.

## Next actions and toolchain

Coordinator runs stable worker Blender scripts because existing worker contexts retained old sandbox. Full access/approvalnever active here; NEVER pass sandbox_permissions. Tool paths/env in toolchain.json. Blender5.2.2, SourceTools3.4.3; modeling via bpy. For pure library assembly renders use --factory-startup to avoid irrelevant SourceTools callback warnings. Do not interrupt existing GUI Blender/client or modify sharedruntime.

Accept only clear visual gains and complete official export/source, rig/action, UV, rawnormal, perroot/contact and one-to-one triangle matching proofs. Integrate focused ownedcommits sequentially; update ledger then validate_quality_pass.py.15productioncandidates remain (3masonry,3houses,3statues,4wells,1fountain,1bonfire),5companions plus retained/coherence finalgate. No latermapproduction until Lorencia gate met.

Draft PR12: https://github.com/vaskodagamo/MuMain/pull/12, ownerfork git@github.com:vaskodagamo/MuMain.git verified. Pushdraft updates authorized; nevermerge/force/upstream. Preserve protected Beer01/terrain and Grass02/Tree12/Tree13 originals.
