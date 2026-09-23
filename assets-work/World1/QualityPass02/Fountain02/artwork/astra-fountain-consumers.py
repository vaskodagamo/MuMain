from pathlib import Path
import subprocess,concurrent.futures,json,hashlib
r=Path('/Users/lukasmac/Documents/claude-test-mumain/MuMain-q02-furniture');conv='/Users/lukasmac/Documents/claude-test-mumain/MuMain/out/build/macos-arm64/tools/bmdconv/Release/bmdconv'
paths=sorted(p for p in (r/'src/bin/Data').rglob('*') if p.suffix.lower()=='.bmd')
def inspect(p):
 q=subprocess.run([conv,'info',str(p)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 s=q.stdout.decode('utf8','replace');return p,q.returncode,s,q.stderr.decode('utf8','replace')
consumers=[];failures=[];ok=0
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
 for p,code,s,error in pool.map(inspect,paths):
  if code:failures.append(dict(path=str(p.relative_to(r)),error=error[:240]));continue
  ok+=1
  if 'reagon_waterspout' in s.lower():consumers.append(dict(path=str(p.relative_to(r)),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),info=s))
texture=r/'src/bin/Data/Object1/reagon_waterspout.OZJ'
report=dict(scanned=len(paths),valid_models=ok,consumers=consumers,failures=failures,texture_sha256=hashlib.sha256(texture.read_bytes()).hexdigest())
Path('/tmp/astra-fountain-consumers.json').write_text(json.dumps(report,indent=2));print(json.dumps(dict(scanned=len(paths),valid=ok,consumers=consumers,failures=len(failures))))
