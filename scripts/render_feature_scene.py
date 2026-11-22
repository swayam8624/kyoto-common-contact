#!/usr/bin/env python3
"""Kyoto Pass 3: scientific 3D mesh/feature visualization from native CSV.

Data are real C++23 *kinematic* trajectories, NOT numerical dynamics.
All candidate links are AABB broad-phase candidates, NOT verified CCD.
Scalar field: accumulated vertex displacement magnitude in world units,
NEVER strain/stress/pressure. No synthetic performance measurements.
"""
from __future__ import annotations
import argparse
import csv
import json
from pathlib import Path
from collections import defaultdict
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import cm, colors
from matplotlib.collections import LineCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection,Line3DCollection

BG="#071322";PANEL="#0b1a2b";FG="#edf4fd"
MUTED="#9ab2c6";GRID="#234058"
CYAN="#49d9e9";GOLD="#f7b774";MAGENTA="#cb93ed";GREEN="#6cdec1"
cm_displacement=plt.get_cmap("viridis")
def theme():
    plt.rcParams.update({"font.family":"DejaVu Sans","font.size":10,
       "figure.facecolor":BG,"axes.facecolor":BG,"savefig.facecolor":BG,
       "axes.labelcolor":MUTED,"axes.edgecolor":GRID,
       "text.color":FG,"xtick.color":MUTED,"ytick.color":MUTED,
       "grid.color":GRID,"grid.alpha":.2})
def axes3d(ax):
    ax.set_facecolor(BG)
    ax.set_xlim(-3.7,3.7);ax.set_ylim(-3.05,3.05);ax.set_zlim(-2.5,2.5)
    ax.set_box_aspect((7.4,6.1,5.))
    ax.view_init(elev=26,azim=-60)
    for a in (ax.xaxis,ax.yaxis,ax.zaxis):
        a.pane.set_facecolor((.03,.07,.12,1))
        a.pane.set_edgecolor(GRID)
        a._axinfo["grid"]["color"]=(.18,.25,.33,.22)
    ax.tick_params(labelsize=7)
    ax.set_xlabel("X / world units",labelpad=4,color=MUTED,fontsize=8)
    ax.set_ylabel("Y / world units",labelpad=4,color=MUTED,fontsize=8)
    ax.set_zlabel("Z / world units",labelpad=4,color=MUTED,fontsize=8)
def heading(fig,text,sub):
    fig.text(.05,.967,text,weight="bold",size=18,va="top")
    fig.text(.05,.922,sub,color=MUTED,size=9,va="top")
def signature(fig,sub):
    fig.text(.05,.022,sub,color=MUTED,size=8)
def save(fig,out,name):
    fig.savefig(out/name,dpi=170,bbox_inches="tight",pad_inches=.12)
    plt.close(fig)
def parse(inp):
    states=pd.read_csv(inp/"mesh_states.csv")
    face_data=pd.read_csv(inp/"faces.csv")
    edge_data=pd.read_csv(inp/"edges.csv")
    candidates=pd.read_csv(inp/"typed_candidates.csv")
    timeline=pd.read_csv(inp/"frame_metrics.csv")
    manifest=json.loads((inp/"scene_manifest.json").read_text())
    assert manifest["frames"]==61 and not manifest["continuous_CCD"]
    assert not manifest["physics_contact_response"]
    assert len(timeline)==61 and timeline.frame.min()==0 and timeline.frame.max()==60
    assert timeline.vertex_face_candidates.sum()+timeline.edge_edge_candidates.sum()\
        ==len(candidates)
    assert timeline.total_candidates.eq(
        timeline.vertex_face_candidates+timeline.edge_edge_candidates).all()
    assert states.vertex_id.nunique()==manifest["vertex_count"]
    assert len(face_data)==manifest["face_count"]
    assert len(edge_data)==manifest["edge_count"]
    frame_data={}
    for frame,df in states.groupby("frame"):
        frame_data[int(frame)]={int(r.vertex_id):
            (np.array([r.x,r.y,r.z],dtype=float),float(r.displacement))
            for r in df.itertuples(index=False)}
    face_map={int(r.face_id):(int(r.object_id),
                              (int(r.a),int(r.b),int(r.c)))
              for r in face_data.itertuples(index=False)}
    edge_map={int(r.edge_id):(int(r.a),int(r.b))
              for r in edge_data.itertuples(index=False)}
    return states,frame_data,face_map,edge_map,candidates,timeline,manifest

