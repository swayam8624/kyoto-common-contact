import bpy,json,pathlib,itertools
from mathutils import Vector
root=pathlib.Path('/Users/swayamsingal/Desktop/Programming/kyoto-common-contact');out=root/'output/research3d'
bpy.ops.wm.open_mainfile(filepath=str(root/'.render3d/replay-base.blend'));sc=bpy.context.scene
cloth=bpy.data.objects.get('Simulated cloth');cloth.hide_render=True;cloth.hide_viewport=True

def mat(name,col,metal=0,rough=.5):
 m=bpy.data.materials.new(name);m.diffuse_color=(*col,1);m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(*col,1);n.inputs['Roughness'].default_value=rough;n.inputs['Metallic'].default_value=metal;return m
teal=mat('Simulated mesh',(.025,.25,.34),.12,.45);amber=mat('Broad-phase candidate',(.9,.34,.035),.08,.4);green=mat('Verified within margin',(.045,.5,.22),.06,.4);muted=mat('Other triangles',(.38,.45,.50),.1,.5);wiremat=mat('Mesh topology',(.025,.07,.09),.05,.5);blue=mat('Query AABB',(.025,.18,.9),.2,.3)
def curve(name,edges,material,radius):
 c=bpy.data.curves.new(name,'CURVE');c.dimensions='3D';c.bevel_depth=radius;c.bevel_resolution=2
 for a,b in edges:
  sp=c.splines.new('POLY');sp.points.add(1);sp.points[0].co=(*a,1);sp.points[1].co=(*b,1)
 o=bpy.data.objects.new(name,c);sc.collection.objects.link(o);o.data.materials.append(material);return o
frame=60
raw=(out/f'frame-{frame:03}.mesh').read_text().split();it=iter(raw);nv=int(next(it));nt=int(next(it));params=[float(next(it)) for _ in range(5)];v=[tuple(float(next(it)) for _ in range(3)) for _ in range(nv)];f=[tuple(int(next(it)) for _ in range(3)) for _ in range(nt)];q=json.loads((out/f'query-{frame:03}.json').read_text())
m=bpy.data.meshes.new('Exact exported cloth triangles');m.from_pydata(v,[],f);m.update();o=bpy.data.objects.new('C++ inspected snapshot',m);sc.collection.objects.link(o)
for material in (teal,amber,green,muted):m.materials.append(material)
for p in m.polygons:p.use_smooth=True
# Fine wire makes actual triangle topology readable, independent of interpolated shading.
meshwire=o.copy();meshwire.data=m.copy();meshwire.name='Triangle wire overlay';sc.collection.objects.link(meshwire);meshwire.data.materials.clear();meshwire.data.materials.append(wiremat)
wm=meshwire.modifiers.new('Triangle edges','WIREFRAME');wm.thickness=.0009;wm.use_replace=True;wm.use_even_offset=True
pad=params[3]+params[4]/2;center=params[:3];corners=[tuple(center[k]+sign[k]*pad for k in range(3)) for sign in itertools.product((-1,1),repeat=3)];edges=[]
for i,a in enumerate(corners):
 for b in corners[i+1:]:
  if sum(abs(a[k]-b[k])>1e-6 for k in range(3))==1:edges.append((a,b))
box=curve('Shared broad-phase sphere bound',edges,blue,.006)
transparent=bpy.data.materials.new('Cutaway · omitted front quadrant');transparent.use_nodes=True
nodes=transparent.node_tree.nodes;nodes.clear();tr=nodes.new('ShaderNodeBsdfTransparent');op=nodes.new('ShaderNodeOutputMaterial');transparent.node_tree.links.new(tr.outputs[0],op.inputs['Surface']);m.materials.append(transparent);meshwire.data.materials.append(transparent)
sc.world.node_tree.nodes['Background'].inputs[1].default_value=.18
for light in bpy.data.lights:light.energy*=.38
sc.view_settings.exposure=-.3
try:sc.view_settings.look='AgX - Medium High Contrast'
except:pass
sc.render.resolution_x=1800;sc.render.resolution_y=1125;sc.cycles.samples=40
# Full perspective with physically computed drape and studio illumination.
sc.camera.data.type='PERSP';sc.camera.data.lens=52;sc.camera.location=(4.6,-6.3,4.0);sc.camera.rotation_euler=(Vector((0,0,.9))-sc.camera.location).to_track_quat('-Z','Y').to_euler()
for stage in ('geometry','candidates','proximity'):
 box.hide_render=stage=='geometry'
 for idx,p in enumerate(m.polygons):
  p.material_index=4 if stage!='geometry' and p.center.x>0 and p.center.y<.2 else 0 if stage=='geometry' else 1 if stage=='candidates' and idx in q['candidateTriangleIds'] else 2 if stage=='proximity' and idx in q['proximityTriangleIds'] else 3
 for p in meshwire.data.polygons:p.material_index=1 if stage!='geometry' and p.center.x>0 and p.center.y<.2 else 0
 sc.render.filepath=str(out/f'{stage}.png');bpy.ops.render.render(write_still=True)
 print('RENDERED',stage,flush=True)
# Store analysis overlays alongside the physical mesh replay in the final native scene.
box.hide_render=True;box.hide_viewport=True;o.hide_render=True;o.hide_viewport=True;meshwire.hide_render=True;meshwire.hide_viewport=True;cloth.hide_render=False;cloth.hide_viewport=False
cloth.data.materials[0]=teal
for screen in bpy.data.screens:
 for area in screen.areas:
  if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
sc.frame_set(60);bpy.ops.wm.save_as_mainfile(filepath=str(out/'Common_Broad_Phase_3D.blend'))
print('RENDER_COMPLETE',flush=True)
