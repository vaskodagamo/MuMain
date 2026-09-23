"""Pair actual current and candidate export renders without changing their framing."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parent
for name in ('Grass05','Grass06'):
    folder=ROOT/name/'review'
    for suffix in ('','-reverse','-small'):
        images=[Image.open(folder/(stage+suffix+'.png')).convert('RGBA') for stage in ('current','candidate')]
        width,height=images[0].size
        sheet=Image.new('RGB',(2*width,height+40),(53,58,53))
        draw=ImageDraw.Draw(sheet)
        for i,(stage,img) in enumerate(zip(('Current: 112 triangles','Candidate: 640 triangles'),images)):
            sheet.paste(img,(i*width,40),img)
            draw.text((i*width+12,14),name+' / '+stage,fill='white')
        sheet.save(folder/('comparison'+suffix+'.jpg'),quality=94)

    for tone,color in (('light',(220,223,211)),('dark',(20,25,20))):
        sheet=Image.new('RGB',(1800,820),color)
        draw=ImageDraw.Draw(sheet)
        for i,stage in enumerate(('current','candidate')):
            img=Image.open(folder/(stage+'.png')).convert('RGBA')
            sheet.paste(img,(900*i,40),img)
            draw.text((900*i+12,14),name+' / '+stage,fill='black' if tone=='light' else 'white')
        sheet.save(folder/('alpha-'+tone+'.jpg'),quality=94)
for folder,label in ((ROOT/'review-assemblies','Grass06 actual placements'),(ROOT/'review-assemblies/Grass05','Grass05 actual placements')):
    if not (folder/'candidate-small.png').exists():continue
    for suffix in ('','-small'):
        images=[Image.open(folder/(stage+suffix+'.png')).convert('RGBA') for stage in ('current','candidate')]
        width,height=images[0].size
        sheet=Image.new('RGB',(width*2,height+40),(70,77,66));draw=ImageDraw.Draw(sheet)
        for i,(stage,img) in enumerate(zip(('current','candidate'),images)):
            sheet.paste(img,(width*i,40),img)
            draw.text((width*i+12,14),label+' / '+stage,fill='white')
        sheet.save(folder/('comparison'+suffix+'.jpg'),quality=94)
