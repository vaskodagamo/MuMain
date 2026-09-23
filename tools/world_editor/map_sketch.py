"""Plan sketches for AI map editing: a tile grid and an edit script's shapes over a top-down image.

Used by `mapctl sketch` (tools/world_editor/mapctl.py); the guide is docs/agents/AI_MAP_EDITING.md.
Python 3.9+ standard library only.

The image is a top-down view of a tile rectangle, north up: a `map-export` layer (one pixel per
tile) or a `screenshot` with `region` taken after `map-camera` `topdown` on the same rectangle.
Tile point (x, y) lies at pixel ((x - x0) / columns * width, (y1 + 1 - y) / rows * height), the
orientation `legend.json` states for the export. Heights shift a perspective shot by a few
pixels; the sketch is a planning aid, not a measurement.

Colours, one per kind of op (`OP_COLOURS`): terrain yellow, texture magenta, attribute red, light
orange, objects cyan (a scatter's shape, a placed object as a filled square), object selections
green, scatter `avoid` areas white. Each shape carries its op's index in the script.
"""

import math
import struct
import zlib

PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
PNG_BIT_DEPTH = 8
PNG_COLOUR_CHANNELS = {0: 1, 2: 3, 4: 2, 6: 4}  # grey, RGB, grey + alpha, RGBA
PNG_RGB = 2
PNG_NO_INTERLACE = 0
PNG_COMPRESSION_LEVEL = 6
PNG_FILTER_NONE, PNG_FILTER_SUB, PNG_FILTER_UP, PNG_FILTER_AVERAGE, PNG_FILTER_PAETH = range(5)
RGB = 3
BYTE_MASK = 0xFF

TARGET_SKETCH_PIXELS = 1024
MAX_SCALE = 16
DEFAULT_GRID_TILES = 10
GRID_OPACITY = 0.35
LINE_THICKNESS = 2
MARKER_HALF_SIZE = 3
CIRCLE_SEGMENTS = 48
CAP_SEGMENTS = 16
DIGIT_WIDTH = 3
DIGIT_HEIGHT = 5
LABEL_SCALE = 2
OP_LABEL_SCALE = 3
LABEL_MARGIN = 2

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
OP_COLOURS = {
    'terrain': ((255, 220, 0), 'yellow'),
    'texture': ((255, 0, 255), 'magenta'),
    'attribute': ((255, 60, 60), 'red'),
    'light': ((255, 150, 0), 'orange'),
    'object': ((0, 255, 255), 'cyan'),
    'select': ((0, 255, 0), 'green'),
    'avoid': (WHITE, 'white'),
}

# 3 x 5 pixel digits, one string of 15 bits per digit, rows top to bottom.
DIGITS = {
    '0': '111101101101111', '1': '010110010010111', '2': '111001111100111',
    '3': '111001111001111', '4': '101101111001001', '5': '111100111001111',
    '6': '111100111101111', '7': '111001001001001', '8': '111101111101111',
    '9': '111101111001111', '-': '000000111000000',
}


class SketchError(Exception):
    pass


class RgbImage:
    """8-bit RGB pixels, rows top to bottom."""

    def __init__(self, width, height, pixels=None):
        if width <= 0 or height <= 0:
            raise SketchError('an image needs a width and a height')
        self.width = width
        self.height = height
        self.pixels = bytearray(pixels) if pixels is not None else bytearray(width * height * RGB)
        if len(self.pixels) != width * height * RGB:
            raise SketchError('the pixel data does not match %d x %d RGB' % (width, height))

    def get(self, x, y):
        offset = (y * self.width + x) * RGB
        return tuple(self.pixels[offset:offset + RGB])

    def put(self, x, y, colour, opacity=1.0):
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return
        offset = (y * self.width + x) * RGB
        for channel in range(RGB):
            old = self.pixels[offset + channel]
            self.pixels[offset + channel] = round(old + (colour[channel] - old) * opacity)

    def scaled(self, factor):
        """Nearest-neighbour enlargement by a whole factor."""
        if factor == 1:
            return RgbImage(self.width, self.height, self.pixels)
        rows = []
        stride = self.width * RGB
        for y in range(self.height):
            row = self.pixels[y * stride:(y + 1) * stride]
            wide = b''.join(bytes(row[x * RGB:(x + 1) * RGB]) * factor for x in range(self.width))
            rows.append(wide * factor)
        return RgbImage(self.width * factor, self.height * factor, b''.join(rows))


# ---------------------------------------------------------------------------------------------
# PNG


