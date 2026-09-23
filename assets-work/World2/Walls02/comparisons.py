"""Label matching rendered images without changing their pixels."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parent


def pair(folder, suffix, target):
    images=[Image.open(folder/f'{name}{suffix}.png').convert('RGB') for name in ('current','candidate')]
    width,height=images[0].size
    sheet=Image.new('RGB',(width*2,height+40),(28,28,28))
    draw=ImageDraw.Draw(sheet)
    for index,(im,label) in enumerate(zip(images,('Current — retain (56 triangles)','Candidate — reject (66 triangles)'))):
        sheet.paste(im,(index*width,40))
        draw.text((index*width+12,12),label,fill='white')
    sheet.save(target)


for folder,name in [(ROOT/'Object51/review','isolated'),(ROOT/'context/candidate-comparison','joined-run')]:
    for suffix in ('','-small','-reverse'):
        pair(folder,suffix,ROOT/f'{name}{suffix}-comparison.png')
