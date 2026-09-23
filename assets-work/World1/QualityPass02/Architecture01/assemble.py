"""Label actual BMD renders and reduced-scale comparison pairs."""
from pathlib import Path
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parent
NAMES=('HouseWall01','HouseWall04','HouseWall05','HouseWall06')
BACKGROUND=(27,32,36)
LABEL_HEIGHT=34


def sheet(paths,labels,out,width):
    images=[Image.open(path).convert('RGB') for path in paths]
    height=round(images[0].height*width/images[0].width)
    canvas=Image.new('RGB',(len(images)*width,height+LABEL_HEIGHT),BACKGROUND)
    draw=ImageDraw.Draw(canvas)
    for i,(image,label) in enumerate(zip(images,labels)):
        canvas.paste(image.resize((width,height),Image.Resampling.LANCZOS),(i*width,LABEL_HEIGHT))
        draw.text((i*width+12,10),label,fill='white')
    canvas.save(out)


for name in NAMES:
    folder=ROOT/name/'review'
    sheet([folder/'baseline.png',folder/'candidate.png'],['Merged baseline 7b808473',f'{name}: exported candidate'],folder/'comparison.jpg',700)
    sheet([folder/'baseline-reverse.png',folder/'candidate-reverse.png'],['Merged baseline: reverse','Exported candidate: reverse'],folder/'comparison-reverse.jpg',700)
    sheet([folder/'baseline-small.png',folder/'candidate-small.png'],['Merged baseline: reduced scale','Candidate: reduced scale'],folder/'comparison-small.png',300)
for name in ('town','west'):
    folder=ROOT/'review'
    if (folder/f'{name}-candidate.png').exists():
        sheet([folder/f'{name}-baseline.png',folder/f'{name}-candidate.png'],['Actual World1 transforms: merged baseline','Actual World1 transforms: candidate (offline)'],folder/f'{name}-comparison.jpg',1000)
