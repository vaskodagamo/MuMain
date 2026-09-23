#!/usr/bin/env python3
"""Add the reviewed art assessment and style proposal to baseline.json."""
from __future__ import annotations

import json
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BASELINE = ROOT / "baseline.json"

ITEM_ASSESSMENTS = {
    "axe": (2, "9 models; 38–112 triangles (60 median), mostly 32–64 px maps. Early axes have thin, blocky heads and little grip separation. UV screen flags exist; inspect the individual maps before changing them."),
    "bow": (2, "24 bow/crossbow models; 58–783 triangles (217 median), with 32 px shared support maps common in the early bows. The first bow has a narrow, angular outline and low-resolution texture. Some UV islands screen above 8:1 anisotropy."),
    "bow_ammunition": (2, "2 arrow models; 42–75 triangles and sub-128 px maps. Their silhouettes are readable but very simple; tiny shafts and fletching make texture stretch worth checking at game scale."),
    "mace": (2, "22 models; 36–1194 triangles (395 median). Early examples include Mace02 at 118 triangles with a 16×16 map and Mace01 at 36 triangles with a 32×32 map. The heads lack rounded volume and clear material breaks."),
    "shield": (2, "22 models; 18–408 triangles (84 median), and low-resolution maps are common. Shield01 has 18 triangles and a 32×32 map; the first shield reads as a thin, flat plate. UV outliers should be reviewed in context."),
    "spear": (2, "11 models; 46–162 triangles (112 median), with 32×16 to 128×32 maps represented. Early profiles are narrow and spare; edge bevels, socket transitions and material breaks need painted or geometric emphasis."),
    "spellbook": (1, "22 models; 16 triangles median and all referenced textures have a side below 128 px. Nineteen models share the same 64×64 book.OZJ; geometry and the shared cover art offer unusually high reuse leverage. UV anisotropy screening is elevated for this family."),
    "staff": (2, "29 models; 50–1360 triangles (462 median), with early 16–64 px maps and several tiny glow/support maps. Staff02 is 50 triangles with 32×32 and 16×16 maps. Orbs and head pieces need stronger silhouettes and value separation."),
    "sword": (2, "44 models; 24–584 triangles (243.5 median). Many early maps have a 16–64 px short side; Sword03 is 49 triangles with a 32×32 map. Blade profiles, guard thickness and grip/pommel readability are the main silhouette opportunities."),
    "wing_gen1": (1, "3 models; 16–46 triangles (32 median). Wing01 is only 16 triangles despite a 64×64 map; the preview confirms a flat, sparse silhouette. Gen1 wings need the most substantial shape and texture rework."),
    "wing_gen2": (2, "4 models; 102–872 triangles (390 median), mostly 64–128 px maps. Articulated outlines improve over Gen1, but the representative remains angular and the 64×32 Wing07 map is undersized for its broad surface."),
    "wing_gen3": (3, "4 models; 274–1842 triangles (995 median), with 128–256 px maps on the main forms. Shape and detail are stronger than earlier generations, but Wing08 exceeds the 1500-triangle item target and the Blender preview shows dark regions that need an in-client alpha/render check."),
    "wing_other": (2, "11 cape, robe and shop-wing models; 16–874 triangles (192 median) and mixed 64–256 px maps. Quality is uneven; several robes and early store wings have flat folded silhouettes. Preserve their alpha and scrolling behavior when retouching."),
}
ARMOUR_ASSESSMENTS = {
    "armor": (3, "97 models; 120–1414 triangles (352 median), with both 128–256 px body maps and tiny hide/overlay maps. The sampled sets range from basic blocky plates to more ornate layered pieces."),
    "boot": (2, "94 models; 106–676 triangles (190 median); 89 of 106 texture references have a side below 128 px. Boot UV screening has the highest median model p95 anisotropy of the armor parts; inspect flagged triangles for degeneracy before editing."),
    "glove": (3, "85 models; 130–492 triangles (180 median); most maps are 64–128 px. The sampled set reads at a distance, but glove seams and raised knuckle forms rely on small painted details."),
    "helm": (3, "92 models; 38–638 triangles (197 median), with many 32–64 px face/trim maps alongside larger skin maps. Some silhouettes are distinctive, while low-triangle helmets and shared hair/skin maps limit finish."),
    "pant": (2, "95 models; 84–636 triangles (142 median); many maps are 32–128 px and some 2×2 hide maps are engine placeholders. Cloth folds and armor/cloth boundaries often need stronger shape or paint definition."),
}

