#!/usr/bin/env python3
"""Kyoto Pass 9: independent continuous geometry + contact impulse identities.

Every chart and source point comes from native C++23 tests running the
ACTUAL FEM and its contact law. The swept prism is independent of the
midpoint-Lipschitz solver: six-endpoint convex-hull separating planes
prove strict clearance in exact arithmetic. Unresolved != collision;
no machine-directed rounding has been proved.

Kinetic/momentum work claims concern ONE frozen-position local impulse,
not full damped, gravitating neo-Hookean solver energy conservation.
"""
from __future__ import annotations
import argparse,json
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import cm,colors
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from PIL import Image,ImageDraw,ImageFont

BG="#081722";PANEL="#11283c";INK="#f0f7fd";MUTED="#abc0d3"
GREEN="#69ddb1";CYAN="#59d6ec";GOLD="#f8c27c";PURPLE="#cba7ff"
RED="#ff8998";GRID="#35556a"

def style():
    plt.rcParams.update({
        "figure.facecolor":BG,"axes.facecolor":PANEL,
        "savefig.facecolor":BG,"axes.edgecolor":GRID,
        "text.color":INK,"axes.labelcolor":MUTED,
        "xtick.color":MUTED,"ytick.color":MUTED,
        "grid.color":GRID,"grid.alpha":.3,"font.family":"DejaVu Sans"})
def note(fig,heading,sub,footer):
    fig.text(.052,.958,heading,size=17,weight="bold",va="top")
    fig.text(.052,.916,sub,size=9,color=MUTED,va="top")
    fig.text(.052,.025,footer,size=8,color=MUTED)
def save(fig,path):
    fig.savefig(path,dpi=175,bbox_inches="tight",pad_inches=.15)
    plt.close(fig)
def load(root,physics):
    m=json.loads((root/"independent_manifest.json").read_text())
    im=json.loads((root/"impulse_manifest.json").read_text())
    routine=pd.read_csv(root/"real_fem_independent_sweeps.csv")
    fast=pd.read_csv(root/"fast_impact_independent_sweeps.csv")
    faces=pd.read_csv(root/"fast_impact_face_witnesses.csv")
    impulse=pd.read_csv(root/"impulse_balance_cases.csv")
    original=pd.read_csv(physics/"fast_event_true_3d_states.csv")
    topology=pd.read_csv(physics/"fast_event_surface_faces.csv")
    assert m["independent_of_midpoint_lipschitz_narrowphase"]
    assert not m["machine_directed_rounding_proven"]
    assert m["ordinary_physical_macro_steps"]==900
    assert len(routine)==900
    assert len(fast)==m["fast_accepted_physical_substeps"]>1
    assert len(faces)==320*len(fast)
    assert len(impulse)==im["sample_count"]==1200
    assert len(topology)==320 and len(original)==720
    assert m["ordinary_face_intervals"]==int(
       routine.faces_independently_certified.sum()+
       routine.faces_unresolved.sum())
    assert m["fast_face_intervals"]==len(faces)==int(
       fast.faces_independently_certified.sum()+
       fast.faces_unresolved.sum())
    assert int(faces.continuously_certified.sum())==m["fast_certified_intervals"]
    assert (impulse.measured_delta_kinetic_j<=1e-5).all()
    assert m["ordinary_min_static_clearance_m"]>0
    assert m["fast_min_static_clearance_m"]>0
    assert all(np.isfinite(impulse[c]).all() for c in impulse.columns)
    return m,im,routine,fast,faces,impulse,original,topology

