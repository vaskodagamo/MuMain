from pathlib import Path
import sys,json,math
sys.dont_write_bytecode=True
sys.path.insert(0,'/Users/lukasmac/Documents/claude-test-mumain/MuMain-q02-furniture/assets-work/World1/QualityPass02/Masonry02')
from audit_exports import triangles
from PIL import Image,ImageDraw,ImageFilter
r=Path('/Users/lukasmac/Documents/claude-test-mumain/MuMain-q02-furniture');faces=triangles(r/'assets-work/World1/Rocks01/Waterspout01/original/smd/Waterspout01.smd')
tex=Image.open(r/'assets-work/World1/Rocks01/original/reagon_waterspout.jpg').convert('RGB');w,h=tex.size
out=Path('/tmp/astra-fountain-texture');out.mkdir(exist_ok=True)
groups={'protected_basin':faces[72:106],'body':faces[106:395],'wings':faces[395:489]};masks={};detail={}
for name,rows in groups.items():
 im=Image.new('L',(w*8,h*8));draw=ImageDraw.Draw(im)
 overlay=tex.resize((1024,1024),Image.Resampling.NEAREST);d=ImageDraw.Draw(overlay)
 for material,corners in rows:
  assert material=='reagon_waterspout.jpg'
  uv=[(r[7],r[8]) for r in corners];pts=[(u*w*8,(1-v)*h*8) for u,v in uv];draw.polygon(pts,fill=255)
  d.line([(u*1024,(1-v)*1024) for u,v in uv+[uv[0]]],fill=(255,75,60),width=1)
 mask=im.resize((w,h),Image.Resampling.BOX).point(lambda x:255 if x else 0);masks[name]=mask;mask.save(out/(name+'-mask.png'));overlay.save(out/(name+'-uv.png'))
 detail[name]={'triangles':len(rows),'texel_coverage':sum(x>0 for x in mask.getdata()),'uv_bounds':[[min(r[k] for _,rs in rows for r in rs) for k in (7,8)],[max(r[k] for _,rs in rows for r in rs) for k in (7,8)]]}
protected=masks['protected_basin'].filter(ImageFilter.MaxFilter(5));protected.save(out/'protected-basin-plus2px.png')
for name in ['body','wings']:
 a=list(masks[name].getdata());b=list(masks['protected_basin'].getdata());c=list(protected.getdata());detail[name]['overlap_protected_pixels']=sum(x>0 and y>0 for x,y in zip(a,b));detail[name]['overlap_protected_plus2px']=sum(x>0 and y>0 for x,y in zip(a,c));detail[name]['unprotected_2px_pixels']=sum(x>0 and not y for x,y in zip(a,c))
tex.resize((1024,1024),Image.Resampling.NEAREST).save(out/'atlas-nearest.png')
(out/'uv-coverage.json').write_text(json.dumps(detail,indent=2));print(json.dumps(detail,indent=2))