def _chunks(data):
    if not data.startswith(PNG_SIGNATURE):
        raise SketchError('not a PNG file')
    position = len(PNG_SIGNATURE)
    while position + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[position:position + 8])
        body = data[position + 8:position + 8 + length]
        if len(body) != length:
            raise SketchError('the PNG file is cut short')
        yield kind, body
        position += 12 + length


def _paeth(left, up, up_left):
    estimate = left + up - up_left
    to_left, to_up, to_up_left = abs(estimate - left), abs(estimate - up), abs(estimate - up_left)
    if to_left <= to_up and to_left <= to_up_left:
        return left
    return up if to_up <= to_up_left else up_left


def _unfilter_row(kind, row, previous, channels):
    stride = len(row)
    if kind == PNG_FILTER_NONE:
        return row
    if kind == PNG_FILTER_UP:
        return bytearray((value + above) & BYTE_MASK for value, above in zip(row, previous))
    for i in range(stride):
        left = row[i - channels] if i >= channels else 0
        if kind == PNG_FILTER_SUB:
            predictor = left
        elif kind == PNG_FILTER_AVERAGE:
            predictor = (left + previous[i]) >> 1
        elif kind == PNG_FILTER_PAETH:
            predictor = _paeth(left, previous[i], previous[i - channels] if i >= channels else 0)
        else:
            raise SketchError('unknown PNG filter %d' % kind)
        row[i] = (row[i] + predictor) & BYTE_MASK
    return row


def _to_rgb(samples, channels):
    if channels == RGB:
        return samples
    if channels == 4:
        return bytearray(b for i, b in enumerate(samples) if i % 4 != 3)
    grey = samples[::channels]
    return bytearray(value for value in grey for _ in range(RGB))


def decode_png(data):
    """8-bit grey, grey + alpha, RGB or RGBA PNG (what the client's stb writer produces) to RGB."""
    header, compressed = None, []
    for kind, body in _chunks(data):
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            compressed.append(body)
    if header is None:
        raise SketchError('the PNG file has no header')
    width, height, depth, colour, _, _, interlace = header
    if depth != PNG_BIT_DEPTH or colour not in PNG_COLOUR_CHANNELS or interlace != PNG_NO_INTERLACE:
        raise SketchError('only 8-bit, non-interlaced grey, RGB and RGBA PNGs are read')
    channels = PNG_COLOUR_CHANNELS[colour]
    try:
        raw = zlib.decompress(b''.join(compressed))
    except zlib.error as error:
        raise SketchError('the PNG image data is damaged: %s' % error)
    stride = width * channels
    if len(raw) < (stride + 1) * height:
        raise SketchError('the PNG image data is cut short')
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        row = _unfilter_row(raw[start], bytearray(raw[start + 1:start + 1 + stride]), previous, channels)
        rows.append(_to_rgb(row, channels))
        previous = row
    return RgbImage(width, height, b''.join(rows))


def _chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)


def encode_png(image):
    stride = image.width * RGB
    raw = b''.join(b'\x00' + bytes(image.pixels[y * stride:(y + 1) * stride]) for y in range(image.height))
    header = struct.pack('>IIBBBBB', image.width, image.height, PNG_BIT_DEPTH, PNG_RGB, 0, 0, PNG_NO_INTERLACE)
    return (PNG_SIGNATURE + _chunk(b'IHDR', header) + _chunk(b'IDAT', zlib.compress(raw, PNG_COMPRESSION_LEVEL)) +
            _chunk(b'IEND', b''))


def read_png(path):
    with open(path, 'rb') as stream:
        return decode_png(stream.read())


def write_png(image, path):
    with open(path, 'wb') as stream:
        stream.write(encode_png(image))


# ---------------------------------------------------------------------------------------------
# Tiles to pixels


class TileView:
    """Where the tiles of `rect` ([x0, y0, x1, y1], both corners included) lie on an image."""

    def __init__(self, rect, width, height):
        x0, y0, x1, y1 = rect
        self.x0, self.x1 = min(x0, x1), max(x0, x1)
        self.y0, self.y1 = min(y0, y1), max(y0, y1)
        self.width = width
        self.height = height
        self.pixels_per_tile_x = width / (self.x1 + 1 - self.x0)
        self.pixels_per_tile_y = height / (self.y1 + 1 - self.y0)

    def to_pixel(self, point):
        x, y = point
        return ((x - self.x0) * self.pixels_per_tile_x, (self.y1 + 1 - y) * self.pixels_per_tile_y)


