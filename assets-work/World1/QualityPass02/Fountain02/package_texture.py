"""Package a128²RGB art draft while preserving all non-dragon JPEG blocks."""
from pathlib import Path
import argparse,sys,subprocess,io,json,hashlib
import numpy as np
from PIL import Image,JpegImagePlugin
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
sys.path.insert(0,str(REPO/'tools'));import mu_texture
p=argparse.ArgumentParser();p.add_argument('draft');p.add_argument('output_dir');p.add_argument('--baseline',default=str(ROOT/'artwork/baseline-reagon_waterspout.OZJ'));p.add_argument('--masks',default=str(ROOT/'artwork/input-references'));p.add_argument('--patcher');a=p.parse_args();out=Path(a.output_dir);out.mkdir(parents=True,exist_ok=True)
baseline=Path(a.baseline);assert hashlib.sha256(baseline.read_bytes()).hexdigest()=='0da0208475dd23f3838e39615ff562ddec46a5f094c198b7c1e481443135b67d'
payload=mu_texture.unwrap_bytes(baseline.read_bytes(),'.ozj');original=Image.open(io.BytesIO(payload));original.load()
assert original.size==(128,128) and original.mode=='RGB' and JpegImagePlugin.get_sampling(original)==0
# Original ICC/metadata markers are retained by the coefficient patcher.
draft=Image.open(a.draft);draft.load();assert draft.size==(128,128) and draft.mode in ('RGB','RGBA')
if draft.mode=='RGBA':assert np.asarray(draft)[:,:,3].min()==255,'Draft must beopaque'
r=Path(a.masks);body=np.asarray(Image.open(r/'body-mask.png'))>0;wing=np.asarray(Image.open(r/'wings-mask.png'))>0;protected=np.asarray(Image.open(r/'protected-basin-plus2px.png'))>0
allowed=np.zeros((16,16),dtype=np.uint8)
for y in range(16):
 for x in range(16):
  block=np.s_[y*8:y*8+8,x*8:x*8+8]
  if (body|wing)[block].any() and not protected[block].any():allowed[y,x]=1
# Entire JPEG blocks touching protected surfaces remain bit-for-bit in coefficient space.
(out/'original.jpg').write_bytes(payload);draft.convert('RGB').save(out/'draft-original-quantization.jpg',format='JPEG',qtables=original.quantization,subsampling=0,optimize=False)
(out/'allowed-blocks.bin').write_bytes(allowed.tobytes())
if not a.patcher:
 flags=subprocess.run(['pkg-config','--cflags','--libs','libjpeg'],check=True,capture_output=True,text=True).stdout.split()
 a.patcher=str((out/'jpeg-block-patch').resolve())
 subprocess.run(['cc',str(ROOT/'jpeg_block_patch.c'),'-o',a.patcher,*flags],check=True)
run=subprocess.run([a.patcher,str(out/'original.jpg'),str(out/'draft-original-quantization.jpg'),str(out/'allowed-blocks.bin'),str(out/'patched.jpg')],check=True,capture_output=True,text=True)
wrapped=mu_texture.wrap_bytes((out/'patched.jpg').read_bytes(),'.ozj');(out/'reagon_waterspout.OZJ').write_bytes(wrapped)
reopened=mu_texture.unwrap_bytes(wrapped,'.ozj');assert reopened==(out/'patched.jpg').read_bytes();actual=Image.open(io.BytesIO(reopened));actual.load()
old=np.asarray(original).astype(int);new=np.asarray(actual).astype(int);difference=np.max(np.abs(old-new),axis=2)
editable=np.repeat(np.repeat(allowed.astype(bool),8,axis=0),8,axis=1)
assert np.max(difference[protected])==0,'Protected basin/filter-margin changed after actualJPEGdecode'
assert np.max(difference[~editable])==0,'Unselected original JPEG block changed after decoding'
assert actual.quantization==original.quantization and JpegImagePlugin.get_sampling(actual)==0
assert actual.info.get('icc_profile')==original.info.get('icc_profile'),'Original ICC profile changed'
report=dict(status='PASS',original_dimensions=list(original.size),subsampling='4:4:4',spatial_blocks_replaced=int(allowed.sum()),spatial_blocks_preserved=int((allowed==0).sum()),protected_margin_pixels=int(protected.sum()),max_protected_rgb_error=int(difference[protected].max()),max_all_unselected_rgb_error=int(difference[~editable].max()),changed_pixels=int((difference>0).sum()),input_draft_sha256=hashlib.sha256(Path(a.draft).read_bytes()).hexdigest(),original_ozj_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),output_ozj_sha256=hashlib.sha256(wrapped).hexdigest(),implementation=run.stdout.strip())
(out/'packaging-proof.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
