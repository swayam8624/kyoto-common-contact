#!/usr/bin/env python3
"""Pass 7A — synchronized whole-face conservative FEM evidence.

Source: kyoto_curved_fem_export CSVs, exactly 900 semi-implicit FEM steps.
Model: 3D compressible neo-Hookean volume + synchronized whole-triangle
Lipschitz swept separation and barycentric effective-mass impulses.
Independent all-step surface audit must show zero overlap.

Never relabel vertex contacts as solved triangle contacts or self-contact.
"""
from __future__ import annotations
import argparse,json
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from matplotlib import cm,colors
BG="#091722";PANEL="#102638";INK="#ecf4fd";MUTED="#9fb6c9"
GRID="#294455";CYAN="#52d4e6";GOLD="#ffc078";GREEN="#65ddb5"
RED="#fc8395";PURPLE="#c8a3f7"

def setup():
    plt.rcParams.update(dict(figure_facecolor=BG,figure_dpi=110,
        savefig_facecolor=BG,axes_facecolor=PANEL,axes_edgecolor=GRID,
        axes_labelcolor=MUTED,text_color=INK,xtick_color=MUTED,
        ytick_color=MUTED,grid_color=GRID,font_family="DejaVu Sans"))
    # Matplotlib rcParams use camel-case? Instead set via explicit valid keys.
def style():
    plt.rcParams.update({
      "figure.facecolor":BG,"savefig.facecolor":BG,
      "axes.facecolor":PANEL,"axes.edgecolor":GRID,
      "axes.labelcolor":MUTED,"text.color":INK,
      "xtick.color":MUTED,"ytick.color":MUTED,
      "grid.color":GRID,"grid.alpha":.3,"font.family":"DejaVu Sans",
      "axes.titlesize":12,"axes.titleweight":"bold"})
def heading(fig,title,sub,footer):
    fig.text(.048,.96,title,size=17,weight="bold",va="top")
    fig.text(.048,.922,sub,size=9,color=MUTED,va="top")
    fig.text(.048,.025,footer,size=8,color=MUTED,va="bottom")
def save(fig,out,name):
    fig.savefig(out/name,dpi=170,bbox_inches="tight",pad_inches=.2)
    plt.close(fig)
def load(p):
    m=json.loads((p/"curved_manifest.json").read_text())
    assert m["simulation_kind"]=="ACTUAL_integrated_neohookean_tetrahedral_FEM"
    assert m["physical_motion"] and m["whole_triangle_lipschitz_CCD"]
    assert m["full_triangle_sphere_safety"] and not m["general_mesh_mesh_IPC_response"]
    assert not m["machine_certified_rounded_intervals"]
    states=pd.read_csv(p/"curved_solid_states.csv")
    metrics=pd.read_csv(p/"curved_solid_metrics.csv")
    impacts=pd.read_csv(p/"curved_contact_impulses.csv")
    faces=pd.read_csv(p/"surface_faces.csv")
    tets=pd.read_csv(p/"tetrahedra.csv")
    audit=pd.read_csv(p/"surface_sphere_clearance.csv")
    assert len(metrics)==m["frames"]==181
    assert len(states)==m["frames"]*m["vertices"]
    assert len(impacts)==m["normal_impulse_vertex_steps"]>0
    assert len(audit)==m["surface_audit_states"]==901
    assert len(faces)==m["surface_triangles"] and len(tets)==m["tetrahedra"]
    assert int((audit.penetrating_triangles>0).sum())==m["states_with_static_triangle_sphere_overlap"]==0
    assert int((audit.vertex_penetrations>0).sum())==m["states_with_vertex_inside_sphere"]
    assert audit.minimum_triangle_clearance_m.min()>-10
    assert audit.minimum_vertex_clearance_m.min()>=-2e-9
    assert metrics.min_tet_jacobian.min()>0
    steps=pd.read_csv(p/"whole_face_contact_step_diagnostics.csv")
    assert len(steps)==m["steps"]==900
    assert (steps.accepted_global_fraction>=0).all()
    assert (steps.accepted_global_fraction<=1).all()
    assert int(steps.time_clipped.sum())==m["time_clipped_steps"]
    return m,states,metrics,impacts,faces,tets,audit