CANDIDATES = [
    ("Item/Mace02.bmd", "Single model", "118 triangles and a 16×16 diffuse map leave little room for a readable head, haft and material separation."),
    ("Item/Shield01.bmd", "Single model", "18 triangles with a 32×32 map; the preview shows a very flat starter shield silhouette."),
    ("Item/Wing01.bmd", "Single model", "16 triangles with a 64×64 map; the Gen1 preview has the weakest silhouette and very little surface structure."),
    ("Item/Bow01.bmd", "Single model", "58 triangles and two 32×32 maps; the string, limbs and grip need stronger shape and contrast."),
    ("Item/Axe01.bmd", "Single model", "48 triangles and one 32×32 map; head thickness and blade-to-haft separation are limited."),
    ("Item/Spear03.bmd", "Single model", "46 triangles with a 32×32 map; the long, narrow form benefits from stronger tip, socket and grip definition."),
    ("Item/Staff02.bmd", "Single model", "50 triangles with 32×32 and 16×16 maps; the head/orb treatment is constrained by low-resolution maps."),
    ("Item/Mace01.bmd", "Single model", "36 triangles and a 32×32 map; the compact head and shaft have little modeled volume."),
    ("Item/Sword03.bmd", "Single model", "49 triangles with a 32×32 map; blade profile, guard and handle read as one thin, low-detail form."),
    ("Item/Book01.bmd", "Shared 19-model spellbook group", "Book01–Book19 each use 16-triangle geometry and share Item/book.OZJ (64×64 across 19 models), so one coordinated redesign improves a whole family."),
    ("Item/Shield02.bmd", "Single model", "26 triangles and a 32×32 map; reinforce rim, boss and handle-side thickness without changing attachment placement."),
    ("Item/Sword04.bmd", "Single model", "72 triangles; its main map is 64×16 and the secondary 2×2 texture is a placeholder-sized reference. Check mesh flags before consolidating."),
    ("Item/Spear04.bmd", "Single model", "60 triangles with a 32×32 map; low geometry and a small map constrain its blade and shaft transitions."),
    ("Item/Bow02.bmd", "Single model", "76 triangles; the main map is 64×64 and its two shared support maps are 32×32. The limbs, grip and string need clearer construction."),
    ("Item/Axe02.bmd", "Single model", "60 triangles and one 32×32 map; enlarge the read of the edge, cheek and haft while retaining the current origin."),
    ("Item/Staff03.bmd", "Single model", "68 triangles with a 64×64 map; a small UV anisotropy screen outlier accompanies a basic silhouette, so inspect mapping before rebuilding."),
    ("Item/Sword11.bmd", "Single model", "66 triangles with two 32×16 maps; the low short side and narrow texture allocation make blade highlights and grip detail difficult."),
    ("Item/Wing07.bmd", "Single model", "268 triangles with a 64×32 map; broad wing surfaces deserve better texel allocation and a clearer feather/energy silhouette."),
    ("Player/*Male01.bmd", "Five-part male armor set", "The sampled five parts total 706 triangles. Individual armor/boot/glove/helm/pant maps are mostly 32–128 px; rework as a coordinated set and preserve player skeleton/action bindings."),
    ("Player/*Elf01.bmd", "Five-part elf armor set", "The sampled five parts total 616 triangles, with mostly 64–128 px garment maps. Improve class silhouette and material separation across all five parts as one set."),
]

TIER_PALETTE = [
    {"tier": "T1", "role": "Common / grounded", "colors": {"base": "#343B42", "metal": "#858B8D", "leather": "#594132", "accent": "#8A795B"}},
    {"tier": "T2", "role": "Forged / veteran", "colors": {"base": "#39434A", "metal": "#A5A39A", "leather": "#4D382B", "accent": "#97704B"}},
    {"tier": "T3", "role": "Elemental / noble", "colors": {"base": "#263D53", "metal": "#BEC8C9", "leather": "#493B39", "accent": "#318C9A"}},
    {"tier": "T4", "role": "Rare / arcane", "colors": {"base": "#35345A", "metal": "#CFD0D5", "leather": "#43304A", "accent": "#7D62B5"}},
    {"tier": "T5", "role": "Heroic / gilded", "colors": {"base": "#292E39", "metal": "#D7C08B", "leather": "#473028", "accent": "#B74834"}},
    {"tier": "T6", "role": "Mythic / radiant", "colors": {"base": "#283A49", "metal": "#E2D6B4", "leather": "#332E47", "accent": "#4DB5BA"}},
    {"tier": "T7", "role": "Legendary / signature", "colors": {"base": "#242832", "metal": "#E4D7B6", "leather": "#3B2942", "accent": "#CB4FA6", "glow": "#65D8E8"}},
]