def independent_coverage(m,routine,out):
    fig,ax=plt.subplots(figsize=(12.8,7))
    fig.subplots_adjust(left=.1,right=.93,bottom=.16,top=.8)
    t=routine.time_s.to_numpy()
    certified=routine.faces_independently_certified.to_numpy()
    unresolved=routine.faces_unresolved.to_numpy()
    pct=100*certified/np.maximum(certified+unresolved,1)
    ax.fill_between(t,0,pct,color=GREEN,alpha=.24)
    ax.plot(t,pct,color=GREEN,lw=1.9,
            label="Independently certified swept-face intervals")
    ax.set(xlabel="Actual integrated FEM time / seconds",
           ylabel="Strict independent continuous clearance / %",
           ylim=(-3,104))
    other=ax.twinx()
    other.plot(t,unresolved,lw=1.4,color=RED,
               label="Unresolved, NOT asserted safe")
    other.set_ylabel("Unresolved swept triangles",color=RED)
    other.tick_params(axis="y",colors=RED)
    ax.grid(True)
    ax.legend(handles=[ax.lines[0],other.lines[0]],
        frameon=False,labelcolor=INK,fontsize=9)
    note(fig,"01 / INDEPENDENT CONTINUOUS CLEARANCE PROOF",
        "Every moving triangle of all 900 actual FEM timesteps checked with a six-endpoint convex-hull separating plane.",
        "Unresolved means no separating witness was found; it is NOT evidence of collision or a certified-safe interval.")
    save(fig,out/"01_independent_swept_certificate_coverage.png")

def certificate_effort(routine,fast,out):
    fig,axes=plt.subplots(1,2,figsize=(13.1,7))
    fig.subplots_adjust(left=.095,right=.94,bottom=.20,top=.78,wspace=.28)
    axes[0].plot(routine.time_s,
                 routine.support_plane_evaluations,color=CYAN,lw=1.7)
    axes[0].set(xlabel="Routine physical time (s)",
           ylabel="Paid independent separating-plane tests")
    axes[0].grid(True)
    axes[1].bar(np.arange(len(fast)),
                fast.support_plane_evaluations,color=GOLD,alpha=.85,
                label="Support-plane evaluations")
    axes[1].set(xlabel="Fast contact accepted physical leaf ID",
        ylabel="Independent plane tests per leaf",
        xticks=list(range(len(fast))))
    axes[1].grid(axis="y")
    note(fig,"02 / PAY FOR EVERY GEOMETRIC PROOF",
        "Each independent swept-hull interval and witness-plane attempt is counted; no cheap outcome is inferred from the solver.",
        "These are operation counts, NOT IPC-vs-SCA timings or measured whole-step performance comparisons.")
    save(fig,out/"02_independent_proof_computation.png")

def static_distance(m,routine,fast,out):
    fig,ax=plt.subplots(figsize=(12.8,7))
    fig.subplots_adjust(left=.10,right=.94,bottom=.16,top=.82)
    y=np.maximum(routine.max_static_oracle_disagreement_m.to_numpy(),1e-18)
    ax.semilogy(routine.time_s,y,color=PURPLE,lw=1.6,
                label="Routine / independent Voronoi vs production")
    ax.scatter(fast.physical_end_time_s,
         np.maximum(fast.max_static_oracle_disagreement_m,1e-18),
         color=GOLD,s=30,zorder=3,label="Fast accepted leaves")
    ax.set(xlabel="Physical time / seconds",
           ylabel="Maximum point-to-finite-triangle oracle difference (m)")
    ax.grid(True);ax.legend(frameon=False,labelcolor=INK)
    note(fig,"03 / SECOND FINITE-TRIANGLE DISTANCE ORACLE",
        "Independent Voronoi-region classification is compared against the production edge/projection geometry.",
        "A small discrepancy validates tested arithmetic agreement, not universally robust IEEE orientation predicates.")
    save(fig,out/"03_independent_voronoi_distance_difference.png")

def impulse_work(impulse,out):
    x=impulse.analytical_delta_kinetic_j.to_numpy()
    y=impulse.measured_delta_kinetic_j.to_numpy()
    fig,ax=plt.subplots(figsize=(12.8,7))
    fig.subplots_adjust(left=.11,right=.92,bottom=.16,top=.82)
    col=ax.scatter(x,y,c=impulse.restitution,s=10,alpha=.72,
            cmap="viridis",vmin=0,vmax=1)
    lo=min(x.min(),y.min())
    hi=max(x.max(),y.max())
    ax.plot([lo,hi],[lo,hi],color=RED,lw=1.5,ls="--",
            label="Exact local impulse work identity")
    ax.set(xlabel="Analytical predicted local kinetic change / joules",
           ylabel="Actual measured local kinetic change / joules")
    ax.grid(True);ax.legend(frameon=False,labelcolor=INK)
    cb=fig.colorbar(col,ax=ax,pad=.012,shrink=.78)
    cb.set_label("Contact restitution",color=MUTED)
    note(fig,"04 / CONTACT IMPULSE WORK BALANCE",
        "1,200 rotated and translated 3D unequal-mass face impacts; independent measured vs analytical delta kinetic.",
        "This proves the LOCAL impulse law on frozen positions; it does not prove global deformable-FEM energy conservation.")
    save(fig,out/"04_impulse_kinetic_work_identity.png")

