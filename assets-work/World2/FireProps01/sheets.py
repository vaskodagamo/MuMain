"""Arrange actual render pixels with honest diagnostic labels."""
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent


def pair(paths,labels,out):
    images=[Image.open(p).convert('RGB') for p in paths]
    width,height=images[0].size
    sheet=Image.new('RGB',(width*len(images),height+36),(28,28,28))
    draw=ImageDraw.Draw(sheet)
    for index,(image,label) in enumerate(zip(images,labels)):
        sheet.paste(image,(index*width,36))
        draw.text((index*width+10,10),label,fill='white')
    sheet.save(out)


for name in ('Object42','Object43'):
    folder=ROOT/name/'review'
    pair([folder/'baseline-full.png',folder/'baseline-wood.png'],['Original full static model','Original wood-only diagnostic'],ROOT/f'{name}-full-and-wood.png')
    pair([folder/'baseline-full-small.png',folder/'roundtrip-full-small.png'],['Original - retained','Unchanged roundtrip CONTROL only'],ROOT/f'{name}-control-small.png')
    pair([folder/'baseline-full-reverse.png',folder/'roundtrip-full-reverse.png'],['Original reverse - retained','Roundtrip reverse CONTROL only'],ROOT/f'{name}-control-reverse.png')
pair([ROOT/'context'/name/'baseline-small.png' for name in ['Object42-wall','Object43']],['Object42 actual wall record297','Object43 actual stand record295'],ROOT/'placed-retention-small.png')
