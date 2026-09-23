"""Prove standalone well/pottery parts match composite in their original bone frames."""
from pathlib import Path
import json
import math
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from raw_audit import rotation

def read(name,stage='new'):
    text=(ROOT/name/'validation'/stage/(name+'.smd')).read_text()
    names=[line.split('"')[1] for line in text.split('nodes\n')[1].split('\nend')[0].splitlines()];poses=[]
    for line in text.split('skeleton\n')[1].split('\nend')[0].splitlines()[1:]:
        row=list(map(float,line.split()));poses.append((row[1:4],rotation(row[4:])))
    lines=text.split('triangles\n')[1].splitlines()[:-1];result=[]
    for i in range(0,len(lines),4):
        corners=[]
        for line in lines[i+1:i+4]:
            r=list(map(float,line.split()));bone=int(r[0]);p,rot=poses[bone];delta=[x-y for x,y in zip(r[1:4],p)]
            local=[sum(rot[k][j]*delta[k] for k in range(3)) for j in range(3)]
            corners.append((names[bone],local,r[7:9]))
        result.append((lines[i],corners))
    return result

def compare(a,b):
    assert len(a)==len(b),(len(a),len(b))
    remaining=set(range(len(b)));maximum_position=0;maximum_uv=0
    for mat,corners in a:
        best=(float('inf'),None,None)
        for i in remaining:
            if b[i][0]!=mat:continue
            for shift in range(3):
                candidate=[b[i][1][(j+shift)%3] for j in range(3)]
                if any(x[0]!=y[0] for x,y in zip(corners,candidate)):continue
                position=max(math.dist(x[1],y[1]) for x,y in zip(corners,candidate))
                uv=max(math.dist(x[2],y[2]) for x,y in zip(corners,candidate))
                error=max(position,uv)
                if error<best[0]:best=(error,i,(position,uv))
            if best[0]<.000001:break
        assert best[0]<.0003,best
        position,uv=best[2]
        assert uv<.000001,('UV budget',uv)
        remaining.remove(best[1])
        maximum_position=max(maximum_position,position);maximum_uv=max(maximum_uv,uv)
    return dict(triangles=len(a),maximum_local_position_error=maximum_position,
                maximum_uv_error=maximum_uv,named_bones_material_cyclic_winding='EXACT')

def face_error(a,b):
    if a[0]!=b[0]:return float('inf')
    best=float('inf')
    for shift in range(3):
        candidates=[b[1][(j+shift)%3] for j in range(3)]
        if any(x[0]!=y[0] for x,y in zip(a[1],candidates)):continue
        best=min(best,max(max(math.dist(x[1],y[1]),math.dist(x[2],y[2])) for x,y in zip(a[1],candidates)))
    return best

def mouth(name):
    old=read(name,'baseline');new=read(name)
    necks={'Cylinder07':.142,'Cylinder08':.4029,'Cylinder09':.6758,'Cylinder10':.9141}
    return [face for face in new if face[0]=='jar_01.jpg'
        and min(c[2][1] for c in face[1])>necks[face[1][0][0]]+.00001
        and not any(face_error(face,baseline)<.0003 for baseline in old)]

def main():
    whole=read('Well01');result={}
    result['Well02']=compare(read('Well02'),[face for face in whole if face[0]=='well.jpg'])
    result['shared_new_pot_mouths']=compare(mouth('Well03'),mouth('Well01'))
    assert result['shared_new_pot_mouths']['triangles']==200,result
    for material in ('tub.jpg','horse_drawn_01.jpg'):
        result[material]=compare([face for face in read('Well01','baseline') if face[0]==material],[face for face in whole if face[0]==material])
    (ROOT/'composite-part-match.json').write_text(json.dumps(dict(status='PASS',parts=result,position_tolerance=.0003,uv_tolerance=.000001,
        criterion='All common new lip/throat triangles match exact named bones, materials, cyclic winding, local positions and UVs; neck join adapts to each retained baseline body. Entire well matches standalone. Readonly cask/support materials retain baseline triangles.'),indent=2))
if __name__=='__main__':main()