def curve_clearance(m,audit,out):
    fig,ax=plt.subplots(figsize=(12.4,7.4))
    fig.subplots_adjust(left=.105,right=.95,bottom=.16,top=.82)
    t=audit.time_s
    ax.plot(t,audit.minimum_vertex_clearance_m*1000,color=CYAN,lw=2.2,
            label="Closest vertex – sphere gap")
    ax.plot(t,audit.minimum_triangle_clearance_m*1000,color=GOLD,lw=2.3,
            label="Closest full triangle – sphere gap")
    ax.axhline(0,color=RED,lw=1.6,ls="--",label="Overlap threshold")
    ax.set(xlabel="Native physical simulation time (seconds)",
           ylabel="Signed separation / millimetres")
    ax.grid(True);ax.legend(frameon=False,labelcolor=INK)
    heading(fig,"02 / VERIFIED WHOLE-FACE STATIC CLEARANCE",
        "Independent closest-point audit of ALL connected triangle faces at all 901 actual integrated states.",
        "No penetrating faces observed; exact-arithmetic separation theorem does not certify machine rounding.")
    save(fig,out,"02_triangle_vs_vertex_gap.png")

def impulse_and_overlap(m,metrics,audit,out):
    fig,ax=plt.subplots(figsize=(12.4,7.3))
    fig.subplots_adjust(left=.11,right=.9,bottom=.17,top=.81)
    ax.bar(metrics.time_s,metrics.normal_impulse_ns_interval,width=.0036,
           color=GREEN,alpha=.75,label="Normal contact impulses per five steps")
    ax.set(xlabel="Physical simulation time (seconds)",
           ylabel="Actual vertex normal impulse / N·s")
    twin=ax.twinx()
    twin.plot(audit.time_s,audit.penetrating_triangles,color=RED,lw=1.8,
              label="Triangles overlapping rigid sphere (independent audit)")
    twin.set_ylabel("Static penetrating surface triangles",color=RED)
    twin.tick_params(axis="y",colors=RED)
    ax.grid(axis="y")
    ax.legend(handles=[ax.patches[0],twin.lines[0]],
        labels=["Actual vertex contact impulse","Audited penetrating faces (required zero)"],
        frameon=False,labelcolor=INK,loc="upper right")
    heading(fig,"03 / BARYCENTRIC IMPULSES AND ZERO FACE OVERLAP",
        "Full-face contact response with measured effective-mass impulses and independent geometric audit.",
        "The Gauss–Seidel contact scheme is not a fully coupled IPC barrier solve; energy consistency remains open.")
    save(fig,out,"03_actual_impulses_and_surface_overlap.png")

def energy(metrics,out):
    fig,ax=plt.subplots(figsize=(12.5,7.2))
    fig.subplots_adjust(left=.10,right=.96,bottom=.16,top=.82)
    for key,col,label in [
       ("kinetic_j",CYAN,"Kinetic"),
       ("elastic_j",PURPLE,"Stored neo-Hookean strain"),
       ("gravitational_j",GOLD,"Gravitational potential (offset datum)"),
       ("mechanical_total_j",GREEN,"Total recorded mechanical")]:
        ax.plot(metrics.time_s,metrics[key],lw=2.2,color=col,label=label)
    ax.set(xlabel="Physical time / seconds",ylabel="Recorded energy / joules")
    ax.grid(True);ax.legend(frameon=False,labelcolor=INK,fontsize=9,ncol=2)
    heading(fig,"04 / ACTUAL NONPLANAR IMPACT ENERGY",
       "3D solid elasticity, gravity and lumped curved-obstacle impulses from C++ semi-implicit integration.",
       "Energy changes include damping, friction, restitution and first-contact time truncation; not an energy-conserving solver.")
    save(fig,out,"04_curved_fem_energy.png")

