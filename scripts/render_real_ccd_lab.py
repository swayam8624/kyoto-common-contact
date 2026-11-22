#!/usr/bin/env python3
"""Stage 4 source-backed *actual official IPC* continuous-CCD visual evidence.

Reads ONLY the C++23 upstream CCD test receipts. Does not re-solve or
invent collision dynamics. Trajectories are linear (a restriction of the
official upstream CCD API). Conservative TOI is not necessarily exact impact
time; independent analytic constructions provide event truth on named cases.
"""
from __future__ import annotations
import argparse,csv,json
from pathlib import Path
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection,Line3DCollection
BG="#081521";PANEL="#102337";TEXT="#eaf2fb"
MUTED="#a1b9cc";GRID="#2d485d"
CYAN="#57d7e8";AMBER="#f2b76f";GREEN="#59d8ab";PINK="#fa8298"
def style():
    plt.rcParams.update({"font.family":"DejaVu Sans","figure.facecolor":BG,
        "axes.facecolor":PANEL,"savefig.facecolor":BG,"text.color":TEXT,
        "axes.labelcolor":MUTED,"axes.edgecolor":GRID,
        "xtick.color":MUTED,"ytick.color":MUTED,
        "grid.color":GRID,"grid.alpha":.3})
def header(fig,title,subtitle):
    fig.text(.06,.955,title,size=18,weight="bold")
    fig.text(.06,.912,subtitle,size=10,color=MUTED)
def footer(fig,label):
    fig.text(.06,.025,label,size=8,color=MUTED)
def save(fig,dst,name):
    fig.savefig(dst/name,dpi=180,bbox_inches="tight",pad_inches=.17)
    plt.close(fig)
def read(path):
    with path.open(newline="") as f:return list(csv.DictReader(f))
def load(p):
    manifest=json.loads((p/"manifest.json").read_text())
    assert manifest["upstream"]=="real IPC Toolkit v1.6.1 pinned release"
    assert not manifest["physics_solver"]
    cases=read(p/"ccd_cases.csv")
    samples=read(p/"linear_vertex_paths.csv")
    assert len(cases)==35
    assert all(int(x["total_events"])==int(x["exhaustive_events"]) for x in cases)
    points=defaultdict(lambda:defaultdict(dict))
    for r in samples:
        points[r["case_name"]][int(r["frame"])][int(r["vertex_id"])]=np.array(
            [float(r[k]) for k in ("x","y","z")])
    return cases,points
def axis3d(ax,limits=None):
    ax.view_init(elev=24,azim=-57)
    ax.set_proj_type("persp")
    ax.set_facecolor(BG)
    for a in (ax.xaxis,ax.yaxis,ax.zaxis):
        a.pane.set_facecolor((.04,.08,.13,1))
        a.pane.set_edgecolor(GRID)
        a._axinfo["grid"]["color"]=(.2,.3,.4,.4)
    ax.set_xlabel("X / world units",color=MUTED,labelpad=8)
    ax.set_ylabel("Y / world units",color=MUTED,labelpad=8)
    ax.set_zlabel("Z / world units",color=MUTED,labelpad=6)
    if limits:
        ax.set_xlim(*limits[0]);ax.set_ylim(*limits[1]);ax.set_zlim(*limits[2])
    ax.set_box_aspect((1.15,1.15,1))
def vf_3d(ax,pts):
    tri=np.array([pts[60][i] for i in (0,1,2)])
    ax.add_collection3d(Poly3DCollection([tri],facecolors=AMBER,
        edgecolors=AMBER,linewidths=2,alpha=.44))
    ax.scatter(tri[:,0],tri[:,1],tri[:,2],color=AMBER,s=65,depthshade=False)
    path=np.array([pts[k][3] for k in range(121)])
    ax.plot(*path.T,color=CYAN,lw=4,label="Moving vertex")
    ax.scatter(*path[::12].T,color=CYAN,s=23,depthshade=False)
    ax.scatter(*path[60],s=230,facecolors="none",
               edgecolors=GREEN,linewidths=2.5,depthshade=False)
    axis3d(ax,((-.3,1.25),(-.3,1.25),(-1.2,1.2)))
