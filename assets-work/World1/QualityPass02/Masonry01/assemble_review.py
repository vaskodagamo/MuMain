"""Label matching actual-export offline comparisons without changing render pixels."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
NAMES = ('HouseEtc01', 'StoneMuWall02', 'StoneMuWall03')
WIDTH, HEIGHT = 1800, 1900
BACKGROUND = (34, 40, 39)
FONT = '/System/Library/Fonts/Helvetica.ttc'


def font(size):
    return ImageFont.truetype(FONT, size)


def paste_image(sheet, path, box):
    image = Image.open(path).convert('RGBA')
    image.thumbnail((box[2], box[3]), Image.Resampling.LANCZOS)
    x = box[0] + (box[2]-image.width)//2
    y = box[1] + (box[3]-image.height)//2
    sheet.paste(image, (x,y), image)


def main():
    sheet = Image.new('RGB', (WIDTH, HEIGHT), BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    draw.text((40,30), 'LORENCIA • MASONRY STUDIES — REJECTED', font=font(38), fill='#eee8da')
    draw.text((40,85), 'OFFLINE BLENDER • Actual BMD reimports • Frozen diffuse textures • Client check pending', font=font(23), fill='#b9c5bd')
    for column, name in enumerate(NAMES):
        x = column*600
        draw.text((x+25,145), name, font=font(28), fill='#d0bd91')
        for stage, y in [('current', 190), ('candidate', 790)]:
            draw.text((x+25,y), 'MERGED CURRENT' if stage=='current' else 'CANDIDATE', font=font(23), fill='#eee8da')
            paste_image(sheet, ROOT/name/'review'/f'{stage}.png', (x,y+35,600,520))
        draw.text((x+25,1400), 'REDUCED CURRENT / CANDIDATE', font=font(21), fill='#b9c5bd')
        paste_image(sheet, ROOT/name/'review/current-small.png', (x+15,1450,270,250))
        paste_image(sheet, ROOT/name/'review/candidate-small.png', (x+305,1450,270,250))
    counts = ' / '.join(str(json.loads((ROOT/name/'validation/blender.json').read_text())['triangles']) for name in NAMES)
    draw.text((40,1770), f'Candidate geometry: {counts} triangles. Same bounds, bones, actions and material order.', font=font(24), fill='#eee8da')
    draw.text((40,1810), 'All game BMDs remain baseline. House seam profile changed; pier gains insufficient at normal scale.', font=font(23), fill='#b9c5bd')
    sheet.save(ROOT/'comparison-sheet.jpg', quality=94)


if __name__ == '__main__':
    main()
