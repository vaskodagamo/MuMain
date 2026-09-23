# Item art baseline study

This is an offline inventory and art-direction baseline for the existing MU item assets. It records the current source geometry and texture dependencies so future reworks can be scoped without changing game data. No model or texture under `src/` was changed, and this study does not create the item editor catalog or request folders.

## Scope and findings

`baseline.json` contains **207 item-folder BMDs** across the weapon, shield, spellbook, ammunition, and wing filename families, plus named special item models, and **463 player armor-part BMDs** across 97 armor, 94 boot, 85 glove, 92 helm, and 95 pant models. Armor includes base-class parts, class/sex variants, HDK/CW parts, and nested LuckyItem parts. This is an asset-file inventory, not a complete in-game item-ID mapping; that belongs to the item-editor catalog milestone.

`bmdconv info` succeeded for all 670 models. All 670 texture references resolved. The report records triangles, mesh count, bone count, action count, bind-pose bounds, per-mesh triangle/texture/render-flag data, texture container and pixel dimensions, and shared model consumers. It found **131 shared texture files**. Full per-model records and shared-consumer lists are in `baseline.json`.

| Family | Models | Triangles, range / median | UV triangles >8:1 | Quality |
|---|---:|---:|---:|---:|
| Axe | 9 | 38–112 / 60 | 30 / 432 (6.94%) | 2/5 |
| Bow and crossbow | 24 | 58–783 / 217 | 364 / 6429 (5.66%) | 2/5 |
| Bow ammunition | 2 | 42–75 / 58.5 | 7 / 101 (6.93%) | 2/5 |
| Mace | 22 | 36–1194 / 395 | 279 / 9177 (3.04%) | 2/5 |
| Shield | 22 | 18–408 / 84 | 81 / 2909 (2.78%) | 2/5 |
| Spear | 11 | 46–162 / 112 | 42 / 944 (4.45%) | 2/5 |
| Spellbook | 22 | 16–178 / 16 | 44 / 638 (6.90%) | 1/5 |
| Staff | 29 | 50–1360 / 462 | 663 / 12937 (5.12%) | 2/5 |
| Sword | 44 | 24–584 / 243.5 | 232 / 10396 (2.23%) | 2/5 |
| Wings, generation 1 | 3 | 16–46 / 32 | 0 / 94 (0%) | 1/5 |
| Wings, generation 2 | 4 | 102–872 / 390 | 24 / 1754 (1.37%) | 2/5 |
| Wings, generation 3 | 4 | 274–1842 / 995 | 53 / 4053 (1.31%) | 3/5 |
| Other wings, robes and capes | 11 | 16–874 / 192 | 4 / 2700 (0.15%) | 2/5 |
| Armor parts | 97 | 120–1414 / 352 | 613 / 38385 (1.60%) | 3/5 |
| Boots | 94 | 106–676 / 190 | 795 / 19993 (3.98%) | 2/5 |
| Gloves | 85 | 130–492 / 180 | 617 / 17405 (3.54%) | 3/5 |
| Helmets | 92 | 38–638 / 197 | 488 / 21983 (2.22%) | 3/5 |
| Pants | 95 | 84–636 / 142 | 476 / 19494 (2.44%) | 2/5 |

### Quality assessment

The 1–5 scale describes the visual rework opportunity: 1 means the largest gaps; 3 means a mixed but usable baseline; 5 would indicate little rework need. The ratings combine model counts, triangle and texture dimensions with inspection of the common-camera previews. They are art-direction judgments, not an engine-validity grade.

- **Axes, bows, maces, shields, spears, staffs and swords:** usually 2/5. Many early examples sit below 120 triangles and use 16–64 px maps. The previews show thin or blocky construction, so stronger large-scale silhouettes and painted material separation are the main wins.
- **Spellbooks:** 1/5. The family median is 16 triangles; 19 models share one 64×64 cover texture, so a coordinated mesh and texture treatment can improve many variants.
- **Generation 1 wings:** 1/5. Wing01 is 16 triangles and appears flat and sparse in its preview. Generation 2 is 2/5: more articulated, but angular and often mapped at 64–128 px. Generation 3 is 3/5: substantially more geometry and 128–256 px maps, though Wing08 exceeds the proposed 1500-triangle item target and its dark offline-render regions need an in-client alpha/render check.
- **Other wings, robes and capes:** 2/5. The group is mixed, with low-detail silhouettes and preserved alpha/scroll behavior to account for.
- **Armor:** body armor, gloves and helmets are 3/5 because the sampled sets include readable, more ornate pieces; boots and pants are 2/5 because many maps are small and their folds/seams are hard to read at game scale. Scores apply per part family.

