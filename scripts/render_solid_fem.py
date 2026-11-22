#!/usr/bin/env python3
"""Stage-5 scientific figures and 3D volumetric outputs from ACTUAL C++ FEM.

The physical state is integrated by include/kyoto/solid_dynamics.hpp.
All plots come exclusively from its native CSVs, including measured
J, von Mises stress, energies and true one-sided rigid-plane impulses.
No invented strain/pressure/contact-force fields or scripted trajectories.
"""
from __future__ import annotations
import argparse,csv,json
from collections import defaultdict
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BG="#081524";PANEL="#102538";FG="#eaf5fb";MUTED="#9bb6c8";GRID="#2e4558"
CYAN="#54d9ee";AMBER="#f2b76f";GREEN="#60e2ac";MAGENTA="#bf93f4"
RED="#fb7b8a"
def theme():
    plt.rcParams.update({
      "font.family":"DejaVu Sans","font.size":10,"figure.facecolor":BG,
      "axes.facecolor":PANEL,"savefig.facecolor":BG,"axes.edgecolor":GRID,
      "axes.labelcolor":MUTED,"text.color":FG,"xtick.color":MUTED,
      "ytick.color":MUTED,"grid.color":GRID,"grid.alpha":.3})
def headline(fig,title,subtitle,footer):
    fig.text(.055,.962,title,size=17,weight="bold",va="top")
    fig.text(.055,.919,subtitle,size=9,color=MUTED,va="top")
    fig.text(.055,.03,footer,size=8,color=MUTED)
def save(fig,out,name):
    fig.savefig(out/name,dpi=180,bbox_inches="tight",pad_inches=.15)
    plt.close(fig)
def load(root):
    manifest=json.loads((root/"solid_manifest.json").read_text())
    assert manifest["rendered_states_are_solver_generated"] is True
    assert manifest["material_model"]=="compressible_neo_hookean_log_J"
    assert manifest["general_mesh_ccd"] is False
    metrics=pd.read_csv(root/"solid_metrics.csv")
    states=pd.read_csv(root/"solid_states.csv")
    faces=pd.read_csv(root/"solid_surface_faces.csv")
    tets=pd.read_csv(root/"solid_tetrahedra.csv")
    impacts=pd.read_csv(root/"floor_impacts.csv")
    assert len(metrics)==manifest["frames"] and len(metrics)==181
    assert len(states)==len(metrics)*manifest["vertices"]
    assert len(tets)==manifest["tetrahedra"]
    assert len(faces)==manifest["surface_triangles"]
    assert manifest["impact_events"]==len(impacts)>0
    assert (metrics.min_height_m>=-1e-11).all()
    assert (metrics.min_tetra_jacobian>0).all()
    assert (states.von_mises_pa>=0).all()
    assert (states.mass_kg>0).all()
    assert (metrics.total_body_mass_kg>0).all()
    return manifest,metrics,states,faces,tets,impacts

def first_contact(metrics,impacts):
    return float(impacts.time_s.min())

def energy(m,d,out):
    fig,ax=plt.subplots(figsize=(12.8,7.2))
    fig.subplots_adjust(left=.09,right=.96,bottom=.15,top=.82)
    for key,color,title in [
        ("kinetic_j",CYAN,"Kinetic energy"),
        ("elastic_j",MAGENTA,"Stored hyperelastic strain energy"),
        ("gravitational_j",AMBER,"Gravitational potential"),
        ("mechanical_total_j",GREEN,"Total mechanical")]:
        ax.plot(m.time_s,m[key],color=color,lw=2.2,label=title)
    ct=first_contact(m,d)
    ax.axvline(ct,color=RED,lw=1.5,ls="--",alpha=.8)
    ax.annotate(f"First ground contact\n{ct:.3f} s",
                (ct,.93),xycoords=("data","axes fraction"),
                xytext=(12,-16),textcoords="offset points",
                color=RED,fontsize=10)
    ax.set(xlim=(0,float(m.time_s.max())),xlabel="Integrated physical time (seconds)",
           ylabel="Integrated mechanical energy (joules)")
    ax.grid(True);ax.legend(frameon=False,labelcolor=FG,ncol=2,fontsize=9)
    headline(fig,"01  /  REAL FEM ENERGY EXCHANGE",
       "Free 3D tetrahedral torus · gravity → kinetic → elastic deformation → impact losses",
       "Explicit semi-implicit dynamics; contact restitution, friction and damping are dissipative.")
    save(fig,out,"01_actual_fem_energy_budget.png")

