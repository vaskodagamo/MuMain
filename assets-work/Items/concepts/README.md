# Item concepts

Concept images for new item designs, generated with the OpenAI Images API. The owner picks a
design, the pick is committed here, and it becomes the reference image of an item request
([`../requests/README.md`](../requests/README.md)) that Codex then models in Blender.

The tool is [`tools/item_editor/concepts.py`](../../../tools/item_editor/concepts.py) (Python
standard library, macOS `sips`, Blender for the reference renders). Run it from the repository root.

```
assets-work/Items/concepts/
  README.md            this file
  <key>/concept.jpg    the picked concept (JPEG, quality 90, longest side 1024 px)
  <key>/prompt.txt     the prompt that made it
  <key>/meta.json      model, size, quality, request id, usage, cost, batch and variant it came from
out/item-concepts/     (git-ignored, never committed)
  refs/<key>.png       reference renders of the current look (+ <key>.json cache record)
  <batch>/             one run: batch.json, run.log, sheet.html, <key>/ref.png, <key>/v1.png ...
```

`<key>` is the catalog key (`0-2` Rapier). An armour set is one concept for all five parts, filed
under its body armour's key (`8-1` for the Dragon set); the spell books that share one texture
(`15-0` and 20 more) are one concept too.

## Workflow

1. **API key, once.** Create a key on the OpenAI platform and put it into your own shell profile;
   never into the repository, a file the tool reads, or a chat:

   ```bash
   echo 'export OPENAI_API_KEY="..."' >> ~/.zshrc   # then open a new terminal
   ```

   The tool reads the key only from `$OPENAI_API_KEY`, only in `run --yes`, and never prints or
   writes it (the run log shows the header as `Bearer [redacted]`). OpenAI may ask the
   organisation to complete **API Organization Verification** before the image models work.

2. **Plan** (dry run, the default command; nothing is sent):

   ```bash
   python3 tools/item_editor/concepts.py plan --study-top 10
   python3 tools/item_editor/concepts.py plan --keys 0-19,6-1 --note "0-19=less gold"
   python3 tools/item_editor/concepts.py plan --family swords --tier T1-T3 --no-prompts
   ```

   It prints per item the prompt, the reference image, the variants and the cost split
   (text / reference input / output) per request and for the batch, and whether the batch is
   within the caps. `--study-top N` takes the art study's ranked rework list
   ([`../study/README.md`](../study/README.md)).

3. **References** (the item's current look, rendered offline with the study's Blender pipeline;
   cached per model sha256, a second call is instant):

   ```bash
   python3 tools/item_editor/concepts.py refs --study-top 10
   ```

   Needs Blender (`--blender`, `$MU_BLENDER`, default `/Applications/Blender.app`) and `bmdconv`
   (`--bmdconv`, `$MU_BMDCONV`, else `out/build/*/tools/bmdconv/*/bmdconv` here or in the
   `MuMain` checkout next to this one). Look at `out/item-concepts/refs/*.png` before spending:
   the image model redesigns what it sees.

4. **Run** (spends money; asks for `--yes`):

   ```bash
   python3 tools/item_editor/concepts.py run --study-top 10          # prints the estimate only
   python3 tools/item_editor/concepts.py run --study-top 10 --yes    # sends it
   ```

   The run writes `out/item-concepts/<YYYYMMDD-HHMMSS>-<slug>/`, prints the actual cost from the
   API's `usage` next to the estimate, and writes the contact sheet. Failed requests are listed;
   `run --resume <batch> --yes` retries only the missing images.

5. **Choose**: open `out/item-concepts/<batch>/sheet.html` (one row per item: current look, v1,
   v2, v3; the prompt under *prompt*; the pick command under each image).

6. **Pick** (and commit the folder it writes):

   ```bash
   python3 tools/item_editor/concepts.py pick <batch> 0-2 v2
   python3 tools/item_editor/concepts.py unpick 0-2        # changed your mind
   ```

7. **Request**: file the item request as usual and copy `concepts/<key>/concept.jpg` into the
   request as `captures/ref-concept.jpg` (listed in `change.reference_images`). Codex models the
   concept within the request's `must_keep` rules (origin, grip, size in the slot, mesh order).

`python3 tools/item_editor/concepts.py plan --batch <batch>` shows a finished batch's estimated
and actual cost per request again.

## Settings

