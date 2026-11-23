#!/usr/bin/env python3
"""Pass 7A: whole-face-safe FEM → source-backed cinematic 3D VTK and volume cutaway.

Unlike earlier kinematic showcase scenes, each vertex position here comes
from the actual 900-step C++ tetrahedral neo-Hookean physical integration.
Color = real computed Cauchy von Mises stress (Pa). Red markers show
barycentrically distributed rigid-sphere impulse contributions,
NOT an IPC generalized triangle-mesh contact solve.
"""
from __future__ import annotations
import argparse,csv,gc,json
from collections import defaultdict
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import numpy as np
import vtk
from matplotlib import colormaps

BG="#071421";INK="#e7f1fa";MUTED="#9eb4c7"
def load(root):
    manifest=json.loads((root/"curved_manifest.json").read_text())
    if not manifest["whole_triangle_lipschitz_CCD"] and manifest["accepted_static_surface_no_penetration_verified"]:
        raise ValueError("will not render unverified kinematic source as physics")
    frames=defaultdict(dict)
    with (root/"curved_solid_states.csv").open(newline="") as file:
        for r in csv.DictReader(file):
            frames[int(r["frame"])][int(r["vertex_id"])]={
                "xyz":tuple(float(r[k]) for k in ("x","y","z")),
                "stress":float(r["stress_von_mises_pa"]),
                "disp":float(r["displacement_m"])}
    tets=[]
    with (root/"tetrahedra.csv").open(newline="") as file:
        for r in csv.DictReader(file):
            tets.append(tuple(int(r[k]) for k in ("a","b","c","d")))
    impacts=defaultdict(list)
    frame_duration=manifest["dt_s"]*manifest["stride"]
    with (root/"curved_contact_impulses.csv").open(newline="") as file:
        for r in csv.DictReader(file):
            t=float(r["time_s"])
            frame=min(manifest["frames"]-1,int(np.ceil(t/frame_duration-1e-9)))
            impacts[frame].append((
              float(r["x"]),float(r["y"]),float(r["z"]),
              float(r["impulse_ns"])))
    if len(frames)!=181 or len(tets)!=480:
        raise ValueError("missing connected volumetric physics source")
    return manifest,frames,tets,impacts

def font(size,bold=False):
    path="/usr/share/fonts/truetype/dejavu/DejaVuSans"
    try:return ImageFont.truetype(path+("-Bold" if bold else "")+".ttf",size)
    except OSError:return ImageFont.load_default()

def make_grid(state,tets):
    ids=sorted(state)
    points=vtk.vtkPoints();points.SetDataTypeToDouble()
    stress=vtk.vtkFloatArray();stress.SetName("von_mises_pa")
    displacement=vtk.vtkFloatArray();displacement.SetName("displacement_m")
    for i in ids:
        points.InsertNextPoint(*state[i]["xyz"])
        stress.InsertNextValue(state[i]["stress"])
        displacement.InsertNextValue(state[i]["disp"])
    grid=vtk.vtkUnstructuredGrid();grid.SetPoints(points)
    for ids4 in tets:
        cell=vtk.vtkTetra()
        for k,i in enumerate(ids4):cell.GetPointIds().SetId(k,i)
        grid.InsertNextCell(cell.GetCellType(),cell.GetPointIds())
    grid.GetPointData().SetScalars(stress)
    grid.GetPointData().AddArray(displacement)
    return grid

def stress_lut(maxstress):
    lut=vtk.vtkLookupTable()
    lut.SetNumberOfTableValues(256)
    lut.SetTableRange(0,maxstress)
    lut.Build()
    cmap=colormaps["viridis"]
    for i in range(256):
        col=cmap(i/255)
        lut.SetTableValue(i,float(col[0]),float(col[1]),float(col[2]),1)
    return lut

def curved_sphere(renderer,manifest):
    """Real static analytic collider radius and center from native manifest."""
    src=vtk.vtkSphereSource()
    src.SetCenter(*manifest["sphere_center_xyz_m"])
    src.SetRadius(float(manifest["sphere_radius_m"]))
    src.SetThetaResolution(80)
    src.SetPhiResolution(50)
    mapper=vtk.vtkPolyDataMapper()
    mapper.SetInputConnection(src.GetOutputPort())
    actor=vtk.vtkActor()
    actor.SetMapper(mapper)
    actor.GetProperty().SetColor(.93,.58,.27)
    actor.GetProperty().SetOpacity(.30)
    actor.GetProperty().SetSpecular(.5)
    actor.GetProperty().SetSpecularPower(30)
    renderer.AddActor(actor)
    wire=vtk.vtkActor()
    wire.SetMapper(mapper)
    wire.GetProperty().SetRepresentationToWireframe()
    wire.GetProperty().SetColor(1.,.77,.43)
    wire.GetProperty().SetLineWidth(.8)
    wire.GetProperty().SetOpacity(.22)
    renderer.AddActor(wire)