def material(m,out):
    fig,ax=plt.subplots(figsize=(12.8,7.1))
    fig.subplots_adjust(left=.10,right=.90,bottom=.15,top=.82)
    p=ax.plot(m.time_s,m.max_von_mises_pa,color=CYAN,lw=2.7,
              label="Maximum Cauchy von Mises stress")[0]
    ax.set(xlabel="Integrated physical time (seconds)",
           ylabel="Maximum von Mises stress (Pa)")
    ax.grid(True)
    bx=ax.twinx()
    bx.plot(m.time_s,m.min_tetra_jacobian,color=AMBER,lw=2,
            label="Minimum element Jacobian J")
    bx.axhline(1,color=AMBER,lw=1,ls=":",alpha=.5)
    bx.set_ylabel("min det(F), dimensionless",color=AMBER)
    bx.tick_params(axis="y",colors=AMBER)
    ax.legend(handles=[p,bx.lines[0]],frameon=False,labelcolor=FG,
              loc="upper right")
    headline(fig,"02  /  STRESS & VOLUMETRIC DEFORMATION",
       "Stress computed from the compressible neo-Hookean first Piola tensor; J measures local volume change.",
       "All tetrahedra are verified positive-J; this is a numerical reference, not calibrated elastomer data.")
    save(fig,out,"02_stress_and_jacobian.png")

def contact(m,impacts,out):
    fig,ax=plt.subplots(figsize=(12.8,7.0))
    fig.subplots_adjust(left=.10,right=.88,bottom=.15,top=.82)
    width=.0036
    ax.bar(m.time_s,m.normal_contact_impulse_ns_this_interval,
           width=width,color=GREEN,label="Contact normal impulse / interval")
    ax.set(xlabel="Integrated physical time (seconds)",
           ylabel="Normal impulse per export interval (N·s)")
    ax.grid(axis="y")
    bx=ax.twinx()
    bx.plot(m.time_s,m.impact_events_this_frame_interval,color=AMBER,
            alpha=.75,lw=1.4,label="Impact events in interval")
    bx.set_ylabel("Number of vertex contact impulses",color=AMBER)
    bx.tick_params(axis="y",colors=AMBER)
    ax.legend(handles=[ax.patches[0],bx.lines[0]],
              labels=["Normal impulse","Contact events"],
              frameon=False,labelcolor=FG)
    headline(fig,"03  /  ACTUAL RIGID-PLANE CONTACT IMPULSES",
       "Native one-sided floor response summed over five integration steps per exported frame.",
       "Impulse (N·s) is real solver output; this is NOT an IPC triangle-mesh contact force.")
    save(fig,out,"03_contact_impulse_timeline.png")

def ground_map(impacts,out):
    x=impacts.x_m.to_numpy();y=impacts.y_m.to_numpy()
    weights=impacts.normal_impulse_ns.to_numpy()
    lim=max(1.,np.max(np.abs(np.r_[x,y]))*1.08)
    bins=np.linspace(-lim,lim,49)
    H,xe,ye=np.histogram2d(x,y,bins=[bins,bins],weights=weights)
    fig,ax=plt.subplots(figsize=(9.8,8.0))
    fig.subplots_adjust(left=.12,right=.88,bottom=.13,top=.81)
    pic=ax.pcolormesh(xe,ye,H.T,cmap="magma",shading="auto")
    ax.set_aspect("equal")
    ax.set(xlabel="Ground contact X (m)",ylabel="Ground contact Y (m)")
    bar=fig.colorbar(pic,ax=ax,pad=.03)
    bar.set_label("Summed normal impulses in spatial bin (N·s)",color=MUTED)
    ax.grid(alpha=.12)
    headline(fig,"04  /  PHYSICAL CONTACT FOOTPRINT",
       "XY histogram weighted by actual computed rigid-plane normal impulse events.",
       "Bin sums are impulses, NOT calibrated pressure or area-normalized traction.")
    save(fig,out,"04_spatial_contact_impulse_map.png")

