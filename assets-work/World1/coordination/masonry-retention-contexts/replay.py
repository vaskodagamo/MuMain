"""Fresh official imports and original-selection masonry views; all output under this directory."""
import hashlib,json,os,pathlib,re,struct,subprocess,sys
sys.dont_write_bytecode=True
OUT=pathlib.Path(__file__).resolve().parent
REPO=pathlib.Path(os.environ.get('MU_REVIEW_REPO','/Users/lukasmac/Documents/claude-test-mumain/MuMain-environment-remake'))
BLENDER=os.environ.get('MU_BLENDER','/Users/lukasmac/Documents/claude-test-mumain/astra-tools/Blender.app/Contents/MacOS/Blender')
CONVERTER=os.environ.get('MU_BMDCONV','/Users/lukasmac/Documents/claude-test-mumain/MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv')
ENV={**os.environ,'MU_REVIEW_REPO':str(REPO),'PYTHONDONTWRITEBYTECODE':'1','BLENDER_USER_SCRIPTS':'/Users/lukasmac/Documents/claude-test-mumain/astra-tools/blender-user/scripts','BLENDER_USER_CONFIG':'/Users/lukasmac/Documents/claude-test-mumain/astra-tools/blender-user/config'}
OLD='fb734c0cee0c3e71b0c39ab9152220336294243c'
GROUPS=['house-stack','south-gate','siege-wall']
def git(*args):return subprocess.check_output(['git',*args],cwd=REPO)
def digest(data):return hashlib.sha256(data).hexdigest()
def prepare():
 raw=git('show','HEAD:src/bin/Data/World1/EncTerrain1.obj');key=bytes.fromhex('d17352f6d29acb273eaf593137b3e7a2');rolling=0x5e;decoded=bytearray()
 for i,v in enumerate(raw):decoded.append(((v^key[i%16])-rolling)&255);rolling=(v+0x3d)&255
 count=struct.unpack_from('<h',decoded,2)[0];records=[struct.unpack_from('<h7f',decoded,4+i*30) for i in range(count)]
 model_types=json.loads((REPO/'assets-work/World1/coordination/dependency-map.json').read_text())['models']
 proof={'revision':git('rev-parse','HEAD').decode().strip(),'old_revision':OLD,'map_sha256':digest(raw),'groups':{},'sources':{}}
 names=set()
 for group in GROUPS:
  rel=f'assets-work/World1/QualityPass02/Masonry01/review-assemblies/{group}-placements.json';old=(OUT/'legacy'/(group+'-placements.json')).read_bytes();p=json.loads(old)
  for q in p['objects']:
   target=tuple(q['position']+q['rotation']+[q['scale']]);hits=[i for i,v in enumerate(records) if tuple(v[1:])==target and v[0]==model_types[q['name']]['type']];assert hits,(group,q);q['world_record_indices']=hits;names.add(q['name'])
  p['old_placement_sha256']=digest(old);p['source_revision']=proof['revision'];(OUT/(group+'-placements.json')).write_text(json.dumps(p,indent=2)+'\n');proof['groups'][group]={'placement_manifest':group+'-placements.json','old_selection_path':rel,'objects':len(p['objects'])}
 for name in sorted(names):
  folder=OUT/name;folder.mkdir(exist_ok=True);src=REPO/'src/bin/Data/Object1'/f'{name}.bmd';info=subprocess.check_output([CONVERTER,'info',str(src)]);(folder/'info.txt').write_bytes(info);files=[src]
  for t in re.findall(rb'texture=(\S+)',info):
   t=pathlib.Path(t.decode());files.append(src.parent/t.with_suffix('.OZT' if t.suffix.lower()=='.tga' else '.OZJ'))
  proof['sources'][name]={str(f.relative_to(REPO)):digest(f.read_bytes()) for f in files}
  with (folder/'import.log').open('w') as log:subprocess.run([BLENDER,'--factory-startup','-b','--python-exit-code','1','--python',str(OUT/'official_import.py'),'--','--bmd',str(src),'--out',str(folder/'import.blend'),'--bmdconv',CONVERTER],env=ENV,stdout=log,stderr=subprocess.STDOUT,check=True)
  print('Imported',name,flush=True)
 (OUT/'source-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
def main():
 prepare()
 for group in GROUPS:
  with (OUT/(group+'.log')).open('w') as log:subprocess.run([BLENDER,'--factory-startup','-b','--python-exit-code','1','--python',str(OUT/'render_context.py'),'--',group],env=ENV,stdout=log,stderr=subprocess.STDOUT,check=True)
  print('Rendered',group,flush=True)
 refs={str(p.relative_to(OUT)):digest(p.read_bytes()) for p in OUT.rglob('*') if p.is_file() and p.name!='manifest.json'};(OUT/'manifest.json').write_text(json.dumps(refs,indent=2)+'\n')
if __name__=='__main__':main()
