#!/usr/bin/env python3
"""Kyoto Pass 10 — source-backed finite-precision and whole-FEM momentum.

The data here are produced ONLY by original native C++ physical
simulation and independently executed numerical audits, not by
Python integration or a hand-painted collision animation.

A plane inequality verified with per-operation outward double
enclosures is a bounded-arithmetic certificate under explicit IEEE
assumptions. This is NOT a proof of all hardware FP modes, arbitrary
compiler transformations, general mesh-mesh CCD, or IPC speedup.

Whole-system momentum balances include gravity, viscous damping,
internal hyperelastic force, external rigid-sphere impulse, and a
discrete angular transport term. Mechanical energy need not remain
constant under this damped/frictional/time-discretized contact model.
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

BG="#071722";PANEL="#122a3d";INK="#f3f7fb";MUTED="#a3bdcf"
CYAN="#64d7ec";GOLD="#f4c384";GREEN="#71e2af";RED="#ff8b9f"
PURPLE="#c6a5f8";GRID="#32556a"

def theme():
    plt.rcParams.update({"figure.facecolor":BG,"savefig.facecolor":BG,
      "axes.facecolor":PANEL,"axes.edgecolor":GRID,
      "axes.labelcolor":MUTED,"text.color":INK,
      "xtick.color":MUTED,"ytick.color":MUTED,
      "grid.color":GRID,"grid.alpha":.32,"font.family":"DejaVu Sans"})
def header(fig,title,subtitle,foot):
    fig.text(.048,.964,title,size=16.5,weight="bold",va="top")
    fig.text(.048,.918,subtitle,size=9,color=MUTED,va="top")
    fig.text(.048,.020,foot,size=8,color=MUTED)
def save(fig,path):
    fig.savefig(path,dpi=170,bbox_inches="tight",pad_inches=.17)
    plt.close(fig)
def read(root,physical):
    numeric=json.loads((root/"rounded_manifest.json").read_text())
    mechanics=json.loads((root/"whole_fem_balance_manifest.json").read_text())
    routine=pd.read_csv(root/"rounded_routine_900.csv")
    fast=pd.read_csv(root/"rounded_fast_substeps.csv")
    face=pd.read_csv(root/"rounded_fast_1600_faces.csv")
    ledger=pd.read_csv(root/"whole_FEM_accepted_substep_balance.csv")
    geometry=pd.read_csv(physical/"fast_event_true_3d_states.csv")
    topology=pd.read_csv(physical/"fast_event_surface_faces.csv")
    scene=json.loads((physical/"manifest.json").read_text())
    assert numeric["ieee754_outward_intervals_used"]
    assert not numeric["formal_hardware_software_proof"]
    assert not numeric["general_mesh_mesh_CCD"]
    assert numeric["ordinary_macro_steps"]==900
    assert len(routine)==900 and len(fast)==numeric["fast_physical_substeps"]
    assert len(face)==numeric["fast_face_intervals"]==320*len(fast)
    assert len(ledger)==mechanics["routine_accepted_physical_substeps"]+\
         mechanics["fast_accepted_physical_substeps"]
    assert len(topology)==320 and len(geometry)==720
    assert routine.cumulative_strictly_certified.iloc[-1]==numeric["ordinary_strictly_proven"]
    assert fast.cumulative_strictly_certified.iloc[-1]==numeric["fast_strictly_proven"]
    assert int(face.strictly_certified.sum())==numeric["fast_strictly_proven"]
    assert (face.strict_lower_margin_m[face.strictly_certified==1]>0).all()
    assert (ledger.momentum_relative<1e-10).all()
    assert (ledger.angular_relative<1e-10).all()
    assert not mechanics["true_physics_calibration_proven"]
    return numeric,mechanics,routine,fast,face,ledger,geometry,topology,scene

def coverage(numeric,routine,fast,out):
    fig,axes=plt.subplots(2,1,figsize=(12.9,7.1),sharex=False)
    fig.subplots_adjust(left=.10,right=.94,top=.80,bottom=.15,hspace=.36)
    fraction=100*routine.cumulative_strictly_certified/\
        routine.cumulative_triangle_intervals
    axes[0].plot(routine.time_s,fraction,color=GREEN,lw=2.5)
    axes[0].set(xlabel="Integrated FEM physical time / seconds",
                ylabel="Outward-enclosed strict proof / %",
                ylim=(0,104))
    axes[0].grid(True)
    fraction_fast=100*fast.cumulative_strictly_certified/\
        fast.cumulative_face_intervals
    axes[1].plot(np.arange(len(fast)),fraction_fast,
                 marker="o",color=CYAN,lw=2.5)
    axes[1].set(xlabel="Fast-event accepted physical substep",
                ylabel="Fast interval proof coverage / %",
                ylim=(0,104),xticks=np.arange(len(fast)))
    axes[1].grid(True)
    header(fig,"01 / OUTWARD-ENCLOSED CONTINUOUS PROOF COVERAGE",
       "Every moving physical triangle is evaluated with binary64 interval LOWER dot and UPPER sphere support.",
       "Scope: triangle versus analytic sphere; IEEE assumptions documented; unresolved intervals can never be labeled safe.")
    save(fig,out/"01_outward_interval_coverage.png")

def paid(numeric,routine,fast,out):
    fig,ax=plt.subplots(figsize=(12.8,6.8))
    fig.subplots_adjust(left=.10,right=.91,bottom=.17,top=.80)
    x=routine.time_s
    a=ax.plot(x,routine.outward_endpoint_dot_products,
              color=CYAN,lw=2,label="Outward interval endpoint dot checks")[0]
    second=ax.twinx()
    b=second.plot(x,routine.proposed_planes,
             color=GOLD,lw=1.9,label="Candidate planes tested")[0]
    ax.set(xlabel="Actual solver physical time / seconds",
           ylabel="Cumulative outward-enclosed endpoint projections")
    second.set_ylabel("Cumulative candidate separating planes",color=GOLD)
    second.tick_params(axis="y",colors=GOLD)
    ax.legend(handles=[a,b],frameon=False,labelcolor=INK,fontsize=9)
    ax.grid(True)
    header(fig,"02 / THE PROOF'S REAL COMPUTATIONAL COST",
       "All original interval projections and plane proposals counted, including successful and unsuccessful candidates.",
       "Operation counts, NOT wall-clock speed comparisons against the official IPC LBVH or SCA maintained frontier.")
    save(fig,out/"02_rounded_proof_paid_work.png")

def margins(numeric,routine,fast,out):
    fig,axes=plt.subplots(1,2,figsize=(13,6.8))
    fig.subplots_adjust(left=.10,right=.96,bottom=.20,top=.79,wspace=.34)
    a=np.maximum(routine.min_strict_margin_m.to_numpy()*1000,1e-12)
    b=np.maximum(fast.min_strict_margin_m.to_numpy()*1000,1e-12)
    axes[0].plot(routine.time_s,a,color=GREEN,lw=1.8)
    axes[0].set(xlabel="Routine elapsed time / seconds",
                ylabel="Strictly verified support margin / mm")
    axes[1].plot(np.arange(len(fast)),b,color=GOLD,
                 lw=2,marker="o")
    axes[1].set(xlabel="Fast accepted physical substep",
                ylabel="Strictly verified support margin / mm",
                xticks=np.arange(len(fast)))
    for ax in axes:
        ax.set_yscale("log");ax.grid(True)
    header(fig,"03 / HOW FAR THE PROOF IS FROM THE NUMERICAL BOUNDARY",
       "Source lower bounds are conservative plane-support margins, not the exact closest-point triangle gaps.",
       "A strictly positive outward-bound margin certifies separation under the stated IEEE operation model.")
    save(fig,out/"03_strict_positive_separation_margins.png")

def momentum(ledger,out):
    routine=ledger[ledger.scenario=="routine"].sort_values("time_s")
    fast=ledger[ledger.scenario=="fast"].sort_values("time_s")
    fig,axes=plt.subplots(2,1,figsize=(12.9,7.4))
    fig.subplots_adjust(left=.10,right=.95,bottom=.14,top=.78,hspace=.43)
    axes[0].semilogy(routine.time_s,
        np.maximum(routine.momentum_relative,1e-19),
        color=CYAN,lw=1.5,label="Whole FEM linear P relative residual")
    axes[0].semilogy(routine.time_s,
        np.maximum(routine.angular_relative,1e-19),
        color=PURPLE,lw=1.5,label="Whole FEM angular L relative residual")
    axes[0].set(xlabel="Routine physical time / seconds",
                ylabel="Discrete balance relative residual")
    axes[0].grid(True);axes[0].legend(frameon=False,labelcolor=INK,fontsize=9)
    axes[1].semilogy(np.arange(len(fast)),
        np.maximum(fast.momentum_relative,1e-19),
        color=CYAN,lw=1.9,marker="o",label="Fast P")
    axes[1].semilogy(np.arange(len(fast)),
        np.maximum(fast.angular_relative,1e-19),
        color=PURPLE,lw=1.9,marker="o",label="Fast L")
    axes[1].set(xlabel="Fast collision actual physical leaf",
                ylabel="Balance relative residual",
                xticks=np.arange(len(fast)))
    axes[1].grid(True);axes[1].legend(frameon=False,labelcolor=INK)
    header(fig,"04 / THE ENTIRE 3D FEM MOMENTUM LEDGER",
       "Mass-weighted actual nodes: gravity, damping, internal elastic force, external sphere impulse and angular transport.",
       "This is discrete algebraic balance, not a claim of experimentally correct material or global energy-conserving dynamics.")
    save(fig,out/"04_full_FEM_linear_angular_momentum_residual.png")

def transport(ledger,out):
    routine=ledger[ledger.scenario=="routine"]
    fig,ax=plt.subplots(figsize=(12.8,6.8))
    fig.subplots_adjust(left=.10,right=.92,bottom=.18,top=.8)
    a=ax.plot(routine.time_s,
         routine.discrete_angular_transport_norm_nsm,
         color=GOLD,lw=1.8,
         label="Position-transport term in angular balance")[0]
    other=ax.twinx()
    b=other.plot(routine.time_s,
         routine.internal_torque_relative,
         color=CYAN,lw=1.4,
         label="Relative neo-Hookean internal torque residual")[0]
    ax.set(xlabel="Actual physical time / seconds",
           ylabel="Angular position transport / N·m·s")
    other.set_ylabel("Internal elastic torque cancellation / relative",
                    color=CYAN)
    other.tick_params(axis="y",colors=CYAN)
    ax.grid(True);ax.legend(handles=[a,b],frameon=False,labelcolor=INK,
                            fontsize=9)
    header(fig,"05 / ANGULAR BALANCE NEEDS A MOVING-GEOMETRY TERM",
       "Discrete positions move during the explicit FEM step: including transport avoids a false angular conservation claim.",
       "The neo-Hookean internal torques nearly cancel by stress symmetry; transport is mathematical, not a separate force.")
    save(fig,out/"05_angular_transport_and_internal_torque.png")

def mechanical(ledger,out):
    routine=ledger[ledger.scenario=="routine"].copy()
    fig,axes=plt.subplots(2,1,figsize=(12.8,7.3),sharex=True)
    fig.subplots_adjust(left=.10,right=.94,bottom=.14,top=.79,hspace=.24)
    axes[0].plot(routine.time_s,
         routine.mechanical_energy_change_j.cumsum(),
         color=GREEN,lw=2.0)
    axes[0].set_ylabel("Cumulative Δ(kinetic + elastic + gravity) / J")
    axes[0].grid(True)
    axes[1].plot(routine.time_s,
         routine.mechanical_energy_change_j,
         color=GOLD,lw=1.1,label="Per-step actual Δ mechanical energy")
    second=axes[1].twinx()
    second.plot(routine.time_s,routine.face_contact_updates,
                color=RED,lw=1.2,alpha=.72,
                label="Barycentric contact impulse updates")
    second.set_ylabel("Contact updates / step",color=RED)
    second.tick_params(axis="y",colors=RED)
    axes[1].set(xlabel="True integrated time / seconds",
                ylabel="Measured energy change / joules")
    axes[1].grid(True)
    header(fig,"06 / ENERGY IS MEASURED, NOT ASSUMED CONSERVED",
       "Actual source FEM elastic, kinetic and gravity energy across every substep, with real contact impulses.",
       "Damping, friction, and finite-dt integration can change total mechanical energy; algebraic P/L balance is a different claim.")
    save(fig,out/"06_whole_FEM_energy_and_contact_work.png")

def sphere_xyz(center,r):
    theta=np.linspace(0,2*np.pi,41)
    phi=np.linspace(0,np.pi,24)
    return (center[0]+r*np.outer(np.cos(theta),np.sin(phi)),
            center[1]+r*np.outer(np.sin(theta),np.sin(phi)),
            center[2]+r*np.outer(np.ones_like(theta),np.cos(phi)))

def true_3d(numeric,face,geometry,topology,scene,out):
    last=face[face.leaf_id==face.leaf_id.max()].sort_values("face_id")
    native=geometry[
        geometry.algorithm=="adaptive_full_elapsed_time"
    ].sort_values("vertex_id")
    P=native[["x","y","z"]].to_numpy(dtype=float)
    I=topology[["a","b","c"]].to_numpy(dtype=int)
    stress=native.von_mises_pa.to_numpy(dtype=float)
    assert len(last)==len(I)==320 and len(P)==180
    sphere=sphere_xyz(scene["sphere_center_xyz_m"],
                      scene["fast_sphere_radius_m"])
    lower=last.strict_lower_margin_m.to_numpy()
    proof=last.strictly_certified.to_numpy(dtype=int)
    fig=plt.figure(figsize=(18.0,7.4))
    fig.subplots_adjust(left=.02,right=.98,top=.80,bottom=.07,wspace=.01)
    for side in range(2):
        ax=fig.add_subplot(1,2,side+1,projection="3d")
        if side==0:
            col=cm.viridis(np.clip(
                stress[I].mean(axis=1)/max(1.,stress.max()),0,1))
            caption="Original C++ neo-Hookean stress / Pa"
        else:
            lo=np.maximum(lower,1e-12)
            spread=np.log10(lo)
            denom=max(1e-9,float(spread.max()-spread.min()))
            col=cm.plasma(np.clip(
                (spread-spread.min())/denom,0,1))
            col[proof==0]=colors.to_rgba(RED,.96)
            caption="Strictly verified positive lower margin"
        mesh=Poly3DCollection(P[I],facecolors=col,
            linewidths=.09,edgecolors="#225264",alpha=.95)
        ax.add_collection3d(mesh)
        ax.plot_surface(*sphere,color=GOLD,alpha=.21,
                        linewidth=0,shade=False)
        ax.set(xlim=(-1.35,1.35),ylim=(-1.35,1.35),
               zlim=(-1.4,1.0))
        ax.set_box_aspect((2.7,2.7,2.4))
        ax.view_init(elev=28,azim=-52)
        ax.set_title(caption,color=INK,pad=7,fontsize=11)
        ax.tick_params(labelsize=7)
        for axis in (ax.xaxis,ax.yaxis,ax.zaxis):
            axis.pane.set_facecolor((.02,.08,.12,1))
            axis.pane.set_edgecolor(GRID)
            axis._axinfo["grid"]["color"]=(.17,.27,.35,.16)
    header(fig,"07 / ROUNDED WITNESSES ON REAL DEFORMABLE 3D GEOMETRY",
      "Two different fields on the SAME source tetrahedral FEM: computed Cauchy stress and conservative per-face lower proof margin.",
      "Red would flag unresolved. The curved sphere and 3D positions are the native source simulation; no scripted collision.")
    save(fig,out/"07_3D_FEM_rounded_witness_margin.png")

def interactive_true_fem(face,geometry,topology,scene,out):
    """Actual source topology and two DIFFERENT measured scalar fields."""
    import plotly.graph_objects as go
    last=face[face.leaf_id==face.leaf_id.max()].sort_values("face_id")
    native=geometry.loc[
        geometry.algorithm=="adaptive_full_elapsed_time"
    ].sort_values("vertex_id")
    P=native[["x","y","z"]].to_numpy(dtype=float)
    I=topology[["a","b","c"]].to_numpy(dtype=int)
    stress=native.von_mises_pa.to_numpy(dtype=float)
    margins=last.strict_lower_margin_m.to_numpy(dtype=float)
    certified=last.strictly_certified.to_numpy(dtype=int)
    assert len(P)==180 and len(I)==len(margins)==320
    assert (margins[certified==1]>0).all()
    center=scene["sphere_center_xyz_m"]
    radius=float(scene["fast_sphere_radius_m"])
    x,y,z=sphere_xyz(center,radius)
    fig=go.Figure()
    common=dict(x=P[:,0],y=P[:,1],z=P[:,2],
         i=I[:,0],j=I[:,1],k=I[:,2],
         flatshading=False,opacity=0.97,
         lighting=dict(ambient=.45,diffuse=.70,
                       specular=.5,roughness=.55))
    fig.add_trace(go.Mesh3d(
        **common,intensity=stress,intensitymode="vertex",
        colorscale="Viridis",cmin=0,cmax=max(1.,float(stress.max())),
        colorbar=dict(title="Cauchy von Mises (Pa)"),
        visible=True,name="Native FEM Cauchy stress"))
    fig.add_trace(go.Mesh3d(
        **common,
        intensity=np.log10(np.maximum(margins,1e-15)),
        intensitymode="cell",colorscale="Plasma",
        colorbar=dict(title="log10(strict margin / m)"),
        visible=False,name="Outward lower proof margin"))
    fig.add_trace(go.Surface(x=x,y=y,z=z,
        surfacecolor=np.zeros_like(x),
        colorscale=[[0,GOLD],[1,GOLD]],
        showscale=False,opacity=.32,
        visible=True,name="Actual analytic rigid sphere"))
    titles=["ACTUAL FEM COMPUTED STRESS",
            "STRICT IEEE OUTWARD-INTERVAL LOWER PROOF MARGINS"]
    buttons=[]
    for i,name in enumerate(titles):
        buttons.append(dict(label=name,method="update",
            args=[{"visible":[i==0,i==1,True]},
                  {"title":"KYOTO / PASS 10 — "+name+
                   "<br><sup>Real 180-node/320-face FEM, source 3D rigid sphere."
                   " Conditional IEEE arithmetic proof; no general IPC mesh CCD.</sup>"}]))
    fig.update_layout(
        title="KYOTO / PASS 10 — ACTUAL 3D FEM STRESS",
        paper_bgcolor=BG,plot_bgcolor=BG,
        font=dict(color=INK),height=820,
        margin=dict(l=10,r=10,b=15,t=115),
        updatemenus=[dict(type="dropdown",showactive=True,
            x=.02,y=1.09,buttons=buttons)],
        scene=dict(bgcolor=BG,aspectmode="manual",
            aspectratio=dict(x=1,y=1,z=.92),
            xaxis=dict(title="X (m)",range=[-1.35,1.35],
                       gridcolor=GRID),
            yaxis=dict(title="Y (m)",range=[-1.35,1.35],
                       gridcolor=GRID),
            zaxis=dict(title="Z (m)",range=[-1.4,1.0],
                       gridcolor=GRID),
            camera=dict(eye=dict(x=1.62,y=-1.87,z=1.27))))
    fig.write_html(out/"08_interactive_3D_stress_and_rounded_clearance.html",
                   full_html=True,include_plotlyjs=True)

def atlas(out):
    items=["01_outward_interval_coverage.png",
           "02_rounded_proof_paid_work.png",
           "03_strict_positive_separation_margins.png",
           "04_full_FEM_linear_angular_momentum_residual.png",
           "05_angular_transport_and_internal_torque.png",
           "06_whole_FEM_energy_and_contact_work.png"]
    img=Image.new("RGB",(1950,3010),BG)
    for k,name in enumerate(items):
        original=Image.open(out/name).convert("RGB")
        original.thumbnail((940,605))
        x=k%2*975+(975-original.width)//2
        y=155+(k//2)*620+(620-original.height)//2
        img.paste(original,(x,y));original.close()
    hero=Image.open(out/"07_3D_FEM_rounded_witness_margin.png").convert("RGB")
    hero.thumbnail((1900,820))
    img.paste(hero,((1950-hero.width)//2,2140))
    hero.close()
    draw=ImageDraw.Draw(img)
    try:
        font=ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",40)
    except OSError:font=ImageFont.load_default()
    draw.text((38,27),"KYOTO / PASS 10 — FP BOUNDS & PHYSICS",
              fill=INK,font=font)
    draw.text((38,88),
       "Outward IEEE intervals  |  3D momentum/energy ledger  |  actual FEM surface witness margins",
       fill=MUTED)
    img.save(out/"STAGE10_ROUNDED_CONTACT_AND_FEM_PHYSICS_ATLAS.png")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--physics",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    n,m,r,f,face,ledger,verts,topology,scene=read(a.input,a.physics)
    theme()
    coverage(n,r,f,a.output)
    paid(n,r,f,a.output)
    margins(n,r,f,a.output)
    momentum(ledger,a.output)
    transport(ledger,a.output)
    mechanical(ledger,a.output)
    true_3d(n,face,verts,topology,scene,a.output)
    interactive_true_fem(face,verts,topology,scene,a.output)
    atlas(a.output)
    print(f"PASS: outward-enclosed sphere intervals "
          f"{n['ordinary_strictly_proven']}/{n['ordinary_face_intervals']} "
          f"ordinary + {n['fast_strictly_proven']}/{n['fast_face_intervals']} "
          f"fast; actual full FEM momentum/energy ledger {len(ledger)} "
          "accepted substeps; seven independent source-backed figures")

if __name__=="__main__":
    main()