def com(m,impacts,manifest,out):
    t=m.time_s.to_numpy()
    z0=float(m.center_z.iloc[0])
    gravity=float(manifest["gravity_m_s2"])
    ballistic=z0-.5*gravity*t*t
    fig,ax=plt.subplots(figsize=(12.8,6.9))
    fig.subplots_adjust(left=.105,right=.96,bottom=.16,top=.82)
    ax.plot(t,m.center_z,color=CYAN,lw=3,label="Solver center of mass Z")
    ax.plot(t,ballistic,color=AMBER,lw=2,ls="--",
            label="Ideal ballistic COM without floor/damping")
    ax.plot(t,m.min_height_m,color=GREEN,lw=1.8,
            label="Lowest vertex height")
    ax.axhline(0,color=RED,lw=1.5)
    ax.axvline(first_contact(m,impacts),color=RED,ls=":",alpha=.8)
    ax.set(xlabel="Integrated physical time (seconds)",
           ylabel="Height above rigid floor (m)")
    ax.grid(True);ax.legend(frameon=False,labelcolor=FG)
    headline(fig,"05  /  FREE FALL → DEFORMABLE IMPACT",
       "Computed 3D solid trajectory versus analytic no-contact gravitational reference.",
       "Difference after impact is due to actual contact impulses and coupled elastic response.")
    save(fig,out,"05_com_impact_vs_ballistic.png")

def stress_distribution(states,m,out):
    frames=[0,60,100,140,180]
    fig,ax=plt.subplots(figsize=(12.8,6.9))
    fig.subplots_adjust(left=.10,right=.96,bottom=.17,top=.82)
    cmap=plt.get_cmap("viridis")
    maxval=float(states.von_mises_pa.max())
    for i,f in enumerate(frames):
        a=states.loc[states.frame==f,"von_mises_pa"].to_numpy()
        ax.hist(a,bins=55,range=(0,maxval),histtype="step",lw=1.9,
                color=cmap(i/4),density=True,
                label=f"t = {float(m.time_s.iloc[f]):.3f} s")
    ax.set(xlabel="Nodal volume-weighted von Mises stress (Pa)",
           ylabel="Probability density over 180 physical mesh vertices")
    ax.legend(frameon=False,labelcolor=FG)
    ax.grid(True)
    headline(fig,"06  /  REAL DEFORMATION FIELD DISTRIBUTIONS",
       "Stress distribution evolves as volumetric elastic motion and ground impact unfold.",
       "Vertex values are volume-weighted tet quantities; not independent samples.")
    save(fig,out,"06_von_mises_distribution_evolution.png")

