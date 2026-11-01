import bpy,math,json,pathlib
from mathutils import Vector
root=pathlib.Path('/Users/swayamsingal/Desktop/Programming/kyoto-common-contact');out=root/'output/research3d';out.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True);sc=bpy.context.scene;sc.frame_end=90;sc.render.fps=30

def mat(name,col,metal=0,rough=.5):
 m=bpy.data.materials.new(name);m.diffuse_color=(*col,1);m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(*col,1);n.inputs['Roughness'].default_value=rough;n.inputs['Metallic'].default_value=metal;return m
clothmat=mat('Cloth · blue',(.035,.30,.40),0,.53);sphereMat=mat('Obstacle · graphite',(.15,.19,.23),.2,.3);floorMat=mat('Studio floor',(.68,.72,.75),0,.65)
bpy.ops.mesh.primitive_uv_sphere_add(segments=64,ring_count=40,radius=.8,location=(0,0,.9));sphere=bpy.context.object;sphere.name='Fixed collision sphere';sphere.data.materials.append(sphereMat);bpy.ops.object.shade_smooth();sphere.modifiers.new('Collision','COLLISION');sphere.collision.thickness_outer=.012
bpy.ops.mesh.primitive_plane_add(size=200);floor=bpy.context.object;floor.name='Ground';floor.data.materials.append(floorMat);floor.modifiers.new('Collision','COLLISION')
N=35;verts=[];faces=[]
for j in range(N):
 for i in range(N):
  x=(i/(N-1)-.5)*3.25;y=(j/(N-1)-.5)*3.25;verts.append((x+.12,y,2.35+.025*x+.04*y))
for j in range(N-1):
 for i in range(N-1):
  a=j*N+i;faces.append((a,a+1,a+1+N,a+N))
mesh=bpy.data.meshes.new('Cloth grid');mesh.from_pydata(verts,[],faces);mesh.update();cloth=bpy.data.objects.new('Simulated cloth',mesh);sc.collection.objects.link(cloth);cloth.data.materials.append(clothmat)
bpy.context.view_layer.objects.active=cloth;cloth.select_set(True);mod=cloth.modifiers.new('Physical cloth simulation','CLOTH');mod.settings.quality=8;mod.settings.mass=.25;mod.settings.tension_stiffness=25;mod.settings.compression_stiffness=25;mod.settings.shear_stiffness=15;mod.settings.bending_stiffness=.35;mod.collision_settings.collision_quality=5;mod.collision_settings.distance_min=.012;mod.collision_settings.use_self_collision=True;mod.collision_settings.self_distance_min=.012;mod.point_cache.frame_end=90
for p in cloth.data.polygons:p.use_smooth=True
world=bpy.data.worlds.new('Studio');sc.world=world;world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.25,.29,.34,1);world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name,loc,power,size in [('Key',(1,-3,7),1600,5),('Fill',(-4,-1,4),900,4),('Rim',(2,4,5),1800,3)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);sc.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4.7,-6.2,4.5));cam=bpy.context.object;cam.name='Research camera';cam.rotation_euler=(Vector((0,0,1))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=5;sc.camera=cam
sc.render.engine='CYCLES';sc.cycles.samples=32;sc.cycles.use_denoising=True;sc.render.resolution_x=1600;sc.render.resolution_y=1000;sc.render.resolution_percentage=100;sc.render.image_settings.file_format='PNG';sc.view_settings.view_transform='AgX'
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Cloth_Simulation_Source.blend'))
allpos=[];triangles=None
for frame in range(1,91):
 sc.frame_set(frame);bpy.context.view_layer.update();ev=cloth.evaluated_get(bpy.context.evaluated_depsgraph_get());em=ev.to_mesh();em.calc_loop_triangles();pos=[list(cloth.matrix_world@v.co) for v in em.vertices];allpos.append(pos)
 if triangles is None:triangles=[list(t.vertices) for t in em.loop_triangles]
 if frame in (1,20,35,60,90):
  with (out/f'frame-{frame:03}.mesh').open('w') as f:
   f.write(f'{len(pos)} {len(triangles)}\n0 0 .9 .8 .05\n');f.writelines(' '.join(map(str,p))+'\n' for p in pos);f.writelines(' '.join(map(str,t))+'\n' for t in triangles)
 ev.to_mesh_clear()
 print('SIMULATION_FRAME',frame,flush=True)
# Bake real physics output into native shape keys for self-contained offline playback.
cloth.modifiers.remove(mod)
for v,p in zip(cloth.data.vertices,allpos[0]):v.co=p
basis=cloth.shape_key_add(name='Basis')
for idx,pos in enumerate(allpos):
 key=cloth.shape_key_add(name=f'Physics frame {idx+1:03}')
 for v,p in zip(key.data,pos):v.co=p
 for frame,value in [(max(1,idx),0),(idx+1,1),(min(90,idx+2),0)]:key.value=value;key.keyframe_insert(data_path='value',frame=frame)
# Set each key explicitly at all three points, accounting for endpoint duplicates.
for idx,key in enumerate(list(cloth.data.shape_keys.key_blocks)[1:]):
 key.value=0
 if idx==0:key.value=1;key.keyframe_insert(data_path='value',frame=1);key.value=0;key.keyframe_insert(data_path='value',frame=2)
 if idx==89:key.value=0;key.keyframe_insert(data_path='value',frame=89);key.value=1;key.keyframe_insert(data_path='value',frame=90)
sc.frame_set(60)
# Triangulate replay mesh so C++ triangle IDs map exactly to render face indices.
tri=cloth.modifiers.new('Triangle primitive mapping','TRIANGULATE')
bpy.context.view_layer.objects.active=cloth
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Common_Broad_Phase_3D.blend'))
(root/'.render3d/replay-base.blend').write_bytes((out/'Common_Broad_Phase_3D.blend').read_bytes())
(out/'simulation_manifest.json').write_text(json.dumps({'source':'Blender cloth physics','frames':90,'fps':30,'vertices':len(verts),'triangles':len(triangles),'sphere':[0,0,.9,.8],'margin':.05,'cache':'native shape key replay of evaluated cloth positions','solverAdapter':'not integrated; C++ queries evaluated mesh snapshots'},indent=2))
print('SIMULATION_COMPLETE',flush=True)
