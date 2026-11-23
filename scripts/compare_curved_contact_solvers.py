#!/usr/bin/env python3
"""Pass 7 independent A/B comparison of SAME actual FEM with two contact laws.

The old vertex-only curved-contact solver is a genuine signed negative
control: its 900-step native trajectory has real face penetration.
The new globally synchronized whole-triangle certificate must produce
zero accepted penetrating faces. Every plotted quantity is read from
two independent C++ exporters; do not generate or overwrite physics.

This is a CORRECTNESS comparison, NOT a matched paid speed benchmark.
Different contact responses produce different physical trajectories.
"""
from __future__ import annotations
import argparse,csv,json
from pathlib import Path
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
BG="#081722";INK="#eff5fb";MUTED="#9cbdce";GRID="#2c4b5f"
RED="#fc8797";CYAN="#53d7e8";GOLD="#fac27a";GREEN="#6ddbb1"
def style():
    plt.rcParams.update({"figure.facecolor":BG,"axes.facecolor":BG,
        "savefig.facecolor":BG,"axes.edgecolor":GRID,
        "axes.labelcolor":MUTED,"text.color":INK,
        "xtick.color":MUTED,"ytick.color":MUTED,
        "grid.color":GRID,"grid.alpha":.3,"font.family":"DejaVu Sans"})
def format_figure(fig,title,sub,footer,out,name):
    fig.text(.055,.953,title,size=16,weight="bold",va="top")
    fig.text(.055,.91,sub,size=9,color=MUTED,va="top")
    fig.text(.055,.02,footer,size=8,color=MUTED)
    fig.savefig(out/name,dpi=180,bbox_inches="tight",pad_inches=.2)
    plt.close(fig)