def stress_j(metrics,out):
    fig,ax=plt.subplots(figsize=(12.5,7.2))
    fig.subplots_adjust(left=.105,right=.90,bottom=.16,top=.82)
    p=ax.plot(metrics.time_s,metrics.max_stress_pa,color=PURPLE,
              lw=2.4,label="Peak Cauchy von Mises stress")[0]
    ax.set(xlabel="Physical time / seconds",ylabel="Maximum von Mises / pascals")
    twin=ax.twinx()
    q=twin.plot(metrics.time_s,metrics.min_tet_jacobian,color=GOLD,
                lw=2,label="Minimum det(F)")[0]
    twin.set_ylabel("Minimum element volume ratio J",color=GOLD)
    twin.tick_params(axis="y",colors=GOLD)
    twin.axhline(1,color=GOLD,ls=":",lw=1,alpha=.5)
    ax.grid(True);ax.legend(handles=[p,q],frameon=False,labelcolor=INK)
    heading(fig,"05 / REAL FEM STRESS AND VOLUME DEFORMATION",
       "Cauchy von Mises (Pa) and tetrahedral determinant J derived from the real solver's deformation gradients.",
       "Material parameters are illustrative, not experimentally calibrated.")
    save(fig,out,"05_curved_stress_and_jacobian.png")

def sphere_surface(center,r,u=32,v=18):
    theta=np.linspace(0,2*np.pi,u)
    phi=np.linspace(0,np.pi,v)
    X=center[0]+r*np.outer(np.cos(theta),np.sin(phi))
    Y=center[1]+r*np.outer(np.sin(theta),np.sin(phi))
    Z=center[2]+r*np.outer(np.ones_like(theta),np.cos(phi))
    return X,Y,Z

def mat3d(ax):
    ax.set_facecolor(BG)
    for a in (ax.xaxis,ax.yaxis,ax.zaxis):
        a.pane.set_facecolor((.035,.07,.11,1))
        a.pane.set_edgecolor(GRID)
        a._axinfo["grid"]["color"]=(.15,.24,.31,.2)
    ax.set(xlim=(-1.35,1.35),ylim=(-1.35,1.35),zlim=(-1.35,1.05))
    ax.set_box_aspect((2.7,2.7,2.4))
    ax.view_init(elev=28,azim=-55)
    ax.set_xlabel("X / m",fontsize=8,labelpad=2)
    ax.set_ylabel("Y / m",fontsize=8,labelpad=2)
    ax.set_zlabel("Z / m",fontsize=8,labelpad=2)
    ax.tick_params(labelsize=7)

def plots_3d(m,states,faces,out):
    ids=faces[["a","b","c"]].to_numpy(dtype=int)
    groups={int(k):v.sort_values("vertex_id") for k,v in states.groupby("frame")}
    first=m["first_curved_contact_time_s"]
    impact_index=int(np.clip(round(first/(m["dt_s"]*m["stride"])),0,180))
    samples=[0,impact_index,min(180,impact_index+35),180]
    fig=plt.figure(figsize=(14,12.4))
    fig.subplots_adjust(top=.875,bottom=.07,left=.02,right=.98,hspace=.035,wspace=.01)
    maxstress=max(1,float(states.stress_von_mises_pa.max()))
    sphere=sphere_surface(m["sphere_center_xyz_m"],m["sphere_radius_m"])
    for k,f in enumerate(samples):
        ax=fig.add_subplot(2,2,k+1,projection="3d")
        rows=groups[f]
        xyz=rows[["x","y","z"]].to_numpy()
        stress=rows.stress_von_mises_pa.to_numpy()
        triangles=xyz[ids]
        shade=np.clip(stress[ids].mean(axis=1)/maxstress,0,1)
        coll=Poly3DCollection(triangles,facecolors=cm.viridis(shade),
             edgecolors=CYAN,linewidths=.09,alpha=.93)
        ax.add_collection3d(coll)
        ax.plot_surface(*sphere,color=GOLD,alpha=.22,linewidth=0,
                        shade=False)
        mat3d(ax)
        ax.set_title(f"t = {f*m['dt_s']*m['stride']:.3f} s  |  real frame {f}",
                     color=INK,pad=8,fontsize=11)
    heading(fig,"01 / DEFORMABLE VOLUMETRIC TORUS VS RIGID CURVED OBSTACLE",
        "True 3D tetrahedral FEM under gravity; gold rigid sphere; surface scalar is actual computed von Mises stress.",
        "Real 3D FEM plus synchronized full-face contact clearance; general IPC mesh self-contact remains open.")
    save(fig,out,"01_real_curved_3d_fem_montage.png")

