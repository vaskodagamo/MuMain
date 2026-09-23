import sys,pathlib,json,hashlib
sys.dont_write_bytecode=True
import os
R=pathlib.Path(os.environ['MU_REVIEW_REPO'])
sys.path.insert(0,str(R/'tools/blender'))
import mu_bmd_import as official

def action_lines(path):
 out=[]
 for line in pathlib.Path(path).read_bytes().splitlines():
  if line.startswith(b'action '):
   w=line.decode('ascii').split();rec={'index':int(w[1])};rec.update(dict(x.split('=',1) for x in w[2:]));out.append(rec)
 return out
official.read_manifest=action_lines
official.main()