def impact_dots(renderer,impacts,frame):
    events=impacts.get(frame,[])
    if not events:return
    point=vtk.vtkPoints()
    for x,y,z,_ in events:point.InsertNextPoint(x,y,max(z,.002))
    poly=vtk.vtkPolyData();poly.SetPoints(point)
    glyph=vtk.vtkVertexGlyphFilter();glyph.SetInputData(poly);glyph.Update()
    mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(glyph.GetOutputPort())
    actor=vtk.vtkActor();actor.SetMapper(mapper)
    actor.GetProperty().SetColor(1,.36,.29)
    actor.GetProperty().SetPointSize(9)
    actor.GetProperty().SetRenderPointsAsSpheres(True)
    renderer.AddActor(actor)

def render(root,out,manifest,frames,tets,impacts,frame,cut=False,
           width=1500,height=960,prefix="studio"):
    grid=make_grid(frames[frame],tets)
    maximum=max(1.,float(manifest["peak_von_mises_pa"]))
    lookup=stress_lut(maximum)
    if cut:
        clipplane=vtk.vtkPlane()
        clipplane.SetOrigin(0.03,0,.3)
        clipplane.SetNormal(1,0,0)
        clip=vtk.vtkClipDataSet()
        clip.SetInputData(grid)
        clip.SetClipFunction(clipplane)
        clip.InsideOutOn()
        extractor=vtk.vtkDataSetSurfaceFilter()
        extractor.SetInputConnection(clip.GetOutputPort())
    else:
        extractor=vtk.vtkDataSetSurfaceFilter()
        extractor.SetInputData(grid)
    normals=vtk.vtkPolyDataNormals()
    normals.SetInputConnection(extractor.GetOutputPort())
    normals.AutoOrientNormalsOn();normals.ConsistencyOn();normals.SplittingOff()
    mapper=vtk.vtkPolyDataMapper()
    mapper.SetInputConnection(normals.GetOutputPort())
    mapper.SetLookupTable(lookup);mapper.SetScalarRange(0,maximum)
    actor=vtk.vtkActor();actor.SetMapper(mapper)
    actor.GetProperty().SetInterpolationToPhong()
    actor.GetProperty().SetSpecular(.46)
    actor.GetProperty().SetSpecularPower(30)
    renderer=vtk.vtkRenderer()
    renderer.SetBackground(.015,.035,.06)
    renderer.SetBackground2(.05,.095,.14)
    renderer.GradientBackgroundOn()
    renderer.AddActor(actor)
    edges=vtk.vtkExtractEdges()
    edges.SetInputConnection(normals.GetOutputPort())
    edge_mapper=vtk.vtkPolyDataMapper()
    edge_mapper.SetInputConnection(edges.GetOutputPort())
    edge_actor=vtk.vtkActor();edge_actor.SetMapper(edge_mapper)
    edge_actor.GetProperty().SetColor(.05,.11,.16)
    edge_actor.GetProperty().SetLineWidth(.8)
    edge_actor.GetProperty().SetOpacity(.18)
    renderer.AddActor(edge_actor)
    curved_sphere(renderer,manifest)
    impact_dots(renderer,impacts,frame)
    cam=vtk.vtkCamera()
    cam.SetFocalPoint(0,0,.10)
    cam.SetPosition(2.7,-3.8,2.10)
    cam.SetViewUp(0,0,1)
    cam.SetViewAngle(32)
    cam.Zoom(1.28)
    renderer.SetActiveCamera(cam)
    renderer.ResetCameraClippingRange()
    for pos,intensity,color in [
        ((4,-6,8),1.2,(.95,.98,1)),
        ((-4,3,7),.74,(.4,.75,1)),
        ((1,6,-2),.75,(1,.62,.34))]:
        light=vtk.vtkLight()
        light.SetPosition(*pos)
        light.SetFocalPoint(0,0,.4)
        light.SetIntensity(intensity)
        light.SetColor(*color)
        light.SetPositional(False)
        renderer.AddLight(light)
    rw=vtk.vtkRenderWindow();rw.SetOffScreenRendering(True)
    rw.AddRenderer(renderer)
    rw.SetSize(width,height)
    rw.Render()
    grab=vtk.vtkWindowToImageFilter()
    grab.SetInput(rw)
    grab.ReadFrontBufferOff()
    grab.Update()
    raw=out/"_current_frame.png"
    writer=vtk.vtkPNGWriter();writer.SetInputConnection(grab.GetOutputPort())
    writer.SetFileName(str(raw));writer.Write()
    im=Image.open(raw).convert("RGB")
    draw=ImageDraw.Draw(im,"RGBA")
    ratio=width/1500
    pad=int(50*ratio)
    top=int(105*ratio);bottom=int(108*ratio)
    draw.rectangle((0,0,width,top),fill=(5,15,26,226))
    draw.rectangle((0,height-bottom,width,height),fill=(5,15,26,226))
    draw.rectangle((pad,int(31*ratio),pad+int(5*ratio),
                    int(79*ratio)),fill=(64,221,189,255))
    draw.text((pad+int(22*ratio),int(27*ratio)),
       "SPACETIME CONTACT ATLAS / WHOLE-FACE FEM",
       font=font(int(28*ratio),True),fill=(236,246,253))
    draw.text((pad+int(22*ratio),int(68*ratio)),
       "Actual 3D neo-Hookean torus · curved rigid sphere · synchronized face-safe contact",
       font=font(int(15*ratio)),fill=(163,196,210))
    t=frame*manifest["dt_s"]*manifest["stride"]
    maxstress=max(v["stress"] for v in frames[frame].values())
    label="SECTION CUT" if cut else "FULL VOLUME"
    draw.text((pad,height-bottom+int(22*ratio)),
       f"{label}    |    t={t:.4f} s    |    frame {frame:03d}/180    |    peak nodal von Mises {maxstress:,.1f} Pa",
       font=font(int(17*ratio)),fill=(238,248,253))
    draw.text((pad,height-bottom+int(67*ratio)),
       "Viridis = computed von Mises (Pa). Red points = barycentric sphere impulse contributions; not IPC mesh CCD.",
       font=font(int(13*ratio)),fill=(165,196,210))
    dst=out/f"{prefix}_{frame:03d}{'_section' if cut else ''}.png"
    im.save(dst,optimize=True)
    im.close()
    raw.unlink()
    rw.Finalize()
    return dst

