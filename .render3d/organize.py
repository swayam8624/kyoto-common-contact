import bpy,json,pathlib,itertools
root=pathlib.Path('/Users/swayamsingal/Desktop/Programming/kyoto-common-contact');out=root/'output/research3d';bpy.ops.wm.open_mainfile(filepath=str(out/'Common_Broad_Phase_3D.blend'));sc=bpy.context.scene
for o in list(bpy.data.objects):
 if o.name.startswith(('C++ inspected','Triangle wire','Shared broad-phase')):bpy.data.objects.remove(o,do_unlink=True)
cloth=bpy.data.objects['Simulated cloth'];cloth.hide_viewport=False;cloth.hide_render=False
cols={}
for name in ['01 Physics replay','02 Candidates · frame 60','03 Proximity · frame 60']:
 c=bpy.data.collections.new(name);sc.collection.children.link(c);cols[name]=c
for c in list(cloth.users_collection):c.objects.unlink(cloth)
cols['01 Physics replay'].objects.link(cloth)
it=iter((out/'frame-060.mesh').read_text().split());nv=int(next(it));nt=int(next(it));sp=[float(next(it)) for _ in range(5)];v=[tuple(float(next(it)) for _ in range(3)) for _ in range(nv)];f=[tuple(int(next(it)) for _ in range(3)) for _ in range(nt)];q=json.loads((out/'query-060.json').read_text())
def material(name,col):
 m=bpy.data.materials.new(name);m.diffuse_color=(*col,1);m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(*col,1);return m
neutral=material('Analysis neutral',(.38,.45,.5));amber=material('C++ candidate mask',(.9,.34,.035));green=material('C++ precise mask',(.045,.5,.22));blue=material('Bound blue',(.025,.18,.9));tr=bpy.data.materials.get('Cutaway · omitted front quadrant')
for name,ids,col in [('02 Candidates · frame 60',set(q['candidateTriangleIds']),amber),('03 Proximity · frame 60',set(q['proximityTriangleIds']),green)]:
 m=bpy.data.meshes.new(name);m.from_pydata(v,[],f);m.update();o=bpy.data.objects.new(name,m);cols[name].objects.link(o)
 for a in (neutral,col,tr):m.materials.append(a)
 for idx,p in enumerate(m.polygons):p.material_index=2 if p.center.x>0 and p.center.y<.2 else 1 if idx in ids else 0;p.use_smooth=True
 pad=sp[3]+sp[4]/2;corners=[tuple(sp[k]+sign[k]*pad for k in range(3)) for sign in itertools.product((-1,1),repeat=3)];c=bpy.data.curves.new('Sphere query bound','CURVE');c.dimensions='3D';c.bevel_depth=.006;c.bevel_resolution=2
 for i,a in enumerate(corners):
  for b in corners[i+1:]:
   if sum(abs(a[k]-b[k])>1e-6 for k in range(3))==1:
    spline=c.splines.new('POLY');spline.points.add(1);spline.points[0].co=(*a,1);spline.points[1].co=(*b,1)
 box=bpy.data.objects.new('Sphere bound · '+name,c);cols[name].objects.link(box);c.materials.append(blue)
# Each native view layer selects one stage; common obstacle/camera/light objects remain shared.
for l in list(sc.view_layers)[1:]:sc.view_layers.remove(l)
first=sc.view_layers[0];first.name='01 Physics replay'
for name in ['02 Candidates · frame 60','03 Proximity · frame 60']:sc.view_layers.new(name)
for layer in sc.view_layers:
 for name in cols:layer.layer_collection.children[name].exclude=name!=layer.name
bpy.context.window.view_layer=sc.view_layers['01 Physics replay'];sc.frame_set(60)
for screen in bpy.data.screens:
 for area in screen.areas:
  if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Common_Broad_Phase_3D.blend'))
print('Native view layers:',[l.name for l in sc.view_layers]);print('Native 3D demo organized and saved')