The UV column reports the modeled share of triangles with anisotropy above 8:1, the objective signal behind the family-specific stretch notes. It is a review flag, not automatic evidence of visible stretching. Near-degenerate triangles can create outliers, so inspect UV islands and in-game appearance before deciding to change them. Every family score in the JSON includes the same measured screen and interpretation.

## Before previews

All 13 PNG previews use the repository Blender BMD import script, no animation, one neutral studio lighting setup, orthographic camera at `(260, -420, 210)`, target `(0, 0, 0)`, 360-unit camera scale, 1024×1024 output, and model scale 1.0. Each preview is recentered around its geometry bounds for presentation; that display translation is recorded in `previews/metadata.json`. The preview therefore does not validate the source pivot or equipped attachment.

| Preview | Representative source |
|---|---|
| [Sword](previews/sword.png) | `Item/Sword01.bmd` |
| [Axe](previews/axe.png) | `Item/Axe01.bmd` |
| [Mace](previews/mace.png) | `Item/Mace01.bmd` |
| [Spear](previews/spear.png) | `Item/Spear01.bmd` |
| [Bow](previews/bow.png) | `Item/Bow01.bmd` |
| [Staff](previews/staff.png) | `Item/Staff01.bmd` |
| [Shield](previews/shield.png) | `Item/Shield01.bmd` |
| [Wings, generation 1](previews/wing-gen1.png) | `Item/Wing01.bmd` |
| [Wings, generation 2](previews/wing-gen2.png) | `Item/Wing04.bmd` |
| [Wings, generation 3](previews/wing-gen3.png) | `Item/Wing08.bmd` |
| [Male armor set 01](previews/armour-set-male-01.png) | `Player/{Helm,Armor,Pant,Glove,Boot}Male01.bmd` |
| [Male test set 20](previews/armour-set-male-20.png) | `Player/{Helm,Armor,Pant,Glove,Boot}MaleTest20.bmd` |
| [LuckyItem set 62](previews/armour-set-lucky-62.png) | `Player/LuckyItem/62/new_{helm,armor,pant,glove,boot}01.bmd` |

## Twenty rework targets

Ranks indicate expected visual lift from silhouette weakness, small texture allocation, UV review flags, or shared-family leverage. This is a model-level shortlist, not an item-ID catalog. The spellbook row is one coordinated shared-texture family target; the two armor rows are five-part set targets. Each candidate's measured triangles, texture sizes and source-model list are in `baseline.json`.

| Rank | Target | Reason |
|---:|---|---|
| 1 | `Item/Mace02.bmd` | 118 triangles; 16×16 texture leaves little room for head shape and material separation. |
| 2 | `Item/Shield01.bmd` | 18 triangles and 32×32 texture; flat starter silhouette. |
| 3 | `Item/Wing01.bmd` | 16 triangles; weakest and sparsest wing-generation silhouette. |
| 4 | `Item/Bow01.bmd` | 58 triangles; two 32×32 maps constrain the limbs, string and grip. |
| 5 | `Item/Axe01.bmd` | 48 triangles and 32×32 map; head thickness and edge-to-haft separation. |
| 6 | `Item/Spear03.bmd` | 46 triangles and 32×32 map; tip, socket and grip transitions. |
| 7 | `Item/Staff02.bmd` | 50 triangles; 32×32 and 16×16 maps constrain the head/orb. |
| 8 | `Item/Mace01.bmd` | 36 triangles and 32×32 map; compact head has little modeled volume. |
| 9 | `Item/Sword03.bmd` | 49 triangles and 32×32 map; blade, guard and handle read as a thin form. |
| 10 | `Item/Book01.bmd`–`Book19.bmd` | 16 triangles each; one 64×64 `book.OZJ` is shared by all 19 models. |
| 11 | `Item/Shield02.bmd` | 26 triangles and 32×32 map; clarify rim, boss and handle-side thickness. |
| 12 | `Item/Sword04.bmd` | 72 triangles; 64×16 main map and 2×2 secondary map need mesh/flag review. |
| 13 | `Item/Spear04.bmd` | 60 triangles and 32×32 map; define blade and shaft transitions. |
| 14 | `Item/Bow02.bmd` | 76 triangles; main map is 64×64, with two 32×32 support maps. |
| 15 | `Item/Axe02.bmd` | 60 triangles and 32×32 map; improve the edge, cheek and haft at fixed origin. |
| 16 | `Item/Staff03.bmd` | 68 triangles and 64×64 map; inspect UV-screen outliers while improving the head silhouette. |
| 17 | `Item/Sword11.bmd` | 66 triangles and two narrow 32×16 maps. |
| 18 | `Item/Wing07.bmd` | 268 triangles and 64×32 map; the wide surface needs better texel allocation. |
| 19 | `Player/*Male01.bmd` (five parts) | Five-part set totals 706 triangles; rework together and preserve skeleton bindings. |
| 20 | `Player/*Elf01.bmd` (five parts) | Five-part set totals 616 triangles; improve class identity across all five parts. |

