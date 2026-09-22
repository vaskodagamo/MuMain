"""Paint-aligned inward shingle laps; frozen dome edges, dormers and animated assembly."""
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
import build_source as common
ROOF='tile_wood03.jpg'
COURSES=(.222,.444,.655,.869)
LAP_DEPTH=4.0
BOUNDARY_MARGIN=6.0
VERTEX_MARGIN=4.0
U_STEP=.25


def roof_geometry(obj):
    faces=[f for f in obj.data.polygons if obj.data.materials[f.material_index].name==ROOF]
    edges={};anchors=[]
    for face in faces:
        points=[p for p,u,b in common.face_data(obj,face)];anchors.extend(points)
        for a,b in zip(points,points[1:]+points[:1]):
            key=tuple(sorted((tuple(round(v,4) for v in a),tuple(round(v,4) for v in b))))
            edges.setdefault(key,[]).append((a,b))
    return faces,[r[0] for r in edges.values() if len(r)==1],anchors


def depth_at(point,uv,bottom,top,high,edges,anchors):
    if top>=high-.00001:return 0
    margin=min(1,min(common.distance_to_edge(point,e) for e in edges)/BOUNDARY_MARGIN,min((point-p).length for p in anchors)/VERTEX_MARGIN)
    return LAP_DEPTH*(uv.y-bottom)/(top-bottom)*margin


def course(output,polygon,bottom,top,high,bone,material,edges,anchors):
    low_u,high_u=min(u.x for p,u in polygon),max(u.x for p,u in polygon)
    cuts=sorted(set([low_u,high_u]+[i*U_STEP for i in range(-20,21) if low_u<i*U_STEP<high_u]))
    for left,right in zip(cuts,cuts[1:]):
        strip=common.arch.clip_polygon(common.arch.clip_polygon(polygon,0,left,True),0,right,False)
        if len(strip)<3:continue
        points=[p-Vector((0,0,depth_at(p,u,bottom,top,high,edges,anchors))) for p,u in strip]
        common.emit(output,points,[u for p,u in strip],bone,material)
        for i,(p,u) in enumerate(strip):
            j=(i+1)%len(strip);q,v=strip[j]
            if abs(u.y-top)>.00001 or abs(v.y-top)>.00001:continue
            if max((points[i]-p).length,(points[j]-q).length)<.00001:continue
            # A narrow painted edge range gives finite UV area to the physical lap face.
            edge_uv=Vector((0,.012))
            common.emit(output,[points[i],points[j],q,p],[u-edge_uv,v-edge_uv,v,u],bone,material)


def build():
    folder=ROOT/'House04';bpy.ops.wm.open_mainfile(filepath=str(folder/'baseline/source.blend'))
    obj=common.mesh_helper.model_mesh();common.mesh_helper.snapshot_reference(obj)
    reference=common.arch.reference_collection('REF_ORIGINAL')
    with bpy.data.libraries.load(str(folder/'original/source.blend')) as (source,loaded):loaded.objects=source.objects
    for item in loaded.objects:
        if item.type=='MESH' and not item.get('mu_helper'):
            item.parent=None;item.modifiers.clear();item['mu_reference']=True;reference.objects.link(item)
    before=common.mesh_helper.bound([obj.matrix_world@v.co for v in obj.data.vertices])
    roof,edges,anchors=roof_geometry(obj);indices={f.index for f in roof};output=[]
    for face in obj.data.polygons:
        rows=common.face_data(obj,face)
        assert len({b for p,u,b in rows})==1
        if face.index not in indices:
            common.emit(output,[p for p,u,b in rows],[u for p,u,b in rows],rows[0][2],face.material_index);continue
        polygon=[(p,u) for p,u,b in rows]
        low,high=min(u.y for p,u in polygon),max(u.y for p,u in polygon)
        cuts=sorted(set([low,high]+[v for v in COURSES if low<v<high]))
        for bottom,top in zip(cuts,cuts[1:]):
            part=common.arch.clip_polygon(common.arch.clip_polygon(polygon,1,bottom,True),1,top,False)
            if len(part)>=3:course(output,part,bottom,top,high,rows[0][2],face.material_index,edges,anchors)
    common.mesh_helper.replace_mesh(obj,output)
    from preserve_source_normals import apply
    apply(obj,folder)
    after=common.mesh_helper.bound([obj.matrix_world@v.co for v in obj.data.vertices])
    assert max(abs(a-b) for aa,bb in zip(before,after) for a,b in zip(aa,bb))<.002
    bpy.ops.file.pack_all();bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'source.blend'))
    (folder/'validation/authored.json').write_text(json.dumps(dict(triangles=len(output),bounds_before=before,bounds_after=after,geometry_only=True,roof_boundary_edges=len(edges),protected='Every nonroof triangle, every original vertex and all dome perimeter/dormer edges'),indent=2)+'\n')

if __name__=='__main__':build()
