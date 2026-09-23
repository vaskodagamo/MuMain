# Dungeon bone-remains baseline review

Read-only baseline triage for Objects45–48 from main7b808473. No BMD or texture changes, no art acceptance, and no client verification. Official Blender import with the ASCII-action adapter was used; each packed preview and diffuse render is hash-bound to its exact BMD and complete texture dependency.

| Model | Placements | Triangles | Appearance in current diffuse preview | Raw model identity |
|---|---:|---:|---|---|
| Object45 | 21 | 486 | Tall collapsed skeletal remains | `Data2\Object2\해골b.smd` |
| Object46 | 25 | 486 | Compact seated/folded skeletal remains | `Data2\Object2\해골c.smd` |
| Object47 | 79 | 480 | Small loose skull-and-bone cluster | `Data2\Object2\해골d.smd` |
| Object48 | 385 | 284 | Long scattered bone arrangement | `Data2\Object2\해골e.smd` |

Together these are 510 static placements. All have one root bone and one one-key action; Dungeon engine object-type paths 44–47 do not add effect or operation handling. The map inventory and model names identify them as skeletal remains. An actual close placement assembly is included for the dense Object47/Object48 cluster at source placement indices 133, 126, 125, 135, 124 and 134.

All use `bons.OZJ`, frozen during this triage. It has nine consumers (Objects19, 32, 33, 44–48, 54), so future work must account for that full set. Objects45 and 46 have the same raw triangle vertex/normal index topology and counts, but different root names, poses and bounds; this does not prove identical geometry, UVs or contacts. Objects47 and 48 intentionally contain different scattered forms.

The previews justify a follow-up close visual review only. They are isolated diffuse views without terrain, occlusion or game camera distance. Do not start replacements until actual visual value, posed-family correspondence, all placements and the shared texture consumers are assessed.
