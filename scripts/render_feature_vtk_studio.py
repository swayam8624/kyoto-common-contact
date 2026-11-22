#!/usr/bin/env python3
"""Publication-quality offscreen VTK studio for native Kyoto mesh traces.

Input: *only* the C++ feature_scene_export CSVs. This renders actual 3D
positions, topology, and broad-phase candidate links. No physics is inferred.
Optional: pip install vtk numpy pillow matplotlib. VTK rendering requires an
offscreen EGL/Cocoa/OSMesa-capable backend.
"""
from __future__ import annotations
import argparse,csv
from collections import defaultdict
from pathlib import Path
import numpy as np
import vtk
from matplotlib import colormaps
from PIL import Image,ImageDraw,ImageFont

def load(root):
    frames=defaultdict(dict)
    with (root/"mesh_states.csv").open(newline="") as f:
        for row in csv.DictReader(f):
            frames[int(row["frame"])][int(row["vertex_id"])]=(
                tuple(float(row[k]) for k in ("x","y","z")),
                float(row["displacement"]))
    faces=[]
    with (root/"faces.csv").open(newline="") as f:
        for r in csv.DictReader(f):
            faces.append((int(r["face_id"]),int(r["object_id"]),
                          (int(r["a"]),int(r["b"]),int(r["c"]))))
    edges={}
    with (root/"edges.csv").open(newline="") as f:
        for r in csv.DictReader(f):
            edges[int(r["edge_id"])]=(int(r["a"]),int(r["b"]))
    links=defaultdict(list)
    with (root/"typed_candidates.csv").open(newline="") as f:
        for r in csv.DictReader(f):
            links[int(r["frame"])].append((r["type"],
                    int(r["first"]),int(r["second"])))
    if sorted(frames)!=list(range(61)):
        raise ValueError("Missing native C++ source frames 0 through 60")
    return frames,faces,edges,links

def font(size,bold=False):
    p="/usr/share/fonts/truetype/dejavu/DejaVuSans-"
    try:
        return ImageFont.truetype(p+("Bold" if bold else "")+".ttf",size)
    except OSError:
        return ImageFont.load_default()

def surf_poly(frames,faces,frame,obj):
    mesh=frames[frame]
    ids=sorted({x for _,o,verts in faces if o==obj for x in verts})
    to_index={id:i for i,id in enumerate(ids)}
    points=vtk.vtkPoints()
    points.SetDataTypeToDouble()
    scalars=vtk.vtkFloatArray()
    scalars.SetName("displacement_magnitude")
    cells=vtk.vtkCellArray()
    for id in ids:
        points.InsertNextPoint(*mesh[id][0])
        scalars.InsertNextValue(mesh[id][1])
    for _,o,verts in faces:
        if o!=obj:continue
        cell=vtk.vtkTriangle()
        for k,id in enumerate(verts):
            cell.GetPointIds().SetId(k,to_index[id])
        cells.InsertNextCell(cell)
    data=vtk.vtkPolyData()
    data.SetPoints(points);data.SetPolys(cells)
    data.GetPointData().SetScalars(scalars)
    normals=vtk.vtkPolyDataNormals()
    normals.SetInputData(data)
    normals.ConsistencyOn();normals.AutoOrientNormalsOn()
    normals.SplittingOff();normals.Update()
    return normals.GetOutput()

def lut():
    lookup=vtk.vtkLookupTable()
    lookup.SetNumberOfTableValues(256)
    lookup.SetTableRange(0,3.4)
    lookup.Build()
    for i in range(256):
        c=colormaps["viridis"](i/255)
        lookup.SetTableValue(i,float(c[0]),float(c[1]),float(c[2]),1)
    return lookup

def surface(renderer,frames,faces,frame):
    torus=vtk.vtkPolyDataMapper()
    torus.SetInputData(surf_poly(frames,faces,frame,1))
    torus.SetLookupTable(lut())
    torus.SetScalarRange(0,3.4)
    shell=vtk.vtkActor();shell.SetMapper(torus)
    shell.GetProperty().SetInterpolationToPhong()
    shell.GetProperty().SetSpecular(.47)
    shell.GetProperty().SetSpecularPower(40)
    renderer.AddActor(shell)
    cage_mapper=vtk.vtkPolyDataMapper()
    cage_mapper.SetInputData(surf_poly(frames,faces,frame,2))
    cage_mapper.ScalarVisibilityOff()
    glass=vtk.vtkActor();glass.SetMapper(cage_mapper)
    glass.GetProperty().SetColor(.95,.52,.24)
    glass.GetProperty().SetOpacity(.20)
    renderer.AddActor(glass)
    cage=vtk.vtkActor();cage.SetMapper(cage_mapper)
    cage.GetProperty().SetRepresentationToWireframe()
    cage.GetProperty().SetLineWidth(1.4)
    cage.GetProperty().SetColor(1,.72,.33)
    cage.GetProperty().SetOpacity(.72)
    renderer.AddActor(cage)

def spatial_grid(renderer):
    points=vtk.vtkPoints();lines=vtk.vtkCellArray()
    def line(a,b):
        x=points.InsertNextPoint(*a)
        y=points.InsertNextPoint(*b)
        cell=vtk.vtkLine()
        cell.GetPointIds().SetId(0,x)
        cell.GetPointIds().SetId(1,y)
        lines.InsertNextCell(cell)
    for v in np.arange(-5,5.1,.5):
        line((v,-4,-2.48),(v,4,-2.48))
    for v in np.arange(-4,4.1,.5):
        line((-5,v,-2.48),(5,v,-2.48))
    poly=vtk.vtkPolyData()
    poly.SetPoints(points);poly.SetLines(lines)
    mapper=vtk.vtkPolyDataMapper();mapper.SetInputData(poly)
    actor=vtk.vtkActor();actor.SetMapper(mapper)
    actor.GetProperty().SetColor(.12,.22,.30)
    actor.GetProperty().SetOpacity(.32)
    renderer.AddActor(actor)