def impulse_footprint(m,impacts,out):
    fig=plt.figure(figsize=(12,8.0))
    fig.subplots_adjust(left=.01,right=.94,top=.82,bottom=.08)
    ax=fig.add_subplot(111,projection="3d")
    mat3d(ax)
    ax.plot_surface(*sphere_surface(m["sphere_center_xyz_m"],
                    m["sphere_radius_m"],50,25),color=GOLD,alpha=.14,
                    edgecolor="#9c774b",linewidth=.2)
    # A reproducible, uniformly spaced subset of genuine impulse records:
    # all metrics are computed on full unsampled native evidence.
    stride=max(1,int(np.ceil(len(impacts)/1000)))
    sample=impacts.iloc[::stride]
    weights=sample.impulse_ns.to_numpy()
    norm=colors.Normalize(vmin=0,vmax=max(1,float(impacts.impulse_ns.quantile(.98))))
    ax.scatter(sample.x,sample.y,sample.z,c=weights,cmap="magma",
               norm=norm,s=8+35*np.sqrt(np.maximum(weights,0)/
                    max(1,weights.max())),alpha=.86,depthshade=False)
    scat=cm.ScalarMappable(norm=norm,cmap="magma")
    cb=fig.colorbar(scat,ax=ax,pad=.03,shrink=.58)
    cb.set_label("Vertex impulse / N·s (source data)",color=MUTED)
    heading(fig,"06 / 3D RIGID-SPHERE IMPULSE FOOTPRINT",
        f"Native contact records: {len(impacts):,}; drawn deterministic subset: {len(sample):,}.",
        "Points are impulse-bearing vertices at accepted solver states, not a mapped face-traction field.")
    save(fig,out,"06_3d_impulse_footprint.png")