## Proposed item style guide

This is a future-art proposal; T1–T7 colors are visual guidance, not authoritative existing item metadata.

**Materials:** keep the shipped engine's single diffuse texture per mesh. Paint broad metal planes, edge highlights and contact shadows into the map. Use restrained warm browns and directional grain for leather and wood; group cloth folds or feathers into readable forms; keep saturated gems or magic accents small. No PBR maps, normal maps, or complex layered materials are required or assumed.

**Palette by tier:**

| Tier | Role | Suggested colors |
|---|---|---|
| T1 | Common / grounded | iron `#858B8D`, charcoal `#343B42`, walnut `#594132`, muted brass `#8A795B` |
| T2 | Forged / veteran | iron `#A5A39A`, slate `#39434A`, leather `#4D382B`, bronze `#97704B` |
| T3 | Elemental / noble | silver `#BEC8C9`, deep blue `#263D53`, leather `#493B39`, teal `#318C9A` |
| T4 | Rare / arcane | silver `#CFD0D5`, indigo `#35345A`, plum `#43304A`, violet `#7D62B5` |
| T5 | Heroic / gilded | ivory gold `#D7C08B`, black steel `#292E39`, leather `#473028`, crimson `#B74834` |
| T6 | Mythic / radiant | pale gold `#E2D6B4`, blue slate `#283A49`, deep violet `#332E47`, turquoise `#4DB5BA` |
| T7 | Legendary / signature | bright silver-gold `#E4D7B6`, charcoal `#242832`, plum `#3B2942`, magenta `#CB4FA6`, glow cyan `#65D8E8` |

**Budgets and compatibility:**

- Design ceiling: **1500 triangles per item BMD**. Use about 250–900 for sword/axe/mace/spear, 300–1000 for bow/staff/shield, and 500–1500 for wings. Each armor part should generally use 150–500 triangles; keep each BMD below 1500 and budget the complete five-part set together.
- Textures: power-of-two, one UV set, at most **1024×1024**. Use 128×128 or 256×256 for ordinary assets, 512×512 for visually important hero assets, and 1024×1024 only when screen size and UV allocation justify it.
- Preserve one texture per mesh, mesh count/order, texture-name suffixes `_R`, `_H`, `_S`, `_N`, item origin, orientation and scale. Armor must keep its player skeleton, bone order and every action. The asset-pipeline validator's 15000-triangle ceiling is a hard file-validity limit; the stricter 1500-triangle item limit in `ITEM_EDITOR_PLAN.md` section 2 is the art target.

## Risks to check before a rework

1. **Shared textures:** 131 files are shared by multiple models. `Player/hide.OZJ` (9×9) is referenced by 102 models; `Item/book.OZJ` (64×64) by 19; `Player/hide_m.OZJ` (2×2) by 19; `Item/bow01.OZJ` (32×32) by 9. Review every consumer in `baseline.json` before repainting.
2. **Attachment origins:** weapons, shields and wings can attach via fixed model origins and hard-coded angles. Since the previews recenter geometry, they cannot validate pivots. Preserve original origin/orientation/scale and check inventory plus equipped placement in the client.
3. **Mesh order and flags:** the engine hides or blends mesh indices; texture suffixes control bright/hidden/scroll/no-blend behavior. Exports may merge meshes sharing a texture name. Preserve mesh count/order and per-mesh names/flags and compare `bmdconv info` before and after.
4. **Armor skeleton/actions:** keep the player skeleton, bone order and all actions for every armor-part rework.
5. **Offline alpha/render differences:** the Wing Gen3 preview has dark regions. Check source alpha and render flags, then verify behavior in the client.

## Reproduction

From the repository root, use the repo-built `bmdconv` and Blender installed with Blender Source Tools:

```sh
BMD_CONV=out/build/macos-arm64/tools/bmdconv/Release/bmdconv
BLENDER=/Applications/Blender.app/Contents/MacOS/Blender
STUDY=assets-work/Items/study

python3 "$STUDY/collect_baseline.py" --bmdconv "$BMD_CONV" --jobs 12
python3 "$STUDY/analyze_uv_stretch.py" --bmdconv "$BMD_CONV" --jobs 12
python3 "$STUDY/finalize_baseline.py"
python3 "$STUDY/prepare_previews.py" --bmdconv "$BMD_CONV" --imports-dir /tmp/mumain-item-study/imports
"$BLENDER" -b --python "$STUDY/render_previews.py" -- \
  --imports-dir /tmp/mumain-item-study/imports --output-dir "$STUDY/previews"
```

`--no-anims` imports skip action-manifest parsing in `tools/blender/mu_bmd_import.py`; this was needed because Wing08's legacy manifest is not UTF-8 and no action data is needed for these static previews. The edit is outside `src/`.