def load(root):
    m=json.loads((root/"curved_manifest.json").read_text())
    audit=pd.read_csv(root/"surface_sphere_clearance.csv")
    states=pd.read_csv(root/"curved_solid_states.csv")
    metrics=pd.read_csv(root/"curved_solid_metrics.csv")
    assert len(audit)==901 and len(metrics)==181 and len(states)==181*180
    return m,audit,states,metrics
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--old",type=Path,required=True)
    p.add_argument("--new",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    opt=p.parse_args()
    opt.output.mkdir(parents=True,exist_ok=True)
    old,oa,os,om=load(opt.old)
    new,na,ns,nm=load(opt.new)
    assert old["physical_motion"] and new["physical_motion"]
    assert old["vertices"]==new["vertices"]==180
    assert old["tetrahedra"]==new["tetrahedra"]==480
    assert old["steps"]==new["steps"]==900
    assert old["dt_s"]==new["dt_s"]==.00075
    assert old["states_with_static_triangle_sphere_overlap"]>0
    assert new["states_with_static_triangle_sphere_overlap"]==0
    assert (na.penetrating_triangles==0).all()
    assert (na.minimum_triangle_clearance_m>=0).all()
    assert (na.vertex_penetrations==0).all()
    assert (om.min_tet_jacobian>0).all() and (nm.min_tet_jacobian>0).all()
    assert len(os)==len(ns)==32580
    original_gaps=oa.minimum_triangle_clearance_m.to_numpy()
    guarded_gaps=na.minimum_triangle_clearance_m.to_numpy()
    delta=np.sqrt((os.x.to_numpy()-ns.x.to_numpy())**2+
                  (os.y.to_numpy()-ns.y.to_numpy())**2+
                  (os.z.to_numpy()-ns.z.to_numpy())**2)
    frame_difference=pd.DataFrame({
        "frame":np.repeat(np.arange(181),180),
        "vertex_id":np.tile(np.arange(180),181),
        "position_difference_m":delta
    }).groupby("frame").agg(
        mean_position_difference_m=("position_difference_m","mean"),
        max_position_difference_m=("position_difference_m","max")).reset_index()
    frame_difference["time_s"]=om.time_s
    table=pd.DataFrame({
        "time_s":oa.time_s,
        "old_triangle_clearance_m":original_gaps,
        "guarded_triangle_clearance_m":guarded_gaps,
        "old_penetrating_faces":oa.penetrating_triangles,
        "guarded_penetrating_faces":na.penetrating_triangles
    })
    table.to_csv(opt.output/"full_901_state_contact_comparison.csv",index=False)
    frame_difference.to_csv(opt.output/"trajectory_divergence.csv",index=False)
    report={
      "source":"independent native C++ volumetric FEM simulations with distinct contact laws",
      "scientific_scope":"full-face static accepted-state safety comparison; not matched performance",
      "steps_each":900,
      "face_audit_states_each":901,
      "old_vertex_only_states_with_face_penetration":int((oa.penetrating_triangles>0).sum()),
      "new_whole_face_states_with_face_penetration":int((na.penetrating_triangles>0).sum()),
      "old_worst_triangle_clearance_m":float(original_gaps.min()),
      "new_worst_triangle_clearance_m":float(guarded_gaps.min()),
      "old_vertex_inside_states":int((oa.vertex_penetrations>0).sum()),
      "new_vertex_inside_states":int((na.vertex_penetrations>0).sum()),
      "new_time_clipped_steps":new["time_clipped_steps"],
      "new_minimum_accepted_displacement_fraction":new["minimum_global_accepted_fraction"],
      "old_final_total_energy_j":float(om.mechanical_total_j.iloc[-1]),
      "new_final_total_energy_j":float(nm.mechanical_total_j.iloc[-1]),
      "old_final_com_height_m":float(om.center_z.iloc[-1]),
      "new_final_com_height_m":float(nm.center_z.iloc[-1]),
      "final_mean_vertex_trajectory_difference_m":float(frame_difference.mean_position_difference_m.iloc[-1]),
      "relevant_limitations":[
        "global accepted-fraction clipping modifies temporal consistency of physical integration",
        "new local barycentric Gauss-Seidel contact impulse is not a globally implicit constrained solve",
        "binary64 time-interval certificate not machine formally directed-rounded",
        "no general triangle-mesh rigid obstacle or self-contact",
        "not a matched paid wall-time performance benchmark"]
    }
    (opt.output/"comparison_summary.json").write_text(json.dumps(report,indent=2)+"\n")
    style()
    fig,axes=plt.subplots(2,1,figsize=(13.3,8),sharex=True)
    fig.subplots_adjust(top=.84,bottom=.12,left=.105,right=.96,hspace=.28)
    axes[0].plot(oa.time_s,1000*original_gaps,color=RED,lw=2,
                 label="Original vertex-only contact")
    axes[0].plot(na.time_s,1000*guarded_gaps,color=GREEN,lw=2,
                 label="Globally synchronized whole-face response")
    axes[0].axhline(0,color=GOLD,lw=1.5,ls="--")
    axes[0].set_ylabel("Minimum surface gap / mm")
    axes[0].legend(frameon=False,labelcolor=INK,fontsize=9)
    axes[0].grid(True)
    axes[1].fill_between(oa.time_s,0,oa.penetrating_triangles,
                         color=RED,alpha=.28)
    axes[1].plot(oa.time_s,oa.penetrating_triangles,color=RED,
                 lw=1.4,label="Old violating triangles")
    axes[1].plot(na.time_s,na.penetrating_triangles,color=GREEN,
                 lw=2,label="New violating triangles (zero)")
    axes[1].set(xlabel="True physically integrated step time / seconds",
                ylabel="Number of penetrating surface faces")
    axes[1].grid(True);axes[1].legend(frameon=False,labelcolor=INK)
    format_figure(fig,"01 / FULL-SURFACE CONTACT: BEFORE AND AFTER",
      "Same neo-Hookean volume, connected mesh, gravity and static sphere. Only the contact algorithm differs.",
      "No states removed; both solvers run 900 steps and independently audit all 901 accepted states.",
      opt.output,"01_full_face_safety_comparison.png")
    fig,ax=plt.subplots(figsize=(12.8,7))
    fig.subplots_adjust(left=.105,right=.95,top=.82,bottom=.15)
    ax.plot(om.time_s,om.mechanical_total_j,color=RED,lw=2,
            label="Old total mechanical energy")
    ax.plot(nm.time_s,nm.mechanical_total_j,color=GREEN,lw=2,
            label="New total mechanical energy")
    ax.set(xlabel="True solver physical time / seconds",
           ylabel="Kinetic + elastic + gravity energy / joules")
    ax.grid(True);ax.legend(frameon=False,labelcolor=INK)
    format_figure(fig,"02 / CONTACT SAFETY HAS A PHYSICAL COST",
      "Independent native energy histories with different contact laws; not a claim of mechanical-energy conservation.",
      "Damping, friction and global clipping all change energy; values need independent solver validation.",
      opt.output,"02_physics_energy_difference.png")
    fig,ax=plt.subplots(figsize=(12.8,7))
    fig.subplots_adjust(left=.105,right=.95,top=.82,bottom=.15)
    ax.plot(om.time_s,om.center_z,color=RED,lw=2,label="Old body center-of-mass Z")
    ax.plot(nm.time_s,nm.center_z,color=GREEN,lw=2,label="New body center-of-mass Z")
    twin=ax.twinx()
    twin.plot(frame_difference.time_s,
              frame_difference.mean_position_difference_m*1000,
              color=GOLD,lw=1.8,ls="--",label="Mean cross-solver vertex separation")
    ax.set(xlabel="Integrated time / seconds",ylabel="Body COM height / metres")
    twin.set_ylabel("Mean position difference / mm",color=GOLD)
    twin.tick_params(axis="y",colors=GOLD)
    ax.grid(True)
    ax.legend(handles=[ax.lines[0],ax.lines[1],twin.lines[0]],
              frameon=False,labelcolor=INK,fontsize=9)
    format_figure(fig,"03 / DYNAMICS MUST NOT BE COMPARED AS IDENTICAL",
      "Synchronized clipping and mass-weighted contact impulses change the solver's trajectory.",
      "These results show real trajectory differences, not performance speedup or verified physical accuracy.",
      opt.output,"03_dynamics_and_trajectory_divergence.png")
    print("PASS: 901 vs 901 full-face comparison, "
          f"{report['old_vertex_only_states_with_face_penetration']} old "
          "penetrating states versus 0 new, with independent energy/trajectory disclosure")
if __name__=="__main__":main()
