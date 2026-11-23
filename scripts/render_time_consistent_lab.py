#!/usr/bin/env python3
"""Pass 8: figures ONLY from original C++ real-FEM time-consistent evidence.

Fine-256 is the SAME numerical model at a finer dt, not independently
verified physics. The original clipped alpha*dt is a nominal motion
fraction, not a physically integrated elapsed time. Keep that distinction.
"""
import argparse,json
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from PIL import Image,ImageDraw,ImageFont
BG="#081521";PANEL="#10283a";INK="#f0f8ff";MUTED="#a5bed1"
CYAN="#64d9ed";GOLD="#fec477";GREEN="#72e1ac";RED="#fc8c9c"
PURPLE="#c5a3fa";GRID="#32536c"
def style():
    plt.rcParams.update({"figure.facecolor":BG,"savefig.facecolor":BG,
      "axes.facecolor":PANEL,"axes.edgecolor":GRID,
      "text.color":INK,"axes.labelcolor":MUTED,
      "xtick.color":MUTED,"ytick.color":MUTED,
      "grid.color":GRID,"grid.alpha":.3,"font.family":"DejaVu Sans"})
def title(fig,heading,sub,footer):
    fig.text(.053,.960,heading,fontsize=17,weight="bold",va="top")
    fig.text(.053,.917,sub,fontsize=9,color=MUTED,va="top")
    fig.text(.053,.025,footer,fontsize=8,color=MUTED)
def save(fig,p):
    fig.savefig(p,dpi=180,bbox_inches="tight",pad_inches=.16)
    plt.close(fig)
def read(root):
    m=json.loads((root/"manifest.json").read_text())
    history=pd.read_csv(root/"routine_900_physics.csv")
    steps=pd.read_csv(root/"fast_adaptive_accepted_substeps.csv")
    comparison=pd.read_csv(root/"fast_original_vs_adaptive_vs_fine.csv")
    vertices=pd.read_csv(root/"fast_event_true_3d_states.csv")
    faces=pd.read_csv(root/"fast_event_surface_faces.csv")
    assert m["actual_solver_physics"] and not m["arbitrary_mesh_contact"]
    assert not m["same_model_fine_reference_is_independent_solver"]
    assert m["ordinary_steps"]==900 and len(history)==901
    assert len(steps)==m["adaptive_accepted_physical_substeps"]>1
    assert len(vertices)==720 and len(faces)==320
    assert (history.minimum_triangle_clearance_m>=0).all()
    assert (steps.minimum_full_triangle_clearance_m>=0).all()
    assert np.isclose(steps.dt_s.sum(),m["fast_trial_dt_s"],atol=1e-12)
    assert np.isclose(steps.end_time_s.iloc[-1],m["fast_trial_dt_s"],atol=1e-12)
    assert np.isnan(comparison.loc[0,"verified_integrated_elapsed_time_s_or_nan"])
    assert np.isclose(comparison.loc[1,"verified_integrated_elapsed_time_s_or_nan"],
                      m["fast_trial_dt_s"],atol=1e-12)
    return m,history,steps,comparison,vertices,faces
def time_partition(m,steps,out):
    fig,ax=plt.subplots(figsize=(12.9,6.8))
    fig.subplots_adjust(left=.105,right=.96,bottom=.19,top=.81)
    total=m["fast_trial_dt_s"]*1000
    for r in steps.itertuples():
        ax.barh(1,width=1000*r.dt_s,left=1000*r.start_time_s,
                height=.3,color=cm.viridis(min(1,r.bisection_depth/12)),
                edgecolor=BG,linewidth=1)
    ax.barh(0,width=total,height=.3,color=PURPLE,alpha=.35)
    old=m["old_motion_fraction_times_dt_s_not_physically_integrated"]*1000
    ax.axvline(old,ls="--",lw=1.9,color=RED)
    ax.annotate(f"Legacy nominal motion prefix {old:.4f} ms",
                (old,.35),color=RED,ha="center",fontsize=10)
    ax.axvline(total,ls=":",color=GOLD)
    ax.set(xlim=(0,total*1.05),ylim=(-.5,1.5),yticks=[0,1],
      yticklabels=["Legacy: charges full dt despite clipping",
                   "New: physically integrated substeps"],
      xlabel="Timestep position (milliseconds)")
    ax.grid(axis="x")
    title(fig,"01 / NO PHANTOM PHYSICAL TIME",
       "Every colored interval is a true fresh FEM integration; their sum equals the complete requested timestep.",
       "The old alpha×dt is merely a nominal displacement fraction: its velocity was computed for full dt.")
    save(fig,out/"01_time_partition.png")
