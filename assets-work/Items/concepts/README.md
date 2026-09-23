# Item concepts

Concept images for new item designs, generated with the OpenAI Images API. The owner picks a
design, the pick is committed here, and it becomes the reference image of an item request
([`../requests/README.md`](../requests/README.md)) that Codex then models in Blender.

The tool is [`tools/item_editor/concepts.py`](../../../tools/item_editor/concepts.py) (Python 3.9 or
newer, standard library only, macOS `sips`, Blender for the reference renders). It finds the
repository from its own location, so it runs from any working directory (`--repo-root` overrides);
the item editor drives it through the [editor protocol](#editor-protocol).

```
assets-work/Items/concepts/
  README.md            this file
  <key>/concept.jpg    the picked concept (JPEG, quality 90, longest side 1024 px)
  <key>/prompt.txt     the prompt that made it
  <key>/meta.json      model, size, quality, request id, usage, cost, batch and variant it came from
out/item-concepts/     (git-ignored, never committed)
  refs/<key>.png       reference renders of the current look (+ <key>.json cache record)
  <batch>/             one run: batch.json, run.log, sheet.html, <key>/ref.png, <key>/v1.png ...
                       (a refine also <key>/parent.png; discarded.json marks discarded variants)
```

`<key>` is the catalog key (`0-2` Rapier). An armour set is one concept for all five parts, filed
under its body armour's key (`8-1` for the Dragon set); the spell books that share one texture
(`15-0` and 20 more) are one concept too.

## Workflow

1. **API key, once.** Create a key on the OpenAI platform and store it in the macOS Keychain
   (never in the repository, a file the tool reads, or a chat):

   ```bash
   security add-generic-password -U -a "$USER" -s openai-api-key -w   # asks for the key
   ```

   The tool takes `$OPENAI_API_KEY` when it is set, else this Keychain item (service
   `openai-api-key`, another one with `MU_OPENAI_KEYCHAIN_SERVICE`). The Keychain matters for the
   item editor: an app started from Finder or the Dock does not read `~/.zshrc`, so an
   `export OPENAI_API_KEY=...` there does not reach it. The key is read only in `run --yes`, never
   printed or written (the run log shows the header as `Bearer [redacted]`), and never passed on
   a command line. OpenAI may ask the organisation to complete **API Organization Verification**
   before the image models work.

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
   `run --resume <batch> --yes` retries only the missing images. Ctrl-C (or SIGTERM) stops
   starting new requests, lets the ones on the wire finish and saves the batch for `--resume`;
   a second Ctrl-C quits at once.

5. **Choose**: open `out/item-concepts/<batch>/sheet.html` (one row per item: current look, v1,
   v2, v3; the prompt under *prompt*; the pick command under each image).

6. **Refine** (optional): a new round from one variant, with a comment. The variant goes to the
   model as the design to revise, the item's render as the second image for its proportions; the
   wording is the `## refine` section of `concept_prompt.md`. Settings default to the variant's
   batch (flags still override).

   ```bash
   python3 tools/item_editor/concepts.py run --from <batch>/0-2/v2 --note "thinner guard, darker grip" --yes
   python3 tools/item_editor/concepts.py run --from 0-2=<batch>/v2 --from 1-0=<batch>/v1 \
       --note 0-2="thinner guard" --note 1-0="longer haft" --yes
   ```

   Each new variant's `meta.json` records its `parent` and `lineage`, so a refine of a refine
   can be traced back to the first round.

7. **Pick** (and commit the folder it writes):

   ```bash
   python3 tools/item_editor/concepts.py list --key 0-2                 # every variant of the item
   python3 tools/item_editor/concepts.py pick <batch> 0-2 v2
   python3 tools/item_editor/concepts.py unpick 0-2        # changed your mind
   python3 tools/item_editor/concepts.py discard <batch> 0-2 v1         # hide a variant (undiscard reverts)
   ```

   `discard` only marks the variant in `<batch>/discarded.json`; no image is deleted.

8. **Request**: file the item request as usual and copy `concepts/<key>/concept.jpg` into the
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
| `--from B/K/V`, `--from K=B/V` | | refine that variant (with `--note`); repeatable, one per item |
| `--max-images`, `--max-cost` | 30, $5 | hard caps: `run` refuses (exit 3) when the estimate exceeds either |
| `--concurrency` | 2 | parallel requests |
| `--repo-root`, `--out-dir`, `--refs-dir`, `--concepts-dir` | from the script | where the checkout, batches, reference renders and picks are |

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

## Editor protocol

The item editor (milestone I5b) runs `concepts.py` as a child process and reads JSON from its
stdout. Every record carries `"protocol": 1`; within version 1 fields are only ever added.

**Starting it.** Run `<python3> <repo>/tools/item_editor/concepts.py <command> ... --repo-root <repo>`
as an argument list (no shell), stdin from `/dev/null`. The working directory does not matter; all
paths in the output are absolute. An app started from Finder gets `PATH=/usr/bin:/bin:/usr/sbin:/sbin`,
so a bare `python3` is Apple's `/usr/bin/python3` (3.9 with the Command Line Tools); the tool and
its tests run on 3.9 and newer, and TLS works there (Apple's Python trusts `/etc/ssl/cert.pem`; a
python.org Python without its certificates falls back to that file). No environment is required:
`OPENAI_API_KEY` is optional (the Keychain item is used otherwise), `MU_OPENAI_KEYCHAIN_SERVICE`
names another Keychain item, `MU_BLENDER` / `MU_BMDCONV` locate the tools for `refs`. The editor
never handles the key itself.

**Streams.** stdout carries only protocol records, UTF-8, one JSON object per line, each line
flushed when it happens; read it line by line. stderr carries human text (for a log view, not for
parsing). Argument errors from the parser (exit 2) print usage to stderr and nothing to stdout.

### One-shot commands: `--json`

One object on stdout when the command ends: `{"protocol": 1, "command": "<name>", "ok": true, ...}`,
or on an error `{"protocol": 1, "command": "<name>", "ok": false, "error": "<text>", "exit": <code>}`.
Costs are `{"parts": {"text", "reference", "output"}, "total"}` in USD; an estimate also has `images`
and `flags`.

| Command | Result fields |
|---------|---------------|
| `plan <selection or --from ...> --json` | `repo_root`, `out_dir`, `refs_dir`, `kind` (`generate` / `refine`), `settings`, `items[]`, `totals` (`images`, `requests`, `parts`, `total`, `flags`), `caps` (`max_images`, `max_cost`), `flags`, `api_key` (`found`, `source` `env`/`keychain`/null, `keychain_service`, `add_command` when missing; never the key), `would_refuse`, `reasons[]` (`code`: `max_images`, `max_cost`, `missing_reference` (+`keys`), `no_api_key`; `message`) |
| `plan --batch B --json` | `batch`, `dir`, `settings`, `estimated`, `actual` (null before any usage), `requests`, `requests_with_usage`, `per_request[]` (`key`, `request`, `estimated`, `actual`, `error`, `ok`) |
| `list --json` | `items[]` (`key`, `name`, `family`, `tier`, `keys`, `kind`, `variants`, `discarded`, `batches[]`, `picked`, `pick`), `batches[]` (`batch`, `dir`, `kind`, `created`, `items`, `images`, `running`), `warnings[]` |
| `list --key K --json` | `key` (the item's concept key: a set's body armour, a shared group's first key), `requested_key`, `item` (`name`, `family`, `tier`, `keys`, `kind`) or null, `variants[]`, `reference` (`path`, `exists`, `source` `refs`/`batch`), `pick` or null, `warnings[]` |
| `pick B K V --json` | `key`, `batch`, `variant`, `pick` |
| `unpick K --json` | `key`, `removed` |
| `discard B K V --json`, `undiscard ...` | `batch`, `key`, `variant`, `discarded`, `changed`, `image` |
| `sheet B --json` | `sheet` |

A plan item: `key`, `keys`, `name`, `family`, `tier`, `kind`, `study_rank`, `note`, `reference`
(`path`, `exists`), `parent` (`batch`, `key`, `variant`) and `parent_image` for a refine, `lineage[]`,
`requests[]` (`id`, `n`, `variants`, `hint`, `images` (attached files, in order), `estimate`,
`prompt` unless `--no-prompts`), `estimate`.

A variant (`list --key`): `batch`, `batch_dir`, `kind`, `key`, `variant`, `image`, `prompt` (text),
`meta` (path), `model`, `quality`, `size`, `created`, `note`, `request`, `cost` (`request_actual`,
`request_estimated`, `image_actual`, `image_estimated`), `parent`, `lineage[]` (nearest first),
`discarded`, `picked`. Only variants whose image exists are listed, oldest batch first.

A pick: `image` (`assets-work/Items/concepts/<key>/concept.jpg`), `meta`, `batch`, `variant`,
`picked` (date), `source_image` (the batch PNG, null when the batch is gone).

### Streaming commands: `--json-progress`

`run` and `refs` write one event per line. Every event has `protocol`, `event` and `time` (ISO 8601
with offset). The last line is always `finished`, `cancelled`, `dry_run`, `refused` or `error`
(unless the process is killed or the parser rejects the arguments).

| Event (`run`) | Fields |
|---------------|--------|
| `dry_run` | `batch`, `estimate`, `requests`, `images` (no `--yes`: nothing sent, exit 0) |
| `refused` | `reasons[]` (`code`, `message`), `estimate` (exit 3) |
| `started` | `batch`, `dir`, `kind`, `estimate`, `requests`, `images`, `concurrency` |
| `request_started` | `key`, `request` (`r1`, ...), `variants` (`v1`, ...), `estimated` |
| `retry` | `key`, `request`, `attempt`, `delay_s`, `reason` (e.g. `HTTP 429: ...`; never the key) |
| `request_done` | `key`, `request`, `variants[]` (`variant`, `image`, `meta`, `prompt` paths), `request_id`, `actual` (cost or null), `estimated` |
| `request_failed` | `key`, `request`, `error`, `status` (HTTP status or null) |
| `request_cancelled` | `key`, `request`: started, then not sent because of a cancel |
| `finished` | `batch`, `dir`, `sheet`, `done`, `failed`, `not_started`, `nothing_to_do`, `cost` (`estimated`, `actual`, `requests`, `requests_with_usage` of the requests this call had to do) |
| `cancelled` | the fields of `finished` plus `reason` (`SIGTERM`, `SIGINT`, `stdout closed`) and `resume` (the arguments that continue it) |
| `error` | `message`, `exit` |

`refs` uses the same names: `started` (`dir`, `items`, `requests` = references to render),
`request_done` (`key`, `reference`, `cached`), `request_started` (`key`; all stale items render in
one Blender call, so these come together), `request_failed` (`key`, `error`), `finished` (`dir`,
`rendered`, `cached`, `failed`), `cancelled`, `error`.

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | ok (also a dry run and "nothing to do") |
| 1 | other error (I/O, Blender, `sips`, a broken template or batch file) |
| 2 | usage: bad arguments, unknown item / model / batch / variant, a refine without comment, missing reference render, wrong `--repo-root` |
| 3 | refused: the estimate exceeds `--max-images` or `--max-cost` |
| 4 | the run finished but some requests failed (`run --resume <batch> --yes` retries them) |
| 5 | no API key (neither `OPENAI_API_KEY` nor the Keychain item); the message names the `security add-generic-password` command |
| 6 | busy: another process holds the batch (or the refs folder) |
| 130 | cancelled |

### Cancellation

Send SIGTERM (or SIGINT) to the child. It starts no new request, ends retry waits at once, lets
requests already on the wire finish (they are paid for; up to `--timeout`, 300 s by default),
saves the batch, writes the contact sheet, emits `cancelled` and exits 130. `run --resume <batch>
--yes --json-progress` sends the rest. A second signal exits at once with 130 and no further output;
images of requests still on the wire are then lost (and may still be billed). If the editor goes
away and stdout closes, the run cancels itself the same way at its next event. `refs` checks for
a cancel between model imports; its Blender render runs to the end.

### Locks

`<out-dir>/.lock` guards short sections (creating a batch folder, pick / unpick / discard),
`<batch>/.lock` is held by the run writing that batch for its whole duration, `<refs-dir>/.lock` by
`refs`. They are `flock` locks, so the system releases them when a process dies and a leftover lock
file of a dead process is simply taken over; while held the file names the holder's `pid`. A second
run of a locked batch exits 6 at once. `list --json` reports `running` per batch, which tells an
editor that restarted whether a run it started is still going.
