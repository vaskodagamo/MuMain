"""Label exact alpha-bearing reimport comparisons, including reduced and reverse views."""
from pathlib import Path
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parent
NAMES=('Tree09','Tree10','Grass03','Grass04')


def comparison(folder,suffix,width):
    images=[Image.open(folder/f'{stage}{suffix}.png').convert('RGB') for stage in ('baseline','candidate')]
    height=round(images[0].height*width/images[0].width)
    sheet=Image.new('RGB',(2*width,height+34),(27,32,36))
    draw=ImageDraw.Draw(sheet)
    for index,image in enumerate(images):
        sheet.paste(image.resize((width,height),Image.Resampling.LANCZOS),(index*width,34))
        draw.text((index*width+12,10),['Merged baseline','Exported geometry candidate'][index],fill='white')
    sheet.save(folder/f'comparison{suffix}.jpg')


for name in NAMES:
    folder=ROOT/name/'review'
    for suffix,width in [('',700),('-reverse',700),('-light',700),('-small',300)]:
        if (folder/f'candidate{suffix}.png').exists(): comparison(folder,suffix,width)