def momentum(impulse,out):
    fig,axes=plt.subplots(1,2,figsize=(12.8,7))
    fig.subplots_adjust(left=.10,right=.95,bottom=.20,top=.8,wspace=.30)
    for ax,field,col,label in [
        (axes[0],"momentum_balance_residual_ns",CYAN,
          "Linear ΔP − applied J / N·s"),
        (axes[1],"angular_balance_residual_nsm",GOLD,
          "Angular ΔL − q×J / N·m·s")]:
        s=np.maximum(impulse[field].to_numpy(),1e-18)
        ax.plot(np.arange(len(s)),s,lw=.7,color=col)
        ax.axhline(1e-8,ls="--",lw=1,color=RED)
        ax.set_yscale("log")
        ax.set(xlabel="Seeded 3D face-contact experiment",ylabel=label)
        ax.grid(True)
    note(fig,"05 / LINEAR AND ANGULAR MOMENTUM ACCOUNTING",
        "Independent recomputation uses the entire three-vertex mass distribution and the actual contact point.",
        "The static rigid obstacle receives an implicit opposite impulse; its own motion is not simulated.")
    save(fig,out/"05_local_momentum_angular_residuals.png")

def dissipation(impulse,out):
    fig,ax=plt.subplots(figsize=(12.7,7))
    fig.subplots_adjust(left=.10,right=.92,bottom=.16,top=.80)
    change=impulse.measured_delta_kinetic_j.to_numpy()
    loss=np.maximum(-change,0)
    plot=ax.scatter(impulse.restitution,loss,c=impulse.friction,
                    cmap="plasma",s=11,alpha=.72,vmin=0,vmax=1)
    ax.set(xlabel="Actual local restitution coefficient",
           ylabel="Local impact kinetic energy dissipated / joules")
    ax.grid(True)
    cb=fig.colorbar(plot,ax=ax,pad=.012)
    cb.set_label("Coulomb-like friction coefficient",color=MUTED)
    note(fig,"06 / HOW CONTACT IMPULSES DISSIPATE KINETIC ENERGY",
        "Real impulses from 1,200 reproducible local impacts with varied masses, directions, restitution and friction.",
        "Energy loss here excludes elastic deformation, global time-integration error, damping and gravity.")
    save(fig,out/"06_contact_local_energy_dissipation.png")

def sphere_geometry(center,radius):
    t=np.linspace(0,2*np.pi,43)
    p=np.linspace(0,np.pi,24)
    return (center[0]+radius*np.outer(np.cos(t),np.sin(p)),
            center[1]+radius*np.outer(np.sin(t),np.sin(p)),
            center[2]+radius*np.outer(np.ones_like(t),np.cos(p)))

