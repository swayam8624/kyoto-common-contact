#!/usr/bin/env python3
"""Source-backed official IPC audit figures for true generated FEM trajectories.

NEVER equate IPC candidate hits to collision impulses or physical contact
force. The official CCD only inspects selected accepted solver steps, and
self-collision is not resolved by the analytical floor-only solver.
"""
import argparse, json, pathlib
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BG="#081523";PANEL="#10273a";FG="#eaf4fc";MUTED="#9cb9cb"
CYAN="#4dd5e7";RED="#ff7186";GREEN="#6bdeaa";AMBER="#f3b66f";GRID="#2a485e"
def style():
    plt.rcParams.update({"font.family":"DejaVu Sans","figure.facecolor":BG,
        "axes.facecolor":PANEL,"savefig.facecolor":BG,
        "axes.labelcolor":MUTED,"axes.edgecolor":GRID,"text.color":FG,
        "xtick.color":MUTED,"ytick.color":MUTED,
        "grid.color":GRID,"grid.alpha":.25})
def render(fig,root,name,title,subtitle,foot):
    fig.text(.06,.961,title,size=17,weight="bold")
    fig.text(.06,.918,subtitle,color=MUTED,size=9)
    fig.text(.06,.022,foot,color=MUTED,size=8)
    fig.savefig(root/name,dpi=180,bbox_inches="tight",pad_inches=.13)
    plt.close(fig)
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=pathlib.Path,required=True)
    p.add_argument("--output",type=pathlib.Path,required=True)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    info=json.loads((a.input/"manifest.json").read_text())
    assert info["external_reference"]=="ACTUAL_PINNED_IPC_TOOLKIT_1.6.1"
    assert not info["all_900_steps_independently_CCD_audited"]
    df=pd.read_csv(a.input/"official_ipc_on_real_fem.csv")
    assert len(df)==info["sampled_steps"]>=50
    assert np.all(np.isfinite(df.select_dtypes(include=[np.number]).values))
    t=df.time_s
    candidates=df.ipc_cross_vf_candidates+df.ipc_cross_ee_candidates
    collisions=df.ipc_cross_vf_ccd_events+df.ipc_cross_ee_ccd_events
    style()
    fig,ax=plt.subplots(figsize=(12.4,7))
    fig.subplots_adjust(left=.1,right=.94,top=.82,bottom=.15)
    ax.plot(t,df.ipc_cross_vf_candidates,color=CYAN,lw=2.3,label="Cross-object vertex-face AABB candidates")
    ax.plot(t,df.ipc_cross_ee_candidates,color=AMBER,lw=2.0,label="Cross-object edge-edge AABB candidates")
    ax.scatter(t,collisions,c=RED,s=23,label="Actual IPC CCD events / sampled step",zorder=5)
    ax.set(xlabel="Native solver-integrated physical time (seconds)",
           ylabel="Candidate / event counts on audited steps")
    ax.grid(True);ax.legend(frameon=False,labelcolor=FG,fontsize=9)
    render(fig,a.output,"01_upstream_candidate_and_ccd_counts.png",
       "01 / ACTUAL IPC CCD ON REAL FEM TRAJECTORIES",
       "Upstream LBVH candidate families vs official continuous CCD, on sampled accepted physics steps.",
       "Not a fully audited 900-step continuum, and no general mesh-contact response is enforced.")
    fig,ax=plt.subplots(figsize=(12.3,7))
    fig.subplots_adjust(left=.1,right=.94,top=.82,bottom=.15)
    ax.bar(t,df.solver_plane_impulse_events,width=.006,color=GREEN,
           alpha=.7,label="Actual analytic floor impulse vertices")
    ax.plot(t,collisions,color=RED,lw=1.8,marker=".",
            label="Upstream cross-object CCD hits on accepted steps")
    ax.set(xlabel="Native physical time (seconds)",
           ylabel="Events on sampled integration step")
    ax.grid(axis="y");ax.legend(frameon=False,labelcolor=FG)
    render(fig,a.output,"02_plane_impulse_vs_ipc_ccd.png",
       "02 / IMPULSES AND CCD ARE NOT INTERCHANGEABLE",
       "The solver records discrete plane impulses; upstream IPC observes geometric linear-step events.",
       "Two different physical/numerical notions. Agreement in timing is informative, not an exact equivalence proof.")
    fig,ax=plt.subplots(figsize=(12.2,7))
    fig.subplots_adjust(left=.095,right=.94,top=.82,bottom=.15)
    ax.plot(t,df.ipc_build_query_ms,color=CYAN,lw=2,label="Real upstream LBVH build+query")
    ax.plot(t,df.ipc_ccd_paid_ms,color=AMBER,lw=2,label="Real upstream narrow-phase CCD")
    ax.set(xlabel="Native solver time (seconds)",
           ylabel="Paid IPC work per audited step (milliseconds)")
    ax.grid(True);ax.legend(frameon=False,labelcolor=FG)
    render(fig,a.output,"03_upstream_paid_cost_per_solver_step.png",
       "03 / EXTERNAL IPC PAID COST PROFILE",
       "Native recorded upstream build/query and narrow-phase times on one GitHub runner session.",
       "One trace, uneven selected steps, no randomized paired baseline or solver speedup claim.")
    fig,ax=plt.subplots(figsize=(12.1,7))
    fig.subplots_adjust(left=.10,right=.94,top=.82,bottom=.15)
    selfdata=df[df.self_audit_performed==1]
    ax.bar(selfdata.time_s-.002,
           selfdata.ipc_self_vf_candidates+selfdata.ipc_self_ee_candidates,
           width=.004,color=AMBER,label="Self-contact broad-phase candidates")
    ax.bar(selfdata.time_s+.002,
           selfdata.ipc_self_vf_ccd_events+selfdata.ipc_self_ee_ccd_events,
           width=.004,color=RED,label="Self-contact CCD event signals")
    ax.set(xlabel="Selected solver time (seconds)",
           ylabel="Self-contact candidate / event counts")
    ax.grid(axis="y");ax.legend(frameon=False,labelcolor=FG)
    render(fig,a.output,"04_unresolved_self_contact_survey.png",
       "04 / THE HONEST SELF-CONTACT AUDIT",
       "Official IPC sees the moving FEM surface, but the native plane solver does not resolve self-contact.",
       "Only a handful of selected steps are audited; zero observed events would NOT prove general self-contact safety.")
    fig,ax=plt.subplots(figsize=(12.2,7))
    fig.subplots_adjust(left=.095,right=.94,top=.82,bottom=.15)
    ax.plot(t,df.analytic_min_height_m,color=GREEN,lw=2.2,label="Accepted minimum solid vertex Z")
    ax.axhline(0,color=RED,ls="--",lw=1.5,label="Analytical ground boundary Z=0")
    ax.set(xlabel="Integrated solver physical time (seconds)",
           ylabel="Height above infinite rigid plane (metres)")
    ax.grid(True);ax.legend(frameon=False,labelcolor=FG)
    render(fig,a.output,"05_floor_constraint_no_penetration.png",
       "05 / NON-PENETRATION OF THE DECLARED PLANE",
       "Every accepted native solver vertex respects the infinite z≥0 constraint on inspected steps.",
       "This is an analytic plane constraint, not a proof against arbitrary triangle-surface interpenetration.")
    report=a.output/"AUDIT_SUMMARY.md"
    with report.open("w") as f:
        f.write("# Actual IPC on solver-generated hyperelastic trajectories\n\n")
        f.write(f"- Original integrated physical steps: {info['total_solver_steps']}\n")
        f.write(f"- Steps externally audited: {info['sampled_steps']}\n")
        f.write(f"- Total sampled IPC cross-body CCD events: {int(collisions.sum())}\n")
        f.write(f"- Total sampled IPC self CCD signals: {int((df.ipc_self_vf_ccd_events+df.ipc_self_ee_ccd_events).sum())}\n")
        f.write(f"- Max paid upstream IPC per sampled step: {float((df.ipc_build_query_ms+df.ipc_ccd_paid_ms).max()):.6g} ms\n\n")
        f.write("**Limit:** partial sampled audit only, with all CCD arithmetic from upstream IPC. The physical solver resolves only a flat ground plane, not self-contact or general mesh collisions.\n")
    print("PASS: 5 plots + self-contact disclosure + native source audit summary")
if __name__=="__main__":main()