def interactive(m,states,faces,out):
    import plotly.graph_objects as go
    ids=faces[["a","b","c"]].to_numpy(dtype=int)
    groups={int(k):v.sort_values("vertex_id") for k,v in states.groupby("frame")}
    peak=max(1,float(states.stress_von_mises_pa.max()))
    c=np.asarray(m["sphere_center_xyz_m"],dtype=float)
    theta=np.linspace(0,2*np.pi,39)
    phi=np.linspace(0,np.pi,21)
    xx=c[0]+m["sphere_radius_m"]*np.outer(np.cos(theta),np.sin(phi))
    yy=c[1]+m["sphere_radius_m"]*np.outer(np.sin(theta),np.sin(phi))
    zz=c[2]+m["sphere_radius_m"]*np.outer(np.ones_like(theta),np.cos(phi))
    def mesh(f):
        df=groups[f]
        return go.Mesh3d(x=df.x,y=df.y,z=df.z,i=ids[:,0],j=ids[:,1],
          k=ids[:,2],intensity=df.stress_von_mises_pa,
          intensitymode="vertex",cmin=0,cmax=peak,colorscale="Viridis",
          colorbar=dict(title="von Mises / Pa",thickness=16),
          flatshading=False,opacity=.95,name="Integrated 3D FEM")
    steps=list(range(0,181,3))
    if steps[-1]!=180:steps.append(180)
    fig=go.Figure(data=[mesh(0),go.Surface(x=xx,y=yy,z=zz,
         colorscale=[[0,GOLD],[1,GOLD]],surfacecolor=np.zeros_like(xx),
         opacity=.28,showscale=False,name="Rigid analytic sphere")],
         frames=[go.Frame(name=str(i),data=[mesh(i)],traces=[0])
                 for i in steps])
    fig.update_layout(
      title="KYOTO SCA / WHOLE-FACE SYNCHRONIZED FEM"
         "<br><sup>180+ native FEM states · synchronized whole-face sweep · every accepted state audited</sup>",
      paper_bgcolor=BG,plot_bgcolor=BG,font=dict(color=INK),
      height=840,margin=dict(t=100,l=5,r=5,b=85),
      scene=dict(bgcolor=BG,aspectmode="manual",
         aspectratio=dict(x=1.0,y=1.0,z=.9),
         xaxis=dict(range=[-1.4,1.4],title="X (m)",gridcolor=GRID),
         yaxis=dict(range=[-1.4,1.4],title="Y (m)",gridcolor=GRID),
         zaxis=dict(range=[-1.4,1.15],title="Z (m)",gridcolor=GRID),
         camera=dict(eye=dict(x=1.6,y=-2.0,z=1.15))),
      updatemenus=[dict(type="buttons",x=.03,y=-.06,buttons=[
       dict(label="▶ Play",method="animate",
            args=[None,dict(frame=dict(duration=65,redraw=True),
                            fromcurrent=True)]),
       dict(label="❚❚ Pause",method="animate",
            args=[[None],dict(frame=dict(duration=0,redraw=False),
                              mode="immediate")])])],
      sliders=[dict(x=.33,y=-.038,len=.63,
           currentvalue=dict(prefix="Physical frame: "),
           steps=[dict(label=str(i),method="animate",
              args=[[str(i)],dict(frame=dict(duration=0,redraw=True),
                                   mode="immediate")]) for i in steps])])
    fig.write_html(out/"07_interactive_curved_fem.html",
                   full_html=True,include_plotlyjs=True,auto_play=False)

def vtk_export(m,states,tets,out):
    import xml.etree.ElementTree as ET
    folder=out/"vtk_volumetric"
    folder.mkdir(parents=True,exist_ok=True)
    cells=tets[["a","b","c","d"]].to_numpy(dtype=int)
    root=ET.Element("VTKFile",type="Collection",version="0.1",
                    byte_order="LittleEndian")
    collection=ET.SubElement(root,"Collection")
    for frame,df in states.groupby("frame"):
        frame=int(frame);df=df.sort_values("vertex_id")
        name=f"curved_{frame:04}.vtk"
        with (folder/name).open("w") as fp:
            fp.write("# vtk DataFile Version 3.0\n")
            fp.write("True C++ tetrahedral FEM curved rigid sphere impact\n")
            fp.write("ASCII\nDATASET UNSTRUCTURED_GRID\n")
            fp.write(f"POINTS {len(df)} double\n")
            for r in df.itertuples(index=False):
                fp.write(f"{r.x:.17g} {r.y:.17g} {r.z:.17g}\n")
            fp.write(f"CELLS {len(cells)} {5*len(cells)}\n")
            for cell in cells:
                fp.write("4 "+" ".join(map(str,cell))+"\n")
            fp.write(f"CELL_TYPES {len(cells)}\n")
            fp.writelines("10\n" for _ in cells)
            fp.write(f"POINT_DATA {len(df)}\n")
            for col in ("stress_von_mises_pa","strain_energy_density_pa",
                        "displacement_m","clearance_m"):
                fp.write(f"SCALARS {col} double 1\nLOOKUP_TABLE default\n")
                fp.writelines(f"{v:.17g}\n" for v in df[col])
            fp.write("VECTORS velocity_mps double\n")
            for r in df.itertuples(index=False):
                fp.write(f"{r.vx:.17g} {r.vy:.17g} {r.vz:.17g}\n")
        ET.SubElement(collection,"DataSet",timestep=str(float(df.time_s.iloc[0])),
                      group="",part="0",file=name)
    ET.indent(root,space="  ")
    ET.ElementTree(root).write(folder/"curved_fem_series.pvd",
                              encoding="utf-8",xml_declaration=True)