def uv_aggregate(rows: list[dict]) -> dict:
    analyzed = sum(row.get("uv_stretch", {}).get("triangles_analyzed", 0) for row in rows)
    over8 = sum(row.get("uv_stretch", {}).get("triangles_ratio_over_8", 0) for row in rows)
    p95s = [row["uv_stretch"]["anisotropy_ratio_p95"] for row in rows if row.get("uv_stretch", {}).get("anisotropy_ratio_p95") is not None]
    return {
        "triangles_analyzed": analyzed,
        "triangles_ratio_over_8": over8,
        "ratio_over_8_percent": round(100 * over8 / analyzed, 2) if analyzed else None,
        "median_model_p95_anisotropy_ratio": round(statistics.median(p95s), 3) if p95s else None,
    }


def candidate_record(data: dict, target: str, grouping: str, reason: str, rank: int) -> dict:
    lookup = {row["path"]: row for row in data["items"] + data["armour_models"]}
    if target == "Player/*Male01.bmd":
        paths = [f"Player/{part}Male01.bmd" for part in ("Helm", "Armor", "Pant", "Glove", "Boot")]
    elif target == "Player/*Elf01.bmd":
        paths = [f"Player/{part}Elf01.bmd" for part in ("Helm", "Armor", "Pant", "Glove", "Boot")]
    elif target == "Item/Book01.bmd":
        paths = [f"Item/Book{i:02}.bmd" for i in range(1, 20)]
    else:
        paths = [target]
    rows = [lookup[p] for p in paths if p in lookup]
    return {
        "rank": rank,
        "target": target,
        "grouping": grouping,
        "models": paths,
        "model_count": len(rows),
        "triangles_total": sum(row["triangles"] for row in rows),
        "triangles_range": [min((row["triangles"] for row in rows), default=0), max((row["triangles"] for row in rows), default=0)],
        "textures": sorted({f"{texture['path']} ({texture['size_px'][0]}×{texture['size_px'][1]})" for row in rows for texture in row["textures"]}),
        "reason": reason,
    }


