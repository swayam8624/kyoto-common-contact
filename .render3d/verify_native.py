import bpy,pathlib,math,json
root=pathlib.Path('/Users/swayamsingal/Desktop/Programming/kyoto-common-contact');out=root/'output/research3d';bpy.ops.wm.open_mainfile(filepath=str(out/'Common_Broad_Phase_3D.blend'));sc=bpy.context.scene;cloth=bpy.data.objects['Simulated cloth'];errs={}
for frame in (1,20,35,60,90):
 sc.frame_set(frame);bpy.context.view_layer.update();obj=cloth.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=obj.to_mesh();tokens=(out/f'frame-{frame:03}.mesh').read_text().split();n=int(tokens[0]);verts=[tuple(map(float,tokens[7+i*3:10+i*3])) for i in range(n)];err=max(math.dist(v.co,p) for v,p in zip(mesh.vertices,verts));assert len(mesh.vertices)==n and err<1e-5,(frame,err);errs[str(frame)]=err;obj.to_mesh_clear()
assert len(sc.view_layers)==3
(out/'native_replay_verification.json').write_text(json.dumps({'maxVertexErrorByFrame':errs,'viewLayers':[l.name for l in sc.view_layers],'passed':True},indent=2));print('NATIVE_REPLAY_VERIFIED',errs)
