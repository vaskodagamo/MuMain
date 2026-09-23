"""Cost estimate and actual cost of concept image requests; standard library only.

Prices live in image_prices.json (the owner's pricing page). One request costs:

  output    n x the per-image price of (model, size, quality); for a model without a per-image
            row: n x output tokens x the output token price (flagged "token-estimated")
  reference reference images x reference_image_tokens_estimate x (ref size / reference_image_size)^2
            x the image input token price (an assumption: the gpt-image-1 rule for a 1024x1024 image
            at high input fidelity, scaled by pixel area for the smaller --ref-size uploads)
  text      prompt characters / text_chars_per_token_estimate x the text input token price

The actual cost of a finished request comes from the `usage` the API returns:
  input_tokens_details.image_tokens x image_in + input_tokens_details.text_tokens x text_in
  + output_tokens x image_out.
"""

from pathlib import Path
import json
import re

HERE = Path(__file__).resolve().parent
PRICES_FILE = HERE / 'image_prices.json'
TOKENS_PER_PRICE_UNIT = 1_000_000
BASE_SIZE = '1024x1024'
SIZE_PATTERN = re.compile(r'^(\d+)x(\d+)$')
SIZE_STEP = 16
FLAG_TOKEN_ESTIMATE = 'token-estimated (no per-image price)'
FLAG_SIZE_SCALED = 'size scaled from another price row'
FLAG_REFERENCE = 'reference input tokens are an assumption'


class CostError(ValueError):
    pass


def load_prices(path=PRICES_FILE):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def model_config(prices, model):
    config = prices['models'].get(model)
    if config is None:
        raise CostError(f'unknown model {model!r}; known: {", ".join(sorted(prices["models"]))} '
                        f'(add it to {PRICES_FILE.name})')
    return config


def parse_size(size):
    match = SIZE_PATTERN.match(size)
    if not match or int(match.group(1)) % SIZE_STEP or int(match.group(2)) % SIZE_STEP:
        raise CostError(f'size must be WIDTHxHEIGHT with multiples of {SIZE_STEP}: {size!r}')
    return int(match.group(1)), int(match.group(2))


def pixels(size):
    width, height = parse_size(size)
    return width * height


def check_quality(config, model, quality):
    if quality not in config['qualities']:
        raise CostError(f'{model} takes quality {"/".join(config["qualities"])}, not {quality!r}')


def size_row(table, size):
    """(row, scale, flags): the row for `size`, else one with the same pixel count, else 1024x1024 scaled."""
    if size in table:
        return table[size], 1.0, []
    same_pixels = [row for name, row in table.items() if pixels(name) == pixels(size)]
    if same_pixels:
        return same_pixels[0], 1.0, [FLAG_SIZE_SCALED]
    if BASE_SIZE not in table:
        raise CostError(f'no {BASE_SIZE} price row to scale from')
    return table[BASE_SIZE], pixels(size) / pixels(BASE_SIZE), [FLAG_SIZE_SCALED]


def token_price(config, kind, tokens):
    return tokens * config['token_prices_per_1m'][kind] / TOKENS_PER_PRICE_UNIT


def image_price(config, size, quality):
    """(price of one output image, flags)."""
    if 'per_image' in config:
        row, scale, flags = size_row(config['per_image'], size)
        return row[quality] * scale, flags
    row, scale, flags = size_row(config['output_tokens'], size)
    return token_price(config, 'image_out', row[quality] * scale), [FLAG_TOKEN_ESTIMATE] + flags


def reference_tokens(prices, ref_size):
    scale = (ref_size / prices['reference_image_size']) ** 2
    return prices['reference_image_tokens_estimate'] * scale


def estimate_request(prices, model, size, quality, n, prompt_chars, references, ref_size):
    """Estimated cost of one request, with its parts and flags."""
    config = model_config(prices, model)
    check_quality(config, model, quality)
    per_image, flags = image_price(config, size, quality)
    ref_tokens = references * reference_tokens(prices, ref_size)
    text_tokens = prompt_chars / prices['text_chars_per_token_estimate']
    parts = {
        'text': token_price(config, 'text_in', text_tokens),
        'reference': token_price(config, 'image_in', ref_tokens),
        'output': per_image * n,
    }
    if references:
        flags.append(FLAG_REFERENCE)
    return {'images': n, 'parts': parts, 'total': sum(parts.values()), 'flags': flags}


def actual_cost(prices, model, usage):
    """Cost parts and total from the API's `usage` block, or None when the response had none."""
    if not usage:
        return None
    config = model_config(prices, model)
    details = usage.get('input_tokens_details') or {}
    image_in = details.get('image_tokens', 0)
    text_in = details.get('text_tokens', usage.get('input_tokens', 0) - image_in)
    parts = {
        'text': token_price(config, 'text_in', text_in),
        'reference': token_price(config, 'image_in', image_in),
        'output': token_price(config, 'image_out', usage.get('output_tokens', 0)),
    }
    return {'parts': parts, 'total': sum(parts.values())}


def cap_problems(image_count, total_cost, max_images, max_cost):
    """Reasons the estimate is over a hard cap (empty when it is within both)."""
    problems = []
    if image_count > max_images:
        problems.append(f'{image_count} images exceed --max-images {max_images}')
    if total_cost > max_cost:
        problems.append(f'estimated ${total_cost:.2f} exceeds --max-cost ${max_cost:.2f}')
    return problems
