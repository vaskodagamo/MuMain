"""Label matching unchanged renders; no asset-paint modification."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parent


def pair(folder, suffix):
    paths = [folder/(stage+suffix+'.png') for stage in ('current','candidate')]
    images = [Image.open(path).convert('RGB') for path in paths]
    canvas = Image.new('RGB',(sum(im.width for im in images),max(im.height for im in images)+32),(24,24,24))
    draw = ImageDraw.Draw(canvas)
    x = 0
    for im,label in zip(images,('CURRENT - 284 triangles','STUDY - 428 triangles, one bone revised')):
        draw.text((x+8,9),label,fill='white')
        canvas.paste(im,(x,32))
        x += im.width
    canvas.save(folder/('comparison'+suffix+'.jpg'),quality=95)


def main():
    for folder in [ROOT/'Object48/review',*(ROOT/'review-assemblies').iterdir()]:
        if not folder.is_dir():
            continue
        for suffix in ('','-reverse','-small'):
            pair(folder,suffix)


if __name__ == '__main__':
    main()