def interactive(states,faces,m,impacts,out):
    import plotly.graph_objects as go
    triangles=faces[["a","b","c"]].to_numpy(dtype=int)
    grouped={int(f):df.sort_values("vertex_id") for f,df in states.groupby("frame")}
    peak=float(states.von_mises_pa.max())
    ev_by_frame=defaultdict(list)
    # Each impact belongs to the first source snapshot at or after it.
    dt=float(m.time_s.iloc[1])
    for r in impacts.itertuples(index=False):
        f=int(np.ceil(float(r.time_s)/dt-1e-8))
        ev_by_frame[max(0,min(180,f))].append((r.x_m,r.y_m,r.z_m,r.normal_impulse_ns))
    def mesh_trace(frame):
        df=grouped[frame]
        return go.Mesh3d(
            x=df.x.to_numpy(),y=df.y.to_numpy(),z=df.z.to_numpy(),
            i=triangles[:,0],j=triangles[:,1],k=triangles[:,2],
            intensity=df.von_mises_pa.to_numpy(),intensitymode="vertex",
            cmin=0,cmax=max(1,peak),colorscale="Viridis",
            colorbar=dict(title="von Mises (Pa)",thickness=15),
            opacity=.97,flatshading=False,name="True FEM stress")
    def impact_trace(frame):
        ev=ev_by_frame.get(frame,[])
        if not ev:ev=[(0,0,-1000,0)]
        return go.Scatter3d(
            x=[x[0] for x in ev],y=[x[1] for x in ev],z=[x[2] for x in ev],
            mode="markers",marker=dict(size=5,color=RED,opacity=.9),
            name="Rigid-floor impulse events",showlegend=True)
    plotframes=list(range(0,181,3))
    if plotframes[-1]!=180:plotframes.append(180)
    animation=[go.Frame(name=str(f),data=[mesh_trace(f),impact_trace(f)],
                        traces=[0,1]) for f in plotframes]
    fig=go.Figure(data=[mesh_trace(0),impact_trace(0)],frames=animation)
    fig.add_trace(go.Surface(
        x=[[-1.3,1.3],[-1.3,1.3]],y=[[-1.3,-1.3],[1.3,1.3]],
        z=[[0,0],[0,0]],opacity=.23,showscale=False,
        colorscale=[[0,"#29435b"],[1,"#29435b"]],name="Infinite rigid floor (visual patch)"))
    fig.update_layout(
       title=("KYOTO SCA / FIRST INTEGRATED 3D HYPERELASTIC SOLID"
           "<br><sup>Actual tetrahedral FEM + gravity + analytic plane impulses; no general mesh CCD or solver speedup</sup>"),
       font=dict(color=FG),paper_bgcolor=BG,plot_bgcolor=BG,
       height=840,margin=dict(t=100,l=10,r=10,b=80),
       scene=dict(bgcolor=BG,aspectmode="manual",
          aspectratio=dict(x=2.0,y=2.0,z=1.2),
          xaxis=dict(range=[-1.4,1.4],title="X / metres",gridcolor=GRID),
          yaxis=dict(range=[-1.4,1.4],title="Y / metres",gridcolor=GRID),
          zaxis=dict(range=[-.05,1.1],title="Z / metres",gridcolor=GRID),
          camera=dict(eye=dict(x=1.55,y=-2.0,z=1.2))),
       updatemenus=[dict(type="buttons",x=.04,y=-.055,buttons=[
          dict(label="▶ Play",method="animate",
               args=[None,dict(frame=dict(duration=65,redraw=True),
                  transition=dict(duration=0),fromcurrent=True)]),
          dict(label="❚❚ Pause",method="animate",
               args=[[None],dict(frame=dict(duration=0,redraw=False),
                mode="immediate")])])],
       sliders=[dict(x=.32,y=-.03,len=.66,
         currentvalue=dict(prefix="Integrated state: "),
         steps=[dict(label=str(f),method="animate",
                    args=[[str(f)],dict(frame=dict(duration=0,redraw=True),
                     mode="immediate")]) for f in plotframes])])
    fig.write_html(out/"07_interactive_physically_integrated_solid.html",
                   full_html=True,include_plotlyjs=True,auto_play=False)