def candidate_links(renderer,frames,faces,edges,candidates,frame):
    by_id={fid:verts for fid,_,*verts in faces}
    scene=frames[frame];all_candidates=candidates[frame]
    count=min(len(all_candidates),220)
    subset=[all_candidates[i] for i in
            np.linspace(0,len(all_candidates)-1,count,dtype=int)]
    for kind,color in (("VF",(.4,.87,1)),("EE",(1,.45,.75))):
        pts=vtk.vtkPoints();cells=vtk.vtkCellArray()
        for category,a,b in subset:
            if category!=kind:continue
            if kind=="VF":
                x=np.asarray(scene[a][0])
                y=np.mean([scene[v][0] for v in by_id[b]],axis=0)
            else:
                x=np.mean([scene[v][0] for v in edges[a]],axis=0)
                y=np.mean([scene[v][0] for v in edges[b]],axis=0)
            i=pts.InsertNextPoint(*x);j=pts.InsertNextPoint(*y)
            l=vtk.vtkLine();l.GetPointIds().SetId(0,i);l.GetPointIds().SetId(1,j)
            cells.InsertNextCell(l)
        lines=vtk.vtkPolyData();lines.SetPoints(pts);lines.SetLines(cells)
        mapper=vtk.vtkPolyDataMapper();mapper.SetInputData(lines)
        actor=vtk.vtkActor();actor.SetMapper(mapper)
        actor.GetProperty().SetColor(*color)
        actor.GetProperty().SetLineWidth(2)
        actor.GetProperty().SetOpacity(.36)
        renderer.AddActor(actor)

def hud(raw,out,frame,candidates):
    im=Image.open(raw).convert("RGB")
    width,height=im.size
    draw=ImageDraw.Draw(im,"RGBA")
    draw.rectangle((0,0,width,112),fill=(4,13,23,220))
    draw.rectangle((0,height-102,width,height),fill=(4,13,23,220))
    draw.rectangle((48,42,54,78),fill=(72,228,204,255))
    draw.text((74,24),"SPACETIME CONTACT ATLAS / MESHTOP 03",
              font=font(30,True),fill=(230,244,252))
    draw.text((74,66),
              "Source-backed 3D feature candidates; kinematic geometry, not physics",
              font=font(17),fill=(153,193,211))
    pairs=candidates[frame];vf=sum(k=="VF" for k,_,_ in pairs)
    draw.text((48,height-80),
       f"FRAME {frame:02d}/60   t={frame/60:.3f}   CANDIDATES {len(pairs):,}   VF {vf:,}   EE {len(pairs)-vf:,}",
       font=font(18),fill=(237,249,255))
    draw.text((48,height-46),
       "Viridis = vertex displacement |x(t)-x(0)|; candidate lines are AABB overlap, NOT CCD contacts.",
       font=font(14),fill=(155,190,209))
    im.save(out,optimize=True)

def render(root,out,frames,faces,edges,links,frame,show_graph):
    renderer=vtk.vtkRenderer()
    renderer.SetBackground(.015,.029,.053)
    renderer.SetBackground2(.027,.065,.095)
    renderer.GradientBackgroundOn()
    spatial_grid(renderer)
    surface(renderer,frames,faces,frame)
    if show_graph:candidate_links(renderer,frames,faces,edges,links,frame)
    camera=vtk.vtkCamera()
    camera.SetFocalPoint(0,0,0)
    camera.SetPosition(6.7,-10.3,6.0)
    camera.SetViewUp(0,0,1)
    camera.SetViewAngle(34)
    camera.Zoom(1.27)
    renderer.SetActiveCamera(camera)
    renderer.ResetCameraClippingRange()
    for pos,intensity,color in [
        ((5,-6,9),1.2,(.9,.95,1)),
        ((-6,3,7),.8,(.3,.7,1)),
        ((0,9,-2),.9,(1,.66,.38))]:
        light=vtk.vtkLight()
        light.SetPosition(*pos)
        light.SetFocalPoint(0,0,0)
        light.SetIntensity(intensity)
        light.SetColor(*color)
        light.SetPositional(False)
        renderer.AddLight(light)
    window=vtk.vtkRenderWindow()
    window.SetOffScreenRendering(True)
    window.SetSize(1500,960)
    window.AddRenderer(renderer)
    window.Render()
    capture=vtk.vtkWindowToImageFilter()
    capture.SetInput(window)
    capture.ReadFrontBufferOff()
    capture.Update()
    raw=out/"_studio_tmp.png"
    writer=vtk.vtkPNGWriter()
    writer.SetInputConnection(capture.GetOutputPort())
    writer.SetFileName(str(raw));writer.Write()
    dst=out/f"studio_frame_{frame:02d}.png"
    hud(raw,dst,frame,links)
    raw.unlink()
    window.Finalize()
    return dst

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    frames,faces,edges,candidates=load(a.input)
    for frame in (0,30,60):
        print(render(a.input,a.output,frames,faces,edges,candidates,
                     frame,show_graph=(frame==30)))

if __name__=="__main__":
    main()
