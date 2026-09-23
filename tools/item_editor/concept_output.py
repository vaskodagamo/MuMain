"""Contact sheets of a concept batch and the owner's picks; standard library only (+ macOS `sips`).

sheet: <batch>/sheet.html, one row per item: current look | v1 | v2 | ..., the item's name, keys
       and tier, the prompts in a <details>, and the pick command under each variant.
pick:  copies one variant into assets-work/Items/concepts/<key>/ as concept.jpg (JPEG quality 90,
       longest side at most 1024 px, converted with `sips`; a transparent background turns
       white), plus prompt.txt and meta.json. unpick removes those files again.
"""

from pathlib import Path
import datetime
import html
import json
import shutil
import struct
import subprocess

import concept_select

CONCEPTS_DIR = concept_select.ROOT / 'assets-work' / 'Items' / 'concepts'
SHEET_FILE = 'sheet.html'
BATCH_FILE = 'batch.json'
REFERENCE_FILE = 'ref.png'
PICK_IMAGE = 'concept.jpg'
PICK_PROMPT = 'prompt.txt'
PICK_META = 'meta.json'
PICK_FILES = (PICK_IMAGE, PICK_PROMPT, PICK_META)
JPEG_QUALITY = 90
MAX_SIDE = 1024
SIPS = 'sips'
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
PNG_SIZE_OFFSET = 16
PNG_SIZE_END = 24


class OutputError(RuntimeError):
    pass


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def variant_name(variant):
    text = str(variant)
    return text if text.startswith('v') else f'v{text}'


def png_size(path):
    header = Path(path).read_bytes()[:PNG_SIZE_END]
    if not header.startswith(PNG_SIGNATURE):
        raise OutputError(f'{path} is not a PNG')
    return struct.unpack('>II', header[PNG_SIZE_OFFSET:PNG_SIZE_END])


# --- sheet -------------------------------------------------------------------------------------

SHEET_STYLE = """
body { background: #1d2126; color: #e4e6e8; font: 14px/1.4 -apple-system, Helvetica, sans-serif; margin: 16px; }
table { border-collapse: collapse; }
td, th { border-top: 1px solid #3a4048; padding: 8px; vertical-align: top; text-align: left; }
figure { margin: 0; }
img { width: 256px; height: 256px; object-fit: contain; display: block;
      background: repeating-conic-gradient(#555b63 0 25%, #6b7179 0 50%) 50% / 16px 16px; }
figcaption, code { font-size: 12px; color: #aeb4ba; }
.picked img { outline: 3px solid #d7c08b; }
details { max-width: 480px; white-space: pre-wrap; font-size: 12px; color: #c9cdd1; }
"""


def picked_variant(concepts_dir, key):
    meta = Path(concepts_dir) / key / PICK_META
    return read_json(meta).get('variant') if meta.is_file() else None


def figure(src, caption, picked=False):
    css = ' class="picked"' if picked else ''
    return (f'<figure{css}><img src="{html.escape(src)}" alt="{html.escape(caption)}" loading="lazy">'
            f'<figcaption>{caption}</figcaption></figure>')


def sheet_row(batch_id, subject, variant_count, concepts_dir):
    key = subject['key']
    picked = picked_variant(concepts_dir, key)
    cells = [figure(f'{key}/{REFERENCE_FILE}', 'current look')]
    for number in range(1, variant_count + 1):
        name = variant_name(number)
        command = html.escape(f'concepts.py pick {batch_id} {key} {name}')
        cells.append(figure(f'{key}/{name}.png', f'{name}<br><code>{command}</code>', picked == name))
    prompts = '\n\n---\n\n'.join(html.escape(request['prompt']) for request in subject['requests'])
    info = (f'<b>{html.escape(subject["name"])}</b><br>{html.escape(", ".join(subject["keys"]))}<br>'
            f'T{subject["tier"]} · {html.escape(subject["family"])}<details><summary>prompt</summary>{prompts}</details>')
    return '<tr><td>' + info + '</td>' + ''.join(f'<td>{cell}</td>' for cell in cells) + '</tr>'


def write_sheet(batch_dir, concepts_dir=CONCEPTS_DIR):
    batch = read_json(Path(batch_dir) / BATCH_FILE)
    variant_count = max(max(r['variants']) for s in batch['subjects'] for r in s['requests'])
    settings = batch['settings']
    title = html.escape(f'{batch["batch"]} - {settings["model"]} {settings["quality"]} {settings["size"]}')
    header = '<tr><th>item</th><th>current</th>' + ''.join(
        f'<th>{variant_name(n)}</th>' for n in range(1, variant_count + 1)) + '</tr>'
    rows = ''.join(sheet_row(batch['batch'], subject, variant_count, concepts_dir) for subject in batch['subjects'])
    page = (f'<!doctype html><html><head><meta charset="utf-8"><title>{title}</title>'
            f'<style>{SHEET_STYLE}</style></head><body><h1>{title}</h1>'
            f'<table>{header}{rows}</table></body></html>\n')
    path = Path(batch_dir) / SHEET_FILE
    path.write_text(page, encoding='utf-8')
    return path


# --- pick / unpick -----------------------------------------------------------------------------

def convert_to_jpeg(source, target):
    if shutil.which(SIPS) is None:
        raise OutputError(f'`{SIPS}` (macOS) is needed to write the JPEG')
    command = [SIPS, '-s', 'format', 'jpeg', '-s', 'formatOptions', str(JPEG_QUALITY)]
    if max(png_size(source)) > MAX_SIDE:
        command += ['-Z', str(MAX_SIDE)]
    result = subprocess.run(command + [str(source), '--out', str(target)], capture_output=True, text=True)
    if result.returncode != 0 or not Path(target).is_file():
        raise OutputError(f'sips failed: {result.stderr.strip() or result.stdout.strip()}')


def pick(batch_dir, key, variant, concepts_dir=CONCEPTS_DIR, today=None):
    """Copy one variant of a batch into concepts/<key>/; returns the folder."""
    name = variant_name(variant)
    source_dir = Path(batch_dir) / key
    image, meta_file, prompt_file = (source_dir / f'{name}{suffix}' for suffix in ('.png', '.meta.json', '.prompt.txt'))
    if not image.is_file():
        raise OutputError(f'no image {image}')
    target = Path(concepts_dir) / key
    target.mkdir(parents=True, exist_ok=True)
    convert_to_jpeg(image, target / PICK_IMAGE)
    shutil.copyfile(prompt_file, target / PICK_PROMPT)
    meta = read_json(meta_file)
    meta.update({'picked_from': f'{Path(batch_dir).name}/{key}/{name}.png', 'variant': name,
                 'picked': (today or datetime.date.today()).isoformat(), 'image': PICK_IMAGE})
    (target / PICK_META).write_text(json.dumps(meta, indent=1, sort_keys=True) + '\n', encoding='utf-8')
    return target


def unpick(key, concepts_dir=CONCEPTS_DIR):
    """Remove concepts/<key>/ (only the files pick writes); returns whether anything was removed."""
    target = Path(concepts_dir) / key
    removed = False
    for name in PICK_FILES:
        if (target / name).is_file():
            (target / name).unlink()
            removed = True
    if target.is_dir() and not any(target.iterdir()):
        target.rmdir()
    return removed
