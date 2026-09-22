"""Independent per-baseline-face band coverage with explicit no-interior-overlap proof."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from audit_exports import triangles
from audit_border import area,clip


def band_parts(points,rect):
    left,right,bottom,top=rect
    rules=(((0,left,False),),((0,right,True),),((0,left,True),(0,right,False),(1,bottom,False)),((0,left,True),(0,right,False),(1,top,True)))
    result=[]
    for region in rules:
        polygon=list(points)
        for axis,limit,greater in region:
            if not polygon:break
            polygon=clip(polygon,axis,limit,greater)
        if polygon and area(polygon)>1e-12:result.append(polygon)
    return result


def intersection(subject,boundary):
    result=list(subject)
    for a,b in zip(boundary,boundary[1:]+boundary[:1]):
        if not result:break
        output=[]
        def side(p):return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
        for x,y in zip(result,result[1:]+result[:1]):
            dx,dy=side(x),side(y)
            if dx>=0:output.append(x)
            if (dx>=0)!=(dy>=0):
                t=dx/(dx-dy);output.append([p+t*(q-p) for p,q in zip(x,y)])
        result=output
    return result


def bounds(poly):
    return [min(p[k] for p in poly) for k in range(2)],[max(p[k] for p in poly) for k in range(2)]


def check():
    folder=ROOT/'HouseEtc01';design=json.loads((folder/'validation/source.json').read_text())['design']
    sx,sy=design['scale'];band=design['border_units'];rect=(.0005+band/sx,.9995-band/sx,.0005+band/sy,.9995-band/sy)
    original=[f for f in triangles(folder/'validation/baseline/HouseEtc01.smd') if f[0]=='c_wall06.jpg']
    candidate=[f for f in triangles(folder/'validation/new/HouseEtc01.smd') if f[0]=='c_wall06.jpg']
    fragments=[p for _,rows in candidate for p in band_parts([r[7:9] for r in rows],rect)]
    boxes=[bounds(p) for p in fragments];maximum_overlap=0
    for i,a in enumerate(fragments):
        for j in range(i+1,len(fragments)):
            if any(min(boxes[i][1][k],boxes[j][1][k])<=max(boxes[i][0][k],boxes[j][0][k]) for k in range(2)):continue
            overlap=intersection(a,fragments[j]);value=area(overlap) if overlap else 0
            assert value<1e-10,(i,j,value)
            maximum_overlap=max(maximum_overlap,value)
    reports=[]
    for index,(_,rows) in enumerate(original):
        triangle=[r[7:9] for r in rows];expected=sum(area(p) for p in band_parts(triangle,rect))
        covered=sum(area(p) for f in fragments if (p:=intersection(f,triangle)))
        assert abs(expected-covered)<1e-6,(index,expected,covered)
        reports.append(dict(baseline_front_triangle=index,expected_area=expected,covered_area=covered,error=abs(expected-covered)))
    (folder/'validation/band-coverage.json').write_text(json.dumps(dict(status='PASS',band_units=band,
        measurement='8 model-axis units from the x/z perimeter, using registered UV-to-axis scales; not geodesic distance',
        candidate_band_fragments=len(fragments),maximum_pairwise_interior_overlap_area=maximum_overlap,
        overlap_tolerance=1e-10,per_baseline_triangle=reports),indent=2))
if __name__=='__main__':check()