def geometry(fr,face_map,object_id):
    triangles=[]; scalars=[]
    for face,(obj,ids) in face_map.items():
        if obj!=object_id:continue
        triangles.append(np.array([fr[i][0] for i in ids]))
        scalars.append(np.mean([fr[i][1] for i in ids]))
    return triangles,np.array(scalars)
def render_object(ax,fr,faces,maximum,cage_alpha=.24):
    tri,disp=geometry(fr,faces,1)
    coll=Poly3DCollection(tri,facecolors=cm_displacement(
        np.clip(disp/max(maximum,1e-12),0,1)),edgecolors=CYAN,
        linewidths=.12,alpha=.92,zsort="average")
    ax.add_collection3d(coll)
    tri,_=geometry(fr,faces,2)
    coll=Poly3DCollection(tri,facecolors=GOLD,edgecolors=GOLD,
        linewidths=.25,alpha=cage_alpha,zsort="average")
    ax.add_collection3d(coll)

def montage(frame_data,faces,out):
    frames=[0,20,40,60]
    maxima=[v[1] for v in frame_data[60].values()]
    maximum=max(maxima)
    fig=plt.figure(figsize=(14.4,12))
    fig.subplots_adjust(left=.0,right=.99,bottom=.075,top=.87,
                        wspace=.0,hspace=.025)
    for i,f in enumerate(frames):
        ax=fig.add_subplot(2,2,i+1,projection="3d")
        axes3d(ax);render_object(ax,frame_data[f],faces,maximum)
        ax.set_title(f"T = {f/60:.2f}  /  FRAME {f:02}",color=FG,
                     fontsize=11,pad=10)
    heading(fig,"01  /  TWO INDEPENDENT 3D MESH FAMILIES",
            "A deforming toroidal shell passes a breathing triangulated cage. Source: native C++ vertex paths.")
    signature(fig,"Cyan/viridis: shell displacement in world units. Gold: cage. Kinematic test, not solver physics.")
    save(fig,out,"01_kinematic_3d_scene_montage.png")

def displacement(frame_data,faces,out):
    f=32
    maximum=max(v[1] for x in frame_data.values() for v in x.values())
    fig=plt.figure(figsize=(12.8,8.2))
    fig.subplots_adjust(left=.0,right=.87,bottom=.06,top=.85)
    ax=fig.add_subplot(111,projection="3d")
    axes3d(ax)
    render_object(ax,frame_data[f],faces,maximum,.13)
    scale=cm.ScalarMappable(norm=colors.Normalize(0,maximum),
                           cmap=cm_displacement)
    scale.set_array([])
    bar=fig.colorbar(scale,ax=ax,shrink=.52,pad=.03,aspect=17)
    bar.set_label("Vertex displacement |x(t)-x(0)| / world units",color=MUTED)
    heading(fig,"02  /  DISPLACEMENT FIELD ON ACTUAL MESH",
           "Surface-shaded per-face means of native C++ vertex displacement, not simulated strain or stress.")
    signature(fig,"Face colors are sampled from measured geometry displacements; fixed scalar range across all frames.")
    save(fig,out,"02_surface_displacement_field.png")