def auto_scale(width, height):
    """The whole factor that brings a small image (a one pixel per tile export) near 1024 pixels."""
    return max(1, min(MAX_SCALE, TARGET_SKETCH_PIXELS // max(width, height)))


# ---------------------------------------------------------------------------------------------
# Drawing


def _draw_line(image, start, end, colour, thickness=LINE_THICKNESS, opacity=1.0):
    (ax, ay), (bx, by) = start, end
    steps = max(1, int(math.ceil(max(abs(bx - ax), abs(by - ay)))))
    half = thickness // 2
    seen = set()
    for step in range(steps + 1):
        t = step / steps
        cx, cy = int(round(ax + (bx - ax) * t)), int(round(ay + (by - ay) * t))
        for dx in range(-half, thickness - half):
            for dy in range(-half, thickness - half):
                if (cx + dx, cy + dy) not in seen:
                    seen.add((cx + dx, cy + dy))
                    image.put(cx + dx, cy + dy, colour, opacity)


def _draw_polyline(image, view, points, closed, colour):
    pixels = [view.to_pixel(point) for point in points]
    if closed:
        pixels.append(pixels[0])
    for start, end in zip(pixels, pixels[1:]):
        _draw_line(image, start, end, colour)


def _draw_marker(image, view, point, colour):
    cx, cy = (int(round(value)) for value in view.to_pixel(point))
    for dx in range(-MARKER_HALF_SIZE, MARKER_HALF_SIZE + 1):
        for dy in range(-MARKER_HALF_SIZE, MARKER_HALF_SIZE + 1):
            edge = max(abs(dx), abs(dy)) == MARKER_HALF_SIZE
            image.put(cx + dx, cy + dy, BLACK if edge else colour)


def draw_text(image, text, left, top, colour, scale=LABEL_SCALE):
    """Digits and '-' in a 3 x 5 pixel font with a black shadow; other characters are skipped."""
    for shadow, paint in ((1, BLACK), (0, colour)):
        x = left
        for character in text:
            glyph = DIGITS.get(character)
            if glyph is None:
                continue
            for index, bit in enumerate(glyph):
                if bit != '1':
                    continue
                gx, gy = x + (index % DIGIT_WIDTH) * scale + shadow, top + (index // DIGIT_WIDTH) * scale + shadow
                for dx in range(scale):
                    for dy in range(scale):
                        image.put(gx + dx, gy + dy, paint)
            x += (DIGIT_WIDTH + 1) * scale


def text_width(text, scale=LABEL_SCALE):
    return len(text) * (DIGIT_WIDTH + 1) * scale


def draw_grid(image, view, step):
    """A line every `step` tiles, labelled with its tile number along the bottom and left edges."""
    if step <= 0:
        return
    label_height = DIGIT_HEIGHT * LABEL_SCALE
    corner = text_width(str(view.y1 + 1)) + LABEL_MARGIN
    for x in range(-(-view.x0 // step) * step, view.x1 + 2, step):
        px = view.to_pixel((x, view.y0))[0]
        _draw_line(image, (px, 0), (px, image.height - 1), WHITE, 1, GRID_OPACITY)
        if px >= corner:
            draw_text(image, str(x), int(px) + LABEL_MARGIN, image.height - label_height - LABEL_MARGIN, WHITE)
    for y in range(-(-view.y0 // step) * step, view.y1 + 2, step):
        py = view.to_pixel((view.x0, y))[1]
        _draw_line(image, (0, py), (image.width - 1, py), WHITE, 1, GRID_OPACITY)
        draw_text(image, str(y), LABEL_MARGIN, int(py) - label_height - LABEL_MARGIN, WHITE)


# ---------------------------------------------------------------------------------------------
# Script shapes


def _circle(center, radius, segments):
    cx, cy = center
    return [(cx + radius * math.cos(2 * math.pi * i / segments), cy + radius * math.sin(2 * math.pi * i / segments))
            for i in range(segments)]


def _path_outlines(points, width):
    half = width / 2
    outlines = [(list(points), False)]
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        length = math.hypot(bx - ax, by - ay)
        if length == 0:
            continue
        nx, ny = -(by - ay) / length * half, (bx - ax) / length * half
        outlines.append(([(ax + nx, ay + ny), (bx + nx, by + ny)], False))
        outlines.append(([(ax - nx, ay - ny), (bx - nx, by - ny)], False))
    outlines.extend((_circle(point, half, CAP_SEGMENTS), True) for point in points)
    return outlines


def shape_outlines(shape):
    """A script shape as polylines in tile units: a list of (points, closed)."""
    kind = shape.get('type')
    if kind == 'circle':
        return [(_circle(shape['center'], shape['radius'], CIRCLE_SEGMENTS), True)]
    if kind == 'rect':
        x0, y0, x1, y1 = shape['rect']
        left, right, bottom, top = min(x0, x1), max(x0, x1) + 1, min(y0, y1), max(y0, y1) + 1
        return [([(left, bottom), (right, bottom), (right, top), (left, top)], True)]
    if kind == 'polygon':
        return [([tuple(point) for point in shape['points']], True)]
    if kind == 'path':
        return _path_outlines([tuple(point) for point in shape['points']], shape['width'])
    raise SketchError('unknown shape type %r' % kind)


def shape_top(shape):
    """The northmost point of a shape's outline, where its op's label goes."""
    points = [point for outline, _ in shape_outlines(shape) for point in outline]
    return max(points, key=lambda point: (point[1], -point[0]))


class SketchItem:
    """One thing to draw for an op: a shape outline or a point marker, in the colour of its role."""

    def __init__(self, op_index, op_name, role, shape=None, point=None):
        self.op_index = op_index
        self.op_name = op_name
        self.role = role
        self.shape = shape
        self.point = point

    def describe(self):
        what = 'point' if self.point is not None else self.shape.get('type')
        return {'op': self.op_index, 'name': self.op_name, 'draws': what, 'colour': OP_COLOURS[self.role][1]}


def _op_items(index, op):
    name = op.get('op', '')
    family = name.split('.', 1)[0]
    items = []
    if isinstance(op.get('shape'), dict):
        items.append(SketchItem(index, name, family if family in OP_COLOURS else 'object', shape=op['shape']))
    for area in (op.get('avoid') or {}).get('areas', []):
        items.append(SketchItem(index, name, 'avoid', shape=area))
    for key in ('select', 'under'):
        select = op.get(key)
        if isinstance(select, dict) and isinstance(select.get('inside'), dict):
            items.append(SketchItem(index, name, 'select', shape=select['inside']))
    for key in ('tile', 'to'):
        value = op.get(key)
        if name.startswith('object.') and isinstance(value, list) and len(value) == 2:
            items.append(SketchItem(index, name, 'object', point=tuple(value)))
    return items


def script_items(script):
    """What a `mu-map-edit/1` script would draw, op by op."""
    ops = script.get('ops') if isinstance(script, dict) else None
    if not isinstance(ops, list):
        raise SketchError('the script has no "ops" list')
    items = []
    for index, op in enumerate(ops):
        if isinstance(op, dict):
            items.extend(_op_items(index, op))
    return items


def _free_label_box(left, top, width, height, taken):
    """Moves a label right until it overlaps no label placed before it."""
    def overlaps(box):
        return left < box[0] + box[2] and box[0] < left + width and top < box[1] + box[3] and box[1] < top + height
    while any(overlaps(box) for box in taken):
        left += width + text_width(' ', OP_LABEL_SCALE)
    taken.append((left, top, width, height))
    return left


def draw_item(image, view, item, taken_labels):
    """The item's outline or marker, and its op's index just above its northmost point."""
    colour = OP_COLOURS[item.role][0]
    if item.point is not None:
        _draw_marker(image, view, item.point, colour)
        anchor = view.to_pixel(item.point)
    else:
        for points, closed in shape_outlines(item.shape):
            _draw_polyline(image, view, points, closed, colour)
        anchor = view.to_pixel(shape_top(item.shape))
    label = str(item.op_index)
    width, height = text_width(label, OP_LABEL_SCALE), DIGIT_HEIGHT * OP_LABEL_SCALE
    top = int(anchor[1]) - height - MARKER_HALF_SIZE - LABEL_MARGIN
    left = _free_label_box(int(anchor[0]) - width // 2, top, width, height, taken_labels)
    draw_text(image, label, left, top, colour, OP_LABEL_SCALE)


def sketch(image, rect, script=None, grid=DEFAULT_GRID_TILES):
    """Draw the grid and the script's shapes on `image` (changed in place). Returns what was drawn."""
    view = TileView(rect, image.width, image.height)
    draw_grid(image, view, grid)
    items = script_items(script) if script is not None else []
    taken_labels = []
    for item in items:
        draw_item(image, view, item, taken_labels)
    return [item.describe() for item in items]