def main() -> None:
    data = json.loads(BASELINE.read_text())
    data["assessment_method"] = {
        "quality_scale": "1 = largest quality gap, 3 = mixed/usable baseline, 5 = little rework indicated; scores are an art-direction assessment, not an engine-validity grade.",
        "uv_screen": "Per-triangle UV anisotropy from bmdconv BMD-to-SMD geometry/UV data. Ratios above 8:1 are review flags only; near-degenerate geometry can produce outliers and every flag requires visual inspection.",
        "candidate_order": "Ranked by expected visual lift from baseline: weak silhouette/low triangle count, small texture dimensions, and shared-family reuse. Armor rows are coordinated five-part set targets. This is a model-level shortlist, not an item-ID catalog.",
        "texture_sharing": "Shared texture records are derived from model texture references and same-folder resolution; case/extension alternatives are recorded on each texture entry.",
    }
    data["quality_scores"] = []
    for source, definitions in (("item_family_summary", ITEM_ASSESSMENTS), ("armour_part_summary", ARMOUR_ASSESSMENTS)):
        rows_by_family = {row["family"]: row for row in (data["items"] if source == "item_family_summary" else data["armour_models"])}
        for summary in data[source]:
            family = summary["family"]
            score, reason = definitions[family]
            rows = [row for row in (data["items"] if source == "item_family_summary" else data["armour_models"]) if row["family"] == family]
            uv_metrics = uv_aggregate(rows)
            uv_count = uv_metrics["triangles_ratio_over_8"]
            uv_total = uv_metrics["triangles_analyzed"]
            uv_percent = uv_metrics["ratio_over_8_percent"]
            reason += (
                f" UV screen: {uv_count}/{uv_total} analyzed triangles ({uv_percent}%) "
                "exceed 8:1 anisotropy; this flags inspection, not confirmed visible stretching."
            )
            data["quality_scores"].append({
                "family": family,
                "scope": "item" if source == "item_family_summary" else "armour_part",
                "score_1_to_5": score,
                "model_count": summary["model_count"],
                "triangles": {"range": summary["triangle_range"], "median": summary["median_triangles"]},
                "texture_reference_count_max_side_le_128px": summary["textures_max_side_le_128px"],
                "uv_screen": uv_metrics,
                "reason": reason,
            })
    data["rework_candidates"] = [candidate_record(data, target, grouping, reason, rank) for rank, (target, grouping, reason) in enumerate(CANDIDATES, 1)]
    data["style_guide"] = {
        "intent": "A coherent, hand-painted MU fantasy style: large readable forms, deliberate silhouette, restrained highlights and class identity. This is a proposal for future item-art requests, not a change to existing asset behavior.",
        "materials": [
            {"material": "steel / iron / bronze", "treatment": "Single diffuse map; broad mid-tone planes, painted bevel highlights and contact shadows; separate edge, face and rivet values without PBR maps."},
            {"material": "leather / wood", "treatment": "Warm restrained browns, directional grain painted at readable scale, dark seam and grip transitions."},
            {"material": "cloth / feathers", "treatment": "Distinct broad folds or feather groups, clear value grouping, alpha only where the source material already needs it."},
            {"material": "gems / magic", "treatment": "Small high-saturation accent, bright center and painted rim/glow; keep the base silhouette readable without bloom."},
        ],
        "palette_by_tier": TIER_PALETTE,
        "geometry_budget_triangles_per_bmd": {
            "hard_item_target_max": 1500,
            "weapons_sword_axe_mace_spear": "250–900 typical; spend toward the top only for a visibly better silhouette or construction.",
            "bow_staff_shield": "300–1000 typical; preserve readable string, grip, shaft, rim and attachment details.",
            "wings": "500–1500; reserve geometry for the outline and large feather/energy forms. Wing08's current 1842 is above the item-art target and should be optimized if reworked.",
            "armour_parts": "150–500 typical for each of the five part BMDs; keep every individual model at or below 1500 and budget the set together.",
            "engine_hard_ceiling": "The asset pipeline validator rejects more than 15000 triangles, but the stricter 1500 item-art limit from Item Editor plan section 2 is the design ceiling.",
        },
        "texture_budget": {
            "common": "256×256 diffuse for important surfaces; 128×128 where surface area/detail is modest.",
            "hero": "512×512 when the model's screen area and paint detail justify it.",
            "exception": "1024×1024 maximum only for a large/high-impact asset where UV allocation benefits; do not upscale as a substitute for clean UVs.",
            "format_and_layout": "Power-of-two dimensions, maximum 1024×1024; one texture per mesh, one UV set. Keep render suffixes _R/_H/_S/_N and mesh order. Use an atlas when a single mesh needs multiple painted material looks.",
        },
        "style_checks": ["readable at inventory and equipped scale", "one dominant silhouette cue per asset", "avoid noisy micro-detail", "paint material separation into the supported diffuse texture", "use geometry for primary shape, texture for small surface detail"],
    }
    data["risks"] = [
        {"risk": "Shared textures can change many models at once.", "evidence": "131 shared texture files in scope; Player/hide.OZJ is referenced by 102 models, Item/book.OZJ by 19, Player/hide_m.OZJ by 19 and Item/bow01.OZJ by 9.", "mitigation": "Use the per-texture model list in shared_textures before repainting; make shared changes only after checking every consumer. Prefer a new uniquely named map only when mesh/material behavior permits."},
        {"risk": "Attachment origin, orientation and scale control equipped placement.", "evidence": "Hands, back and shields attach through model origin; game-side angles are hard-coded. Preview renders recenter each model for display, so their visual placement does not validate the original pivot.", "mitigation": "Preserve the original pivot/orientation/scale, record raw bind-pose bounds, and compare equipped/inventory placement in the client before acceptance."},
        {"risk": "Mesh order and texture suffixes carry runtime behavior.", "evidence": "The engine hides or blends item meshes by index; _R, _H, _S and _N suffixes select render flags. Export can merge meshes that share a texture name.", "mitigation": "Preserve mesh count/order and per-mesh texture/flags; compare `bmdconv info` before/after and validate each exported BMD."},
        {"risk": "Armor has skeleton and action compatibility requirements.", "evidence": "Player armor component models bind to the player skeleton; imported armor data records bones/actions and their ordering.", "mitigation": "Keep the Player skeleton, bone order and every action for armor changes; use `bmdconv compare` against the source."},
        {"risk": "The offline Blender material/alpha preview can differ from the game renderer.", "evidence": "Wing Gen3's preview contains large dark regions; preview is offline and uses the repository BMD importer.", "mitigation": "Inspect source texture alpha and flags, then confirm questionable alpha/glow behavior in the client."},
    ]
    metadata = json.loads((ROOT / "previews/metadata.json").read_text())
    data["previews"] = {
        "render_count": len(metadata["previews"]),
        "camera": metadata["camera"],
        "object_scale": metadata["object_scale"],
        "display_recentered_per_preview": metadata["display_recentered_per_preview"],
        "lighting": metadata["lighting"],
        "renderer": "Blender Cycles, offline, source models imported with tools/blender/mu_bmd_import.py; no animation used.",
        "entries": metadata["previews"],
    }
    BASELINE.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
    print(f"Annotated {BASELINE}: {len(data['quality_scores'])} family scores, {len(data['rework_candidates'])} rework targets, {len(data['previews']['entries'])} previews")


if __name__ == "__main__":
    main()