def sample_links(frame_data,faces,edges,candidates,frame,max_links=200):
    fr=frame_data[frame];rows=candidates[candidates.frame==frame]
    links=[]; kinds=[]
    if len(rows)>max_links:
        # Uniform deterministic stratified sampling across the ordered
        # source rows. Do not call this all contacts or hide the count.
        rows=rows.iloc[np.linspace(0,len(rows)-1,max_links,dtype=int)]
    for r in rows.itertuples(index=False):
        if r.type=="VF":
            if int(r.second) not in faces:continue
            ids=faces[int(r.second)][1]
            a=fr[int(r.first)][0]
            b=np.mean([fr[int(i)][0] for i in ids],axis=0)
        elif r.type=="EE":
            first=edges[int(r.first)];second=edges[int(r.second)]
            a=np.mean([fr[int(i)][0] for i in first],axis=0)
            b=np.mean([fr[int(i)][0] for i in second],axis=0)
        else:raise ValueError("unexpected candidate type")
        links.append([a,b]);kinds.append(r.type)
    return links,kinds,len(candidates[candidates.frame==frame])

def candidate_network(frames,faces,edges,candidates,out):
    f=30
    fr=frames[f]
    links,kinds,total=sample_links(frames,faces,edges,candidates,f,220)
    fig=plt.figure(figsize=(13,8.7))
    fig.subplots_adjust(top=.83,left=.0,right=.98,bottom=.08)
    ax=fig.add_subplot(111,projection="3d");axes3d(ax)
    render_object(ax,fr,faces,maximum=max(x[1] for x in fr.values()),cage_alpha=.10)
    for kind,col in (("VF",CYAN),("EE",MAGENTA)):
        segs=[l for l,k in zip(links,kinds) if k==kind]
        if segs:
            ax.add_collection3d(Line3DCollection(segs,colors=col,
                                                linewidths=.75,alpha=.55))
    heading(fig,"03  /  EMBEDDED FEATURE CANDIDATE NETWORK",
       f"Frame 30: {total:,} AABB candidates. Drawn: {len(links)} stratified links (vertex-face / edge-edge).")
    signature(fig,"Spatial endpoints are real feature centroids; links mean possible AABB overlap, NOT verified contact events.")
    save(fig,out,"03_embedded_feature_candidate_graph.png")

def timeline_figure(timeline,out):
    fig,ax=plt.subplots(figsize=(12,7))
    fig.subplots_adjust(top=.83,bottom=.16,left=.105,right=.95)
    ax.fill_between(timeline.time,0,timeline.vertex_face_candidates,
                    color=CYAN,alpha=.14)
    ax.plot(timeline.time,timeline.vertex_face_candidates,
            color=CYAN,lw=2.4,label="Vertex-face candidates")
    ax.plot(timeline.time,timeline.edge_edge_candidates,
            color=MAGENTA,lw=2.3,label="Edge-edge candidates")
    ax.plot(timeline.time,timeline.total_candidates,
            color=GOLD,lw=2.2,ls="--",label="Total candidate pairs")
    ax.set(xlabel="Normalized trajectory time",
           ylabel="Instantaneous AABB candidate count",xlim=(0,1))
    ax.legend(frameon=False,labelcolor=FG,loc="upper right")
    ax.grid(True);ax.set_axisbelow(True)
    heading(fig,"04  /  3D MESH CONTACT CANDIDATE EVOLUTION",
        "Actual feature-level sweep-and-prune output across 61 C++ trajectory frames.")
    signature(fig,"No CCD event count or collision forces inferred; this is solely broad-phase occupancy.")
    save(fig,out,"04_feature_candidate_timeline.png")

