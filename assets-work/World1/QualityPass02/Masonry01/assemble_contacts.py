"""Label actual HouseEtc01 stacked and adjacent placement comparisons."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
REVIEW = ROOT / 'review-assemblies'
FONT = '/System/Library/Fonts/Helvetica.ttc'


def main():
    sheet = Image.new('RGB',(1440,1120),(34,40,39))
    draw = ImageDraw.Draw(sheet)
    title = ImageFont.truetype(FONT,30)
    label = ImageFont.truetype(FONT,23)
    draw.text((25,20),'HOUSE ETC 01 / ACTUAL PLACEMENT CONTACT CHECK',font=title,fill='#eee8da')
    draw.text((25,65),'OFFLINE BLENDER • Unchanged World1 transforms • Merged current / candidate',font=label,fill='#b9c5bd')
    for row, group in enumerate(('house-stack','house-adjacent')):
        y = 110 + row*475
        draw.text((25,y),group.upper()+' — CURRENT',font=label,fill='#d0bd91')
        draw.text((745,y),'CANDIDATE',font=label,fill='#d0bd91')
        for column,stage in enumerate(('before','after')):
            image=Image.open(REVIEW/f'{group}-{stage}.png').convert('RGBA')
            image.thumbnail((720,420),Image.Resampling.LANCZOS)
            sheet.paste(image,(column*720,y+35),image)
    draw.text((25,1070),'Rejected: new stacked side-edge notch changes required modular profile. Baseline BMD retained.',font=label,fill='#b9c5bd')
    sheet.save(REVIEW/'house-contact-sheet.jpg',quality=94)


if __name__=='__main__':
    main()