def atlas(out):
    from PIL import Image,ImageDraw,ImageFont
    names=["01_real_curved_3d_fem_montage.png",
           "02_triangle_vs_vertex_gap.png",
           "03_actual_impulses_and_surface_overlap.png",
           "04_curved_fem_energy.png",
           "05_curved_stress_and_jacobian.png",
           "06_3d_impulse_footprint.png",
           "07_synchronized_motion_fraction.png"]
    canvas=Image.new("RGB",(1900,2910),BG)
    for i,name in enumerate(names):
        im=Image.open(out/name).convert("RGB")
        im.thumbnail((950,670))
        x=i%2*950+(950-im.width)//2
        y=135+i//2*670+(670-im.height)//2
        canvas.paste(im,(x,y))
        im.close()
    d=ImageDraw.Draw(canvas)
    try:font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",34)
    except OSError:font=ImageFont.load_default()
    d.text((35,30),"KYOTO / PASS 07 — WHOLE-FACE FEM",fill=INK,font=font)
    d.text((35,79),"Zero-overlap triangle audit, real computed stress, energy and contact dynamics",fill=MUTED)
    canvas.save(out/"STAGE7_WHOLE_FACE_FEM_SCIENCE_ATLAS.png")

def synchronized_fraction(source,out):
    steps=pd.read_csv(source/"whole_face_contact_step_diagnostics.csv")
    fig,ax=plt.subplots(figsize=(12.5,7.2))
    fig.subplots_adjust(left=.11,right=.89,bottom=.17,top=.82)
    t=steps.time_s.to_numpy()
    ax.plot(t,steps.accepted_global_fraction,color=CYAN,lw=1.8,
            label="Accepted synchronized fraction")
    ax.set(xlabel="Physical simulation time / seconds",
           ylabel="Global accepted displacement fraction",ylim=(-.025,1.025))
    twin=ax.twinx()
    twin.plot(t,steps.normal_impulse_ns,color=GOLD,lw=1.5,
              label="Mass-weighted normal impulse")
    twin.set_ylabel("Barycentric normal impulse / N·s",color=GOLD)
    twin.tick_params(axis="y",colors=GOLD)
    ax.grid(True)
    ax.legend(handles=[ax.lines[0],twin.lines[0]],frameon=False,
              labelcolor=INK)
    heading(fig,"07 / SYNCHRONIZED GEOMETRIC SAFETY VS PHYSICS",
        "Each actual integrated timestep: full-body safe motion fraction and barycentrically distributed impulse.",
        "Clipping (<1) alters global position advancement; no fully coupled contact energy-conservation claim.")
    save(fig,out,"07_synchronized_motion_fraction.png")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    opt=p.parse_args()
    opt.output.mkdir(parents=True,exist_ok=True)
    m,states,metrics,impacts,faces,tets,audit=load(opt.input)
    style()
    plots_3d(m,states,faces,opt.output)
    curve_clearance(m,audit,opt.output)
    impulse_and_overlap(m,metrics,audit,opt.output)
    energy(metrics,opt.output)
    stress_j(metrics,opt.output)
    impulse_footprint(m,impacts,opt.output)
    synchronized_fraction(opt.input,opt.output)
    interactive(m,states,faces,opt.output)
    vtk_export(m,states,tets,opt.output)
    atlas(opt.output)
    print("PASS: 901 static triangle clearance observations, seven real FEM charts, 181 tetrahedral time states, offline interactive 3D")

if __name__=="__main__":main()
