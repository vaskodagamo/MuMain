# Item concept prompt

The prompt `concepts.py` sends for each item. Edit the text freely; the tool reads this file on
every run. Everything above the first `## ` heading (this introduction) is ignored.

A prompt is built from these sections, in this order, separated by blank lines:

1. `## base`
2. `## family <family>` for the item's family, else `## family default`
3. `## view single`, or `## view sheet` with `--sheet`
4. `## variant same` (all images of the item come from one request, the default), or
   `## variant 1`, `## variant 2`, ... with `--hints distinct` (one request per variant)
5. `## note`, only when the owner passed `--note`

A refine (`run --from <batch>/<key>/<variant> --note "comment"`, a new round from one earlier
concept) uses `## refine` (with the comment as `$note`), then the family section and the view
section; no `## base`, no variant hint. It attaches two images: first the concept to revise, then
the item's current in-game model.

Placeholders (`$name`): `$name` item name, `$keys` catalog keys, `$family` family label,
`$tier` 1..7, `$tier_role` (e.g. "Heroic / gilded"), `$palette` the tier's colours,
`$materials` the study's material rules, `$note` the owner's note. Write `$$` for a literal `$`.
Tier colours and material rules come from the art study (`assets-work/Items/study/baseline.json`,
`style_guide`). Family names are the catalog families; the tool maps `wings-1`..`wings-mini` to
`wings`; a whole armour set (any part of a set selects the set) uses `armour-set`.

## base

Game-ready concept art for one item of a fantasy MMORPG with low-poly models (at most 1500 triangles, one hand-painted diffuse texture): "$name", a $family, tier $tier of 7 ($tier_role).
The attached image is the item's current in-game model. Redesign it as a better version of the same item: keep its silhouette, proportions, length-to-width ratio, grip or attachment point and orientation, so it still fits the same model origin, hand position and inventory slot.
Style: hand-painted MU Online fantasy, large readable forms, one dominant silhouette cue, restrained highlights, no noisy micro-detail; the design must read clearly at inventory-icon size.
Tier palette ($tier_role): $palette.
Materials: $materials
No text, letters, logos, watermarks, signatures, frames, hands, characters or scenery; show only the item.

## family swords

Sword: clear blade, guard, grip and pommel; a strong edge highlight along the blade; the grip length stays one hand (or two hands if the reference has a long grip).

## family axes

Axe: a thick, readable head with a clear edge, cheek and haft separation; the haft length and grip position stay as in the reference.

## family maces

Mace or blunt weapon: a heavy, volumetric head with distinct flanges or studs, a clear haft and grip.

## family spears

Spear or polearm: a readable tip, socket and shaft transitions; the shaft length and grip position stay as in the reference.

## family bows

Bow or crossbow: clear limbs, grip and string; keep the curvature and span of the reference.

## family staffs

Staff or stick: a readable head or orb as the silhouette cue, a clear shaft and grip; magic accents small and bright.

## family shields

Shield: a clear rim, face and boss; show some thickness at the rim; keep the outline and the handle side of the reference.

## family wings

Wings worn on the back: large readable feather or membrane groups, a symmetric pair, the same span and attachment at the centre as the reference; alpha-friendly edges.

## family armour-set

Armour set of five parts worn together (helm, armour, pants, gloves, boots), shown on the neutral body pose of the reference: the parts read as one set with shared trim, colours and motifs; keep the proportions and the body pose.

## family skill-books

Spell book or scroll: a closed book with a readable cover design, clasps and spine; the same design family works for many books that differ only in colour and emblem.

## family default

Keep the item's type immediately recognisable from its outline.

## view single

View: a single orthographic three-quarter view matching the reference camera angle, the whole item in frame and centred, generous margin, soft even studio lighting, no cast shadow, on a transparent or plain neutral grey background.

## view sheet

View: a turnaround sheet on one landscape canvas: the front view on the left and the side view on the right, orthographic, same scale, aligned on one baseline, the whole item in frame in both, soft even studio lighting, on a plain neutral light grey background.

## variant same

Variation: a faithful refresh that stays close to the reference's design ideas, but cleaner, more readable and better crafted; each image may explore different details within these rules.

## variant 1

Variation: a faithful refresh - the same design ideas as the reference, cleaner and better crafted, with clear material separation.

## variant 2

Variation: a stronger silhouette - bolder, more distinctive outline and larger primary shapes, still the same item type, size and grip.

## variant 3

Variation: more ornate within the tier - richer trim, engraving and accents that fit tier $tier, without exceeding the tier's palette or adding noisy detail.

## note

Owner's note: $note

## refine

A revision round of game-ready concept art for one item of a fantasy MMORPG with low-poly models (at most 1500 triangles, one hand-painted diffuse texture): "$name", a $family, tier $tier of 7 ($tier_role).
The first attached image is the concept to revise. The second attached image is the item's current in-game model; use it only for the proportions.
Revise the attached concept: $note
Keep everything else as in the first image: the same design, colours and materials, the same view and camera angle, the same background, lighting and hand-painted style. Keep the proportions constraints of the item: silhouette, length-to-width ratio, grip or attachment point and orientation stay as in the in-game model, so it still fits the same model origin, hand position and inventory slot.
Tier palette ($tier_role): $palette.
No text, letters, logos, watermarks, signatures, frames, hands, characters or scenery; show only the item.