def gap_and_impulse(steps,out):
    fig,ax=plt.subplots(figsize=(12.9,7))
    fig.subplots_adjust(left=.10,right=.90,bottom=.16,top=.81)
    t=steps.end_time_s*1000
    p=ax.plot(t,steps.minimum_full_triangle_clearance_m*1000,
              marker=".",lw=2.2,color=GREEN,label="Full-surface sphere clearance")[0]
    ax.axhline(0,ls="--",lw=1.3,color=RED)
    ax.set(xlabel="Actually integrated time (ms)",
           ylabel="Minimum entire-triangle separation (mm)")
    bx=ax.twinx()
    bars=bx.bar(t,steps.normal_impulse_ns,width=steps.dt_s*650,
               color=GOLD,alpha=.48)
    bx.set_ylabel("Contact normal impulse / accepted substep (N·s)",color=GOLD)
    bx.tick_params(axis="y",colors=GOLD)
    ax.grid(True)
    ax.legend(handles=[p,bars[0]],
              labels=["Independent full-surface gap","Physical face impulse"],
              frameon=False,labelcolor=INK)
    title(fig,"02 / CONTACT AT REAL PHYSICAL SUBSTEPS",
       "Actual native closest-triangle distances and mass-weighted impulses, with positive clearance at every accepted substep.",
       "A finite skin and local contact law are used; this is not IPC barrier-contact optimization.")
    save(fig,out/"02_clearance_and_impulse.png")
def fine_model(comparison,out):
    fig,axes=plt.subplots(1,2,figsize=(12.7,6.7))
    fig.subplots_adjust(left=.105,right=.95,bottom=.22,top=.8,wspace=.36)
    fields=[
      ("mean_position_difference_from_fine_m",1000,
       "Mean vertex position difference (mm)"),
      ("mean_velocity_difference_from_fine_m_per_s",1,
       "Mean vertex velocity difference (m/s)")]
    for ax,(key,factor,label) in zip(axes,fields):
        vals=comparison[key].to_numpy()[:2]*factor
        bars=ax.bar(["Old clipped","Adaptive replay"],vals,
                    width=.6,color=[RED,GREEN])
        ax.set_ylabel(label)
        ax.grid(axis="y")
        for rect,val in zip(bars,vals):
            ax.annotate(f"{val:.4g}",
                (rect.get_x()+rect.get_width()/2,rect.get_height()),
                xytext=(0,6),textcoords="offset points",
                ha="center",color=INK)
    title(fig,"03 / FINER REPLAY IS A NUMERICAL CHECK",
       "Both methods compared with 256 smaller steps of the SAME material/contact model.",
       "This is not independent experimental truth. Lower numerical error does not prove accurate real-world dynamics.")
    save(fig,out/"03_fine_model_differences.png")
def energy(history,out):
    # Gravitational potential is recorded against an arbitrary -100 m
    # height datum in the native source. Subtract the *initial* energy
    # for visualization only; never alter the raw CSV or physics.
    fig,ax=plt.subplots(figsize=(12.8,6.8))
    fig.subplots_adjust(left=.10,right=.95,bottom=.16,top=.81)
    potential_delta=history.gravity_j-history.gravity_j.iloc[0]
    initial_total=float((history.kinetic_j+history.elastic_j+
                         history.gravity_j).iloc[0])
    total_delta=(history.kinetic_j+history.elastic_j+
                 history.gravity_j)-initial_total
    for data,color,label in [
      (history.kinetic_j,CYAN,"Kinetic energy"),
      (history.elastic_j,PURPLE,"Stored neo-Hookean strain energy"),
      (potential_delta,GOLD,"Change in gravitational potential"),
      (total_delta,GREEN,"Change in total mechanical energy")]:
        ax.plot(history.time_s,data,color=color,lw=2,label=label)
    ax.set(xlabel="True integrated physical time (seconds)",
           ylabel="Native energy or change from initial datum (joules)")
    ax.grid(True);ax.legend(frameon=False,labelcolor=INK,fontsize=9,ncol=2)
    title(fig,"04 / ROUTINE 900-STEP MATERIAL ENERGY",
      "Original C++ kinetic and elastic energies; gravitational and total curves shifted by their initial constants.",
      "Visual datum shift only. Damping, restitution and friction can dissipate mechanical energy.")
    save(fig,out/"04_routine_energy.png")
def sphere(c,r):
    th=np.linspace(0,2*np.pi,44);phi=np.linspace(0,np.pi,24)
    return (c[0]+r*np.outer(np.cos(th),np.sin(phi)),
            c[1]+r*np.outer(np.sin(th),np.sin(phi)),
            c[2]+r*np.outer(np.ones_like(th),np.cos(phi)))
