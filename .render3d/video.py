import bpy,pathlib
root=pathlib.Path('/Users/swayamsingal/Desktop/Programming/kyoto-common-contact');out=root/'output/research3d';frames=out/'video_frames';frames.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(out/'Common_Broad_Phase_3D.blend'));sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=8;sc.cycles.use_denoising=True;sc.render.resolution_x=1024;sc.render.resolution_y=640;sc.render.resolution_percentage=100
for layer in sc.view_layers:layer.use=layer.name=='01 Physics replay'
for i,frame in enumerate(range(1,91,3)):
 sc.frame_set(frame);sc.render.filepath=str(frames/f'{i:03}.png');bpy.ops.render.render(write_still=True);print('VIDEO_FRAME',frame,flush=True)