def ee_3d(ax,pts):
    for k in (0,1):
        p=pts[60][k]
        ax.scatter(*p,color=AMBER,s=50,depthshade=False)
    a=pts[60][0];b=pts[60][1]
    ax.plot(*np.array([a,b]).T,color=AMBER,lw=4)
    for f,col,alpha in [(0,CYAN,.28),(30,CYAN,.45),(60,GREEN,1.),(90,CYAN,.45),(120,CYAN,.28)]:
        a,b=pts[f][2],pts[f][3]
        ax.plot(*np.array([a,b]).T,color=col,lw=3,alpha=alpha)
    ax.scatter([0],[0],[0],s=210,facecolors="none",
               edgecolors=GREEN,linewidths=2,depthshade=False)
    axis3d(ax,((-1.2,1.2),(-1.2,1.2),(-1.2,1.2)))
def plot_geometry(out,points):
    fig=plt.figure(figsize=(14,7.8))
    fig.subplots_adjust(left=.03,right=.97,top=.85,bottom=.10,wspace=.04)
    ax=fig.add_subplot(121,projection="3d")
    vf_3d(ax,points["VF_TRUE_MIDSTEP"])
    ax.set_title("Vertex travels through triangle interior",color=TEXT,pad=18)
    ax=fig.add_subplot(122,projection="3d")
    ee_3d(ax,points["EE_TRUE_MIDSTEP"])
    ax.set_title("Moving edge crosses stationary edge",color=TEXT,pad=18)
    header(fig,"01  /  ACTUAL 3D CONTINUOUS IMPACTS",
          "Upstream IPC narrow-phase CCD detects both analytic mid-step events despite disjoint endpoints.")
    footer(fig,"C++ linear trajectories + actual upstream CCD; cyan = moving path, gold = stationary features, green = analytic crossing.")
    save(fig,out,"01_verified_3d_continuous_crossings.png")
def plot_nearmiss(out,points):
    fig,axes=plt.subplots(1,2,figsize=(13,6.8))
    fig.subplots_adjust(left=.095,right=.965,bottom=.16,top=.80,wspace=.23)
    x=points["VF_TRUE_MIDSTEP"]; y=points["VF_BOX_FALSE_POSITIVE"]
    for ax,pts,title,col in [
       (axes[0],x,"Inside triangle: genuine contact",GREEN),
       (axes[1],y,"Outside triangle: AABB false positive",PINK)]:
        times=np.array([k/120 for k in range(121)])
        coord=np.array([pts[k][3][2] for k in range(121)])
        ax.plot(times,coord,color=CYAN,lw=3)
        ax.axhline(0,color=AMBER,lw=2,ls="--",label="Face plane z=0")
        ax.axvline(.5,color=col,lw=1.2,ls=":")
        ax.scatter([.5],[0],color=col,s=110,zorder=4)
        ax.set(xlim=(0,1),ylim=(-1.15,1.15),
               xlabel="Normalized time",
               ylabel="Vertex z / world units")
        ax.grid(True)
        ax.set_title(title,color=TEXT,pad=13,fontsize=12)
    header(fig,"02  /  OVERLAPPING BOXES ≠ CONTINUOUS CONTACT",
        "Both vertices cross z=0 halfway; only the one inside the finite triangle really collides.")
    footer(fig,"The full 3D triangle/vertex geometry is tested by IPC. A plane crossing alone is insufficient.")
    save(fig,out,"02_midstep_geometry_vs_false_positive.png")
