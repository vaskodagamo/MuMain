"""Read all available BMD headers to distinguish same-name texture resources."""
from pathlib import Path
import json
import struct
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
sys.path.insert(0,str(REPO/'assets-work/World1/Architecture03'))
from raw_bindings import payload


def main():
    consumers=[]
    failures=[]
    paths=sorted((REPO/'src/bin/Data').rglob('*.bmd'))
    for path in paths:
        try:
            data=payload(path)
            count=struct.unpack_from('<h',data,32)[0]
            cursor=38
            for index in range(count):
                nv,nn,nu,nt,slot=struct.unpack_from('<5h',data,cursor)
                cursor+=10+nv*16+nn*20+nu*8+nt*64
                material=data[cursor:cursor+32].split(b'\0')[0].decode('ascii')
                cursor+=32
                if material.lower()=='deep_wall12.jpg':
                    consumers.append(dict(model=str(path.relative_to(REPO)),mesh=index,
                        relative_folder_resource=str(path.parent.joinpath('deep_wall12.OZJ').relative_to(REPO)),
                        same_object2_resource=path.parent.name=='Object2'))
        except (AssertionError,ValueError,struct.error,UnicodeDecodeError) as error:
            failures.append(dict(path=str(path.relative_to(REPO)),error=repr(error)))
    result=dict(scanned=len(paths),matching_name_consumers=consumers,unreadable=failures,
        scope='Static Object2 models resolve this name inside Object2. Other ObjectN directories are independent resources; all texture bytes remain frozen.')
    (ROOT/'texture-consumers.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