def paraview(states,tets,manifest,out):
    import xml.etree.ElementTree as ET
    mesh_tets=tets[["a","b","c","d"]].to_numpy(dtype=int)
    d=out/"vtk_tetrahedra";d.mkdir(exist_ok=True,parents=True)
    root=ET.Element("VTKFile",type="Collection",version="0.1",
                    byte_order="LittleEndian")
    coll=ET.SubElement(root,"Collection")
    for f,df in states.groupby("frame"):
        f=int(f);df=df.sort_values("vertex_id")
        file=f"solid_{f:04d}.vtk"
        with (d/file).open("w") as outfp:
            outfp.write("# vtk DataFile Version 3.0\n")
            outfp.write("Kyoto REAL FEM volumetric tetrahedral state\n")
            outfp.write("ASCII\nDATASET UNSTRUCTURED_GRID\n")
            outfp.write(f"POINTS {len(df)} double\n")
            for r in df.itertuples(index=False):
                outfp.write(f"{r.x:.17g} {r.y:.17g} {r.z:.17g}\n")
            outfp.write(f"CELLS {len(mesh_tets)} {5*len(mesh_tets)}\n")
            for tet in mesh_tets:
                outfp.write("4 "+" ".join(str(x) for x in tet)+"\n")
            outfp.write(f"CELL_TYPES {len(mesh_tets)}\n")
            outfp.writelines("10\n" for _ in mesh_tets)
            outfp.write(f"POINT_DATA {len(df)}\n")
            for col in ["von_mises_pa","strain_energy_density_pa","mass_kg",
                        "displacement_m"]:
                outfp.write(f"SCALARS {col} double 1\nLOOKUP_TABLE default\n")
                outfp.writelines(f"{x:.17g}\n" for x in df[col])
            outfp.write("VECTORS velocity_m_per_s double\n")
            for r in df.itertuples(index=False):
                outfp.write(f"{r.vx:.17g} {r.vy:.17g} {r.vz:.17g}\n")
            outfp.write("VECTORS displacement_vector_m double\n")
            for r in df.itertuples(index=False):
                outfp.write(f"{r.x-r.rest_x:.17g} "
                            f"{r.y-r.rest_y:.17g} "
                            f"{r.z-r.rest_z:.17g}\n")
        ET.SubElement(coll,"DataSet",timestep=f"{float(df.time_s.iloc[0]):.17g}",
                      group="",part="0",file=file)
    ET.indent(root,space="  ")
    ET.ElementTree(root).write(d/"solid_time_series.pvd",
                              encoding="utf-8",xml_declaration=True)

def atlas(out):
    from PIL import Image,ImageDraw,ImageFont
    files=[f"{i:02d}_"+suffix for i,suffix in [
        (1,"actual_fem_energy_budget.png"),
        (2,"stress_and_jacobian.png"),
        (3,"contact_impulse_timeline.png"),
        (4,"spatial_contact_impulse_map.png"),
        (5,"com_impact_vs_ballistic.png"),
        (6,"von_mises_distribution_evolution.png")]]
    canvas=Image.new("RGB",(1900,2140),BG)
    for k,name in enumerate(files):
        source=Image.open(out/name).convert("RGB")
        source.thumbnail((940,650))
        px=k%2*950+(950-source.width)//2
        py=150+(k//2)*650+(650-source.height)//2
        canvas.paste(source,(px,py))
    draw=ImageDraw.Draw(canvas)
    try:
        font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",37)
    except OSError:font=ImageFont.load_default()
    draw.text((40,28),"KYOTO / PASS 05 — REAL VOLUMETRIC FEM",font=font,fill=FG)
    draw.text((40,82),"Source-backed energies, actual stress, analytical ground impulses, no scripted motion",fill=MUTED)
    canvas.save(out/"STAGE5_ACTUAL_PHYSICS_ATLAS.png")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    a=ap.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    manifest,m,states,faces,tets,impacts=load(a.input)
    theme()
    energy(m,impacts,a.output)
    material(m,a.output)
    contact(m,impacts,a.output)
    ground_map(impacts,a.output)
    com(m,impacts,manifest,a.output)
    stress_distribution(states,m,a.output)
    interactive(states,faces,m,impacts,a.output)
    paraview(states,tets,manifest,a.output)
    atlas(a.output)
    print("PASS: genuine solver output -> 6 figures, interactive 3D FEM, "
          "181 time-indexed ParaView volumetric tetra states")

if __name__=="__main__":main()