def density(frames,faces,edges,candidates,out):
    f=30
    links,kinds,total=sample_links(frames,faces,edges,candidates,f,2500)
    if links:
        points=np.mean(np.asarray(links),axis=1)
    else:points=np.zeros((0,3))
    bins=[np.linspace(-3.7,3.7,13),np.linspace(-3,3,12),np.linspace(-2.5,2.5,11)]
    counts,_=np.histogramdd(points,bins=bins)
    centers=np.meshgrid(*[(b[:-1]+b[1:])/2 for b in bins],indexing="ij")
    sel=counts>0
    fig=plt.figure(figsize=(12.4,8))
    fig.subplots_adjust(top=.83,left=.01,right=.9,bottom=.08)
    ax=fig.add_subplot(111,projection="3d");axes3d(ax)
    if sel.any():
        vals=counts[sel]
        scatter=ax.scatter(centers[0][sel],centers[1][sel],centers[2][sel],
           c=vals,cmap="magma",vmin=1,vmax=max(float(vals.max()),2),
           s=30+200*(vals/vals.max()),alpha=.75,depthshade=False)
        cb=fig.colorbar(scatter,ax=ax,shrink=.58,pad=.04)
        cb.set_label("Rendered candidate-link midpoints per voxel",color=MUTED)
    heading(fig,"05  /  3D CANDIDATE-DENSITY ATLAS",
       f"Frame 30: spatial histogram of {len(links):,} sampled links from {total:,} source candidates.")
    signature(fig,"Voxel count is a sampling visualization, not a contact-pressure, penetration, or force field.")
    save(fig,out,"05_3d_candidate_density_map.png")

def vtk_series(frame_data,faces,out):
    import xml.etree.ElementTree as ET
    folder=out/"vtk";folder.mkdir(exist_ok=True)
    ordered=sorted(frame_data[0])
    id_to_index={vid:i for i,vid in enumerate(ordered)}
    root=ET.Element("VTKFile",type="Collection",version="0.1",
                    byte_order="LittleEndian")
    collection=ET.SubElement(root,"Collection")
    for f in sorted(frame_data):
        name=f"mesh_{f:04}.vtk"
        with (folder/name).open("w") as dst:
            dst.write("# vtk DataFile Version 3.0\n")
            dst.write(f"Kyoto C++ kinematic surface t={f/60:.10f}\n")
            dst.write("ASCII\nDATASET POLYDATA\n")
            dst.write(f"POINTS {len(ordered)} double\n")
            for vid in ordered:
                p=frame_data[f][vid][0]
                dst.write("%.17g %.17g %.17g\n"%tuple(p))
            dst.write(f"POLYGONS {len(faces)} {4*len(faces)}\n")
            for obj,ids in faces.values():
                dst.write("3 %d %d %d\n"%tuple(id_to_index[i] for i in ids))
            dst.write(f"POINT_DATA {len(ordered)}\n")
            dst.write("SCALARS displacement_magnitude double 1\n")
            dst.write("LOOKUP_TABLE default\n")
            for vid in ordered:
                dst.write("%.17g\n"%frame_data[f][vid][1])
            dst.write("SCALARS object_id int 1\nLOOKUP_TABLE default\n")
            for vid in ordered:
                dst.write(str(1 if vid<2000 else 2)+"\n")
        ET.SubElement(collection,"DataSet",timestep=f"{f/60:.17g}",
                      group="",part="0",file=name)
    ET.indent(root,space="  ")
    ET.ElementTree(root).write(folder/"mesh_series.pvd",
                              encoding="utf-8",xml_declaration=True)