| Option | Default | Meaning |
|--------|---------|---------|
| `--preset explore` | yes | `gpt-image-2.5-flare`, quality `medium`, 3 images per item in one request (`n=3`), reference 512 px |
| `--preset final` | | `gpt-image-2.5-sunburst`, quality `high`, 1 image, reference 1024 px |
| `--model` | preset | also `gpt-image-2`, `gpt-image-1.5`, `gpt-image-1`, `gpt-image-1-mini` |
| `--quality` | preset | `low`, `medium`, `high`; the 2.5 models also `xhigh`, `max` (`auto` is not offered: it cannot be estimated) |
| `--variants N` | preset | images per item |
| `--hints same` / `distinct` | `same` | `same`: one request with `n=N`, the reference is uploaded and billed once. `distinct`: one request per variant with its own hint (v1 faithful refresh, v2 stronger silhouette, v3 more ornate) |
| `--ref-size PX` | preset | longest side of the uploaded reference (downscaled with `sips`) |
| `--sheet` | off | front + side turnaround on one canvas, `1536x1024`, opaque background |
| `--size`, `--background` | `1024x1024`, `transparent` | explicit overrides |
| `--note "text"` / `--note KEY="text"` | | appended to every prompt / to one item's prompt |
| `--max-images`, `--max-cost` | 30, $5 | hard caps: `run` refuses (exit 3) when the estimate exceeds either |
| `--concurrency` | 2 | parallel requests |

Explicit flags override the preset. The presets and prices live in
[`tools/item_editor/image_prices.json`](../../../tools/item_editor/image_prices.json); the prompt
text in [`tools/item_editor/concept_prompt.md`](../../../tools/item_editor/concept_prompt.md)
(edit it freely: sections per family and view, the variant hints, `$placeholders` for the item's
name, tier, the study's T1-T7 palette and material rules).

## Costs

Prices of the owner's pricing page (2026-09-23), per image at 1024x1024 low / medium / high:
GPT Image 2 $0.006 / $0.053 / $0.211 (1024x1536: $0.005 / $0.041 / $0.165), GPT Image 1.5
$0.009 / $0.034 / $0.133, GPT Image 1 $0.011 / $0.042 / $0.167, GPT Image 1 Mini
$0.005 / $0.011 / $0.036. The 2.5 models have no per-image price: their estimate is output tokens
(1024x1024: low 196, medium 439, high 1756, xhigh 3122, max 7024) at $30 per million, and is
flagged. A request also pays for its text prompt ($5 per million tokens, $2 for mini) and the
reference image as input tokens ($8 per million for the 2 / 2.5 models).

**Measured on the first real run (2026-09-23, gpt-image-2.5-flare, medium, n=3, 512 px
reference, study top 10):** $0.706 for 30 images, $0.07 per item. Per request: the reference was
3072 input image tokens, billed **once** for all three variants; the prompt was about 1300 text
tokens (about 1.5 characters per token); the output was 1317 tokens = 3 x 439, exactly as
estimated. `image_prices.json` now uses these numbers; a 1024 px reference is still unmeasured
and estimated by area from the 512 px one (an over-estimate). Uploading the reference at 512 px
and asking for all variants in one request keeps the reference, which costs more than an image
at medium, to one charge per item. The Batch API discount does not apply to the 2.5 models.

Speed depends on the organisation's usage tier: at Tier 1 image models allow about 5 images per
minute. The tool retries 429, 5xx and timeouts with exponential backoff and jitter and follows the
API's `Retry-After`; a quota error (`insufficient_quota`) stops at once.

## API

`POST https://api.openai.com/v1/images/edits`, `multipart/form-data`: `model`, `prompt`, `size`,
`quality`, `background`, `output_format=png`, `n` (1-10), and the reference as `image[]` (PNG).
`input_fidelity` is not sent (not documented for the 2.5 models, ignored by `gpt-image-2`; a model
entry in `image_prices.json` can set it for the gpt-image-1 family). The response's
`data[i].b64_json` images, `usage` and `x-request-id` header are stored per variant. Sources
(checked 2026-09-23):

- <https://developers.openai.com/api/reference/python/resources/images/methods/edit> (parameters)
- <https://developers.openai.com/api/reference/resources/images/methods/edit>
- <https://developers.openai.com/api/docs/guides/image-generation> (`image[]` curl example, sizes, qualities, transparency)
- <https://developers.openai.com/api/docs/models/gpt-image-2>
- <https://developers.openai.com/api/docs/guides/images-vision> (input image token rule)

## What to commit

Commit only `assets-work/Items/concepts/<key>/` folders written by `pick` (a JPEG of at most
1024 px, the prompt and `meta.json`). Never commit `out/item-concepts/` (batches, references,
logs; `out/` is git-ignored), and never a file containing the API key. `meta.json` holds no key.

## Limits of the reference renders

The references use the art study's offline pipeline: one fixed three-quarter camera direction,
the study's lights, no animation. Alpha textures render opaque (Wings of Elf shows dark quads
around the feathers), and armour parts stand in their bind pose, so gloves and boots can sit
slightly off the body. The prompt asks the model to keep the silhouette and proportions, not these
render artefacts; check the reference before a run.
