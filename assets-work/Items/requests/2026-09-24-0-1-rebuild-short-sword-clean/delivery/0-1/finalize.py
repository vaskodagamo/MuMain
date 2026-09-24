import bpy,bmesh,json
from pathlib import Path
D=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(D/'source.blend'));o=bpy.data.objects['Short_Sword_Retopology'];mat=o.data.materials[0];mat.name='sword03.jpg'
s=bpy.context.scene;s.render.image_settings.file_format='JPEG';s.render.image_settings.quality=98;s.view_settings.view_transform='Standard';s.view_settings.look='None'
img=bpy.data.images['sword03'];img.save_render(str(D/'exports/sword03.jpg'),scene=s)
img.unpack(method='REMOVE');img.filepath=str(D/'exports/sword03.jpg');img.reload();img.pack()
bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
seen=set();volumes=[]
for v in bm.verts:
 if v in seen:continue
 todo=[v];component=set()
 while todo:
  q=todo.pop()
  if q in seen:continue
  seen.add(q);component.add(q);todo.extend(e.other_vert(q) for e in q.link_edges)
 faces={f for q in component for f in q.link_faces}
 volumes.append(sum(f.verts[0].co.dot(f.verts[1].co.cross(f.verts[2].co))/6 for f in faces))
report={'non_manifold_edges':sum(not e.is_manifold for e in bm.edges),'degenerate_triangles':sum(f.calc_area()<1e-9 for f in bm.faces),'connected_closed_components':len(volumes),'signed_component_volumes':volumes,'outward_normals':all(v>0 for v in volumes),'triangles':len(bm.faces),'bone_assignments':sorted(set(len(v.groups) for v in o.data.vertices)),'uv_layers':len(o.data.uv_layers),'material':mat.name}
bm.to_mesh(o.data);bm.free();print(report);(D/'validation/topology.json').write_text(json.dumps(report,indent=2)+'\n');bpy.ops.wm.save_as_mainfile(filepath=str(D/'source.blend'),compress=True)