def montage(out,paths):
    images=[Image.open(p).convert("RGB") for p in paths]
    canvas=Image.new("RGB",(2000,1550),BG)
    for k,im in enumerate(images):
        im.thumbnail((985,690))
        x=k%2*1000+(1000-im.width)//2
        y=120+k//2*690+(690-im.height)//2
        canvas.paste(im,(x,y))
    draw=ImageDraw.Draw(canvas)
    draw.text((48,26),"KYOTO / PASS 07 — FULL-FACE SAFE VOLUMETRIC FEM",
              font=font(34,True),fill=INK)
    draw.text((48,79),"True neo-Hookean FEM, sphere contact, original stress and mass-weighted impulses",
              font=font(17),fill=MUTED)
    dst=out/"STAGE7_WHOLE_FACE_CINEMATIC_ATLAS.png"
    canvas.save(dst)
    for im in images:im.close()

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--gif",action="store_true",help="Render a short actual solver-state GIF")
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    m,states,tets,impacts=load(args.input)
    contact=min((f for f in sorted(impacts) if impacts[f]),default=70)
    chosen=sorted(set([0,contact,min(contact+16,180),180]))
    if len(chosen)!=4:
        chosen=[0,60,105,180]
    snapshots=[render(args.input,args.output,m,states,tets,impacts,f)
               for f in chosen]
    render(args.input,args.output,m,states,tets,impacts,chosen[2],cut=True,
           prefix="tetra_cutaway")
    montage(args.output,snapshots)
    if args.gif:
        frames=[]
        for f in range(0,181,6):
            path=render(args.input,args.output,m,states,tets,impacts,f,
                        width=900,height=576,prefix="motion")
            with Image.open(path) as im:
                frames.append(im.convert("P",palette=Image.Palette.ADAPTIVE,
                                         colors=128))
            path.unlink()
            gc.collect()
        frames[0].save(args.output/"WHOLE_FACE_FEM_MOTION.gif",
                       save_all=True,append_images=frames[1:],duration=100,
                       loop=0,optimize=True,disposal=2)
    print("PASS: C++ actual FEM states -> native VTK studio, cutaway, source-backed atlas")

if __name__=="__main__":main()