def interactive(frames,faces,metrics,out):
    import plotly.graph_objects as go
    ids_by_object={obj:sorted({v for _,(x,vertices) in faces.items()
                  if x==obj for v in vertices}) for obj in (1,2)}
    triangles_by_object={obj:[ids for _,(x,ids) in faces.items() if x==obj]
                         for obj in (1,2)}
    def geometry_trace(f,obj):
        ids=ids_by_object[obj]
        ix={id:k for k,id in enumerate(ids)}
        xyz=np.array([frames[f][i][0] for i in ids])
        cells=np.array([[ix[i] for i in tri] for tri in triangles_by_object[obj]])
        disp=np.array([frames[f][i][1] for i in ids])
        return go.Mesh3d(x=xyz[:,0],y=xyz[:,1],z=xyz[:,2],
          i=cells[:,0],j=cells[:,1],k=cells[:,2],
          intensity=disp if obj==1 else None,
          colorscale="Viridis",color=GOLD if obj==2 else None,
          cmin=0,cmax=3.3,opacity=.91 if obj==1 else .36,
          flatshading=False,name="Deforming toroidal shell" if obj==1
          else "Triangulated geodesic cage",showscale=obj==1,
          colorbar=dict(title="Displacement") if obj==1 else None)
    steps=list(range(0,61,3))
    snapshots=[go.Frame(name=str(f),data=[geometry_trace(f,1),
                  geometry_trace(f,2)]) for f in steps]
    fig=go.Figure(data=[geometry_trace(0,1),geometry_trace(0,2)],
                  frames=snapshots)
    fig.update_layout(
       title=("KYOTO SCA / MESH FEATURE OBSERVATORY"
       "<br><sup>61 C++ source-backed 3D kinematic states; no CCD or physical solver</sup>"),
       font=dict(color=FG),paper_bgcolor=BG,plot_bgcolor=BG,height=790,
       margin=dict(l=0,r=10,b=70,t=95),
       scene=dict(bgcolor=BG,aspectmode="data",
          xaxis=dict(range=[-3.7,3.7],title="X / world units",
                     gridcolor=GRID,backgroundcolor=BG),
          yaxis=dict(range=[-3.05,3.05],title="Y / world units",
                     gridcolor=GRID,backgroundcolor=BG),
          zaxis=dict(range=[-2.5,2.5],title="Z / world units",
                     gridcolor=GRID,backgroundcolor=BG),
          camera=dict(eye=dict(x=1.7,y=-2.1,z=1.15))),
       updatemenus=[dict(type="buttons",x=.02,y=-.06,
         buttons=[dict(label="▶ Play",method="animate",args=[
                     None,dict(frame=dict(duration=75,redraw=True),
                                fromcurrent=True)]),
                  dict(label="❚❚ Pause",method="animate",args=[
                     [None],dict(frame=dict(duration=0,redraw=False),
                                  mode="immediate")])])],
       sliders=[dict(x=.35,y=-.035,len=.61,currentvalue=dict(prefix="Frame: "),
        steps=[dict(method="animate",label=str(k),args=[
          [str(k)],dict(frame=dict(duration=0,redraw=True),
                        transition=dict(duration=0),mode="immediate")])
               for k in steps])])
    fig.write_html(out/"06_interactive_mesh_3d.html",full_html=True,
                   include_plotlyjs=True,auto_play=False)

def overview(out):
    from PIL import Image,ImageDraw,ImageFont
    names=["01_kinematic_3d_scene_montage.png","02_surface_displacement_field.png",
       "03_embedded_feature_candidate_graph.png","04_feature_candidate_timeline.png",
       "05_3d_candidate_density_map.png"]
    canvas=Image.new("RGB",(1900,1780),BG)
    spots=[(0,105,940,810),(960,105,940,810),
           (0,915,620,770),(640,915,620,770),(1280,915,620,770)]
    for name,(x,y,w,h) in zip(names,spots):
        pic=Image.open(out/name).convert("RGB")
        pic.thumbnail((w,h))
        canvas.paste(pic,(x+(w-pic.width)//2,y+(h-pic.height)//2))
    d=ImageDraw.Draw(canvas)
    try:
        f=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",35)
    except OSError:f=ImageFont.load_default()
    d.text((48,28),"KYOTO / PASS 03 — 3D MESH FEATURE LAB",fill=FG,font=f)
    d.text((48,76),"Source-backed shell + cage; vertex-face and edge-edge candidate anatomy",fill=MUTED)
    canvas.save(out/"STAGE3_MESH_FEATURE_ATLAS.png")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args();a.output.mkdir(exist_ok=True,parents=True)
    states,frames,faces,edges,candidates,metrics,manifest=parse(a.input)
    assert len(frames)==61
    theme()
    montage(frames,faces,a.output)
    displacement(frames,faces,a.output)
    candidate_network(frames,faces,edges,candidates,a.output)
    timeline_figure(metrics,a.output)
    density(frames,faces,edges,candidates,a.output)
    interactive(frames,faces,metrics,a.output)
    vtk_series(frames,faces,a.output)
    overview(a.output)
    print("PASS: 5 scientific figures, interactive 3D, 61 VTK frames, atlas")

if __name__=="__main__":main()