def spatial(m,vertices,faces,out):
    order=["initial","legacy_global_clip","adaptive_full_elapsed_time",
           "fine_256_same_model"]
    labels=["START / physical mesh","OLD / partial displacement",
            "NEW / full elapsed time","FINE / same-model dt/256"]
    tri=faces[["a","b","c"]].to_numpy(dtype=int)
    groups={k:v.sort_values("vertex_id") for k,v in vertices.groupby("algorithm")}
    xyzsphere=sphere(m["sphere_center_xyz_m"],m["fast_sphere_radius_m"])
    maxstress=max(1.,vertices.von_mises_pa.max())
    fig=plt.figure(figsize=(18.8,6.3))
    fig.subplots_adjust(left=.01,right=.99,top=.78,bottom=.08,wspace=.02)
    for k,name in enumerate(order):
        a=groups[name]
        positions=a[["x","y","z"]].to_numpy(dtype=float)
        stress=a.von_mises_pa.to_numpy(dtype=float)
        triangles=positions[tri]
        ax=fig.add_subplot(1,4,k+1,projection="3d")
        patches=Poly3DCollection(triangles,
            facecolors=cm.viridis(np.clip(stress[tri].mean(axis=1)/maxstress,0,1)),
            edgecolors="#1b3f4b",linewidths=.09,alpha=.95)
        ax.add_collection3d(patches)
        ax.plot_surface(*xyzsphere,color=GOLD,alpha=.23,linewidth=0,shade=False)
        for axis in (ax.xaxis,ax.yaxis,ax.zaxis):
            axis.pane.set_facecolor((.025,.065,.11,1))
            axis.pane.set_edgecolor(GRID)
            axis._axinfo["grid"]["color"]=(.15,.25,.32,.22)
        ax.set(xlim=(-1.4,1.4),ylim=(-1.4,1.4),zlim=(-1.40,1.1))
        ax.set_box_aspect((2.8,2.8,2.5))
        ax.view_init(elev=28,azim=-57)
        ax.tick_params(labelsize=7)
        ax.set_title(labels[k],color=INK,fontsize=10)
    title(fig,"05 / SOURCE-DERIVED 3D DEFORMABLE IMPACT",
      "All mesh vertices, sphere geometry, and stress scalars are original native C++ physics data.",
      "Colors show actual Cauchy von Mises stress; geometry is physical FEM, not scripted animation.")
    save(fig,out/"05_true_3d_contact_states.png")
def atlas(out):
    # 4 full-size scientific figures followed by a genuinely wide
    # 4-state 3D comparison. This avoids shrinking the 3D evidence
    # into one quarter of a largely empty research poster.
    first=["01_time_partition.png","02_clearance_and_impulse.png",
           "03_fine_model_differences.png","04_routine_energy.png"]
    canvas=Image.new("RGB",(1900,1940),BG)
    for k,file in enumerate(first):
        source=Image.open(out/file).convert("RGB")
        source.thumbnail((930,505))
        x=(k%2)*950+(950-source.width)//2
        y=150+(k//2)*550+(525-source.height)//2
        canvas.paste(source,(x,y));source.close()
    wide=Image.open(out/"05_true_3d_contact_states.png").convert("RGB")
    wide.thumbnail((1860,645))
    canvas.paste(wide,((1900-wide.width)//2,1270))
    wide.close()
    draw=ImageDraw.Draw(canvas)
    try:
        font=ImageFont.truetype(
          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",38)
    except OSError:font=ImageFont.load_default()
    draw.text((40,28),"KYOTO / PASS 08 — PHYSICAL TIME REPLAY",
              fill=INK,font=font)
    draw.text((40,87),
      "True physical substeps | full-surface gap | numerical resolution | real 3D FEM impact",
      fill=MUTED)
    draw.text((40,1885),
      "Source: actual C++23 FEM traces. Numerical reference uses the SAME contact model; general IPC physics remains open.",
      fill=MUTED)
    canvas.save(out/"STAGE8_ADAPTIVE_TIME_REPLAY_ATLAS.png")
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    m,history,steps,compare,vertices,faces=read(a.input)
    style();time_partition(m,steps,a.output)
    gap_and_impulse(steps,a.output)
    fine_model(compare,a.output)
    energy(history,a.output)
    spatial(m,vertices,faces,a.output)
    atlas(a.output)
    print("PASS: 900-step native FEM time-replay, five actual physics figures, original 3D solver geometry")
if __name__=="__main__":main()