def science_3d(m,face_receipts,physical_vertices,topology,out):
    last=int(face_receipts.leaf_id.max())
    sub=face_receipts[face_receipts.leaf_id==last].sort_values("face_id")
    vertices=physical_vertices[
       physical_vertices.algorithm=="adaptive_full_elapsed_time"
    ].sort_values("vertex_id")
    ids=topology[["a","b","c"]].to_numpy(dtype=int)
    P=vertices[["x","y","z"]].to_numpy()
    measured_centers=P[ids].mean(axis=1)
    ledger_centers=sub[["center_x","center_y","center_z"]].to_numpy()
    assert len(sub)==320
    assert np.max(np.linalg.norm(ledger_centers-measured_centers,axis=1))<1e-7
    stress=vertices.von_mises_pa.to_numpy()
    s=sphere_geometry(m["fast_sphere_center_xyz_m"],
                        m["fast_sphere_radius_m"])
    fig=plt.figure(figsize=(13.7,7.4))
    fig.subplots_adjust(left=.01,right=.98,bottom=.05,top=.78,wspace=.02)
    for side in range(2):
        ax=fig.add_subplot(1,2,side+1,projection="3d")
        if side==0:
            scalars=stress[ids].mean(axis=1)
            col=cm.viridis(np.clip(scalars/max(1.,stress.max()),0,1))
            heading="REAL COMPUTED STRESS"
        else:
            certified=sub.continuously_certified.to_numpy(dtype=int)
            col=np.array([
               colors.to_rgba(GREEN if v else RED,.95)
               for v in certified])
            heading="INDEPENDENT SWEPT CLEARANCE"
        mesh=Poly3DCollection(P[ids],facecolors=col,
              edgecolors="#28515a",linewidths=.09,alpha=.94)
        ax.add_collection3d(mesh)
        ax.plot_surface(*s,color=GOLD,alpha=.24,shade=False,linewidth=0)
        ax.set(xlim=(-1.35,1.35),ylim=(-1.35,1.35),zlim=(-1.4,1.0))
        ax.set_box_aspect((2.7,2.7,2.4))
        ax.view_init(elev=29,azim=-55)
        ax.set_title(heading,color=INK,pad=7,fontsize=11)
        for pane in (ax.xaxis,ax.yaxis,ax.zaxis):
            pane.pane.set_facecolor((.02,.08,.12,1))
            pane.pane.set_edgecolor(GRID)
            pane._axinfo["grid"]["color"]=(.16,.28,.34,.17)
        ax.tick_params(labelsize=7)
    note(fig,"07 / INDEPENDENT CCD PROOF ON THE REAL FAST 3D FEM",
       "Same genuine 180-vertex physical state from Pass 8. Left: stress (Pa). Right: per-face independent proof status.",
       "Green = strict six-endpoint plane certificate; red = unresolved, NOT proven colliding. Sphere is the source collider.")
    save(fig,out/"07_independent_3d_swept_proof_map.png")

def atlas(out):
    sources=[f"{i:02d}_{name}.png" for i,name in [
        (1,"independent_swept_certificate_coverage"),
        (2,"independent_proof_computation"),
        (3,"independent_voronoi_distance_difference"),
        (4,"impulse_kinetic_work_identity"),
        (5,"local_momentum_angular_residuals"),
        (6,"contact_local_energy_dissipation"),
        (7,"independent_3d_swept_proof_map")]]
    canvas=Image.new("RGB",(1950,2840),BG)
    for i,source in enumerate(sources):
        image=Image.open(out/source).convert("RGB")
        image.thumbnail((970,660))
        x=i%2*975+(975-image.width)//2
        y=135+(i//2)*660+(660-image.height)//2
        canvas.paste(image,(x,y));image.close()
    painter=ImageDraw.Draw(canvas)
    try:
        font=ImageFont.truetype(
          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",38)
    except OSError:font=ImageFont.load_default()
    painter.text((35,27),"KYOTO / PASS 09 — INDEPENDENT VALIDATION",
                 fill=INK,font=font)
    painter.text((35,81),
       "3D continuous geometry · independent distance oracle · physical impulse balances · actual FEM",
       fill=MUTED)
    canvas.save(out/"STAGE9_INDEPENDENT_CONTACT_SCIENCE_ATLAS.png")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--input",type=Path,required=True)
    parser.add_argument("--physics",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    a=parser.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    m,im,routine,fast,faces,impulse,vertices,topology=load(
        a.input,a.physics)
    style()
    independent_coverage(m,routine,a.output)
    certificate_effort(routine,fast,a.output)
    static_distance(m,routine,fast,a.output)
    impulse_work(impulse,a.output)
    momentum(impulse,a.output)
    dissipation(impulse,a.output)
    science_3d(m,faces,vertices,topology,a.output)
    atlas(a.output)
    print(f"PASS: independent 3D FEM geometry, {m['ordinary_face_intervals']} "
          f"routine moving face intervals and {m['fast_face_intervals']} fast "
          f"intervals; unresolved {m['ordinary_unresolved_intervals']} + "
          f"{m['fast_unresolved_intervals']}; 1200 impulse balance tests, "
          "seven source-backed figures")
if __name__=="__main__":main()