def plot_funnel(out,cases):
    categories=[
        ("Constructive / control", [c for c in cases if not c["case_name"].startswith("SEEDED_")]),
        ("28 randomized topology", [c for c in cases if c["case_name"].startswith("SEEDED_")])]
    fig,ax=plt.subplots(figsize=(12.5,6.8))
    fig.subplots_adjust(left=.10,right=.965,bottom=.18,top=.80)
    x=np.arange(len(categories))
    for i,(label,rows) in enumerate(categories):
        candidates=sum(int(r["total_candidates"]) for r in rows)
        events=sum(int(r["total_events"]) for r in rows)
        ax.bar(i-.18,candidates,width=.32,color=AMBER,label="LBVH candidates" if i==0 else None)
        ax.bar(i+.18,events,width=.32,color=GREEN,label="Actual IPC CCD events" if i==0 else None)
        for v,shift in ((candidates,-.18),(events,.18)):
            ax.text(i+shift,v+max(1,.025*max(candidates,1)),str(v),ha="center",color=TEXT)
    ax.set_xticks(x,[v[0] for v in categories])
    ax.set(ylabel="Aggregated counts (independent cases)")
    ax.grid(axis="y")
    ax.legend(frameon=False,labelcolor=TEXT,loc="upper left")
    header(fig,"03  /  FROM CANDIDATE TO VERIFIED EVENT",
           "Official IPC v1.6.1 LBVH candidates → actual official narrow-phase CCD outcomes.")
    footer(fig,"Aggregation is across test fixtures, not production solver frames or an algorithm speed comparison.")
    save(fig,out,"03_true_event_filtering.png")
def plot_oracle(out,cases):
    arr=np.array([[int(c["total_events"]),int(c["exhaustive_events"])] for c in cases])
    fig,ax=plt.subplots(figsize=(12.2,6.8))
    fig.subplots_adjust(left=.105,right=.96,bottom=.15,top=.81)
    x=np.arange(len(cases))
    ax.plot(x,arr[:,1],color=AMBER,lw=1.6,ls="--",label="All-pairs upstream CCD (enumeration oracle)")
    ax.scatter(x,arr[:,0],s=52,color=GREEN,marker="o",
        label="LBVH-filtered upstream CCD",zorder=5)
    ax.set(xlim=(-.5,len(cases)-.5),
           xlabel="Deterministic geometry fixture index (0–34)",
           ylabel="Independent observed CCD event count")
    ax.grid(True);ax.legend(frameon=False,labelcolor=TEXT)
    header(fig,"04  /  NO MISSING CCD EVENTS IN TESTED SCENES",
       "Exact event-set agreement between independent all-pairs enumeration and LBVH-filtered narrow phase.")
    footer(fig,"Both paths call the SAME upstream CCD math; this proves tested coverage, not independent mathematical CCD correctness.")
    save(fig,out,"04_official_ccd_event_coverage.png")
def plot_toi(out,cases):
    selected=[c for c in cases if float(c["analytic_toi"])>0]
    fig,ax=plt.subplots(figsize=(11.8,6.6))
    fig.subplots_adjust(left=.11,right=.96,bottom=.19,top=.80)
    x=np.arange(len(selected))
    analytic=[float(c["analytic_toi"]) for c in selected]
    actual=[float(c["ccd_earliest_toi"]) for c in selected]
    ax.bar(x-.18,analytic,width=.32,color=AMBER,label="Analytical geometric contact time")
    ax.bar(x+.18,actual,width=.32,color=CYAN,label="Conservative upstream CCD TOI")
    ax.set_xticks(x,[c["case_name"].replace("_"," ").replace("TRUE ","") for c in selected],
                  rotation=12,ha="right")
    ax.set_ylim(0,1)
    ax.set(ylabel="Normalized simulation step t")
    ax.grid(axis="y");ax.legend(frameon=False,labelcolor=TEXT,loc="upper right")
    header(fig,"05  /  CONSERVATIVE TIME-OF-IMPACT",
        "The CCD result can report a safe pre-impact fraction; it need not equal the analytical impact time.")
    footer(fig,"Only analytic constructed events included; no error bars or unsupported certainty claims.")
    save(fig,out,"05_analytic_vs_conservative_toi.png")
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    cases,points=load(args.input)
    for name in ("VF_TRUE_MIDSTEP","VF_BOX_FALSE_POSITIVE","EE_TRUE_MIDSTEP"):
        assert name in points and len(points[name])==121
    style()
    plot_geometry(args.output,points)
    plot_nearmiss(args.output,points)
    plot_funnel(args.output,cases)
    plot_oracle(args.output,cases)
    plot_toi(args.output,cases)
    print("PASS: five native-source figures of real upstream IPC continuous CCD")

if __name__=="__main__": main()
