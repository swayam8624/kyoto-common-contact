#!/usr/bin/env python3
"""Source-backed scientific figures from the C++23 Kyoto motion exporter.
This renders 3D kinematic geometry, not a physics simulation or general CCD.
Reproducibility: raw C++ trajectory/envelopes CSV and manifest are mandatory.
"""
from __future__ import annotations
import argparse, csv, json
from collections import defaultdict
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from mpl_toolkits.mplot3d.art3d import Poly3DCollection, Line3DCollection

BG="#071320"; FG="#e4eef8"; MUTED="#8da9c0"
CYAN="#43d7ec"; ORANGE="#ffb566"; RED="#ed6179"
GREEN="#5be6b6"; GOLD="#e4d795"; GRID="#273b51"

def rows(path):
    with open(path,encoding="utf-8",newline="") as f:
        return list(csv.DictReader(f))

def theme():
    plt.rcParams.update({
        "font.family":"DejaVu Sans","font.size":11,
        "figure.facecolor":BG,"axes.facecolor":BG,"savefig.facecolor":BG,
        "axes.labelcolor":MUTED,"axes.edgecolor":GRID,"text.color":FG,
        "xtick.color":MUTED,"ytick.color":MUTED,"grid.color":GRID,
        "grid.alpha":.35,"axes.titlesize":15,"axes.titleweight":"bold"})

def scene_axis(ax,labels=("X / world units","Y / world units",
                         "Z / world units")):
    ax.set_facecolor(BG)
    for axis in (ax.xaxis,ax.yaxis,ax.zaxis):
        axis.pane.set_facecolor((.035,.08,.13,1))
        axis.pane.set_edgecolor(GRID)
        axis.line.set_color(GRID)
        axis._axinfo["grid"]["color"]=(.16,.26,.36,.34)
    ax.tick_params(labelsize=8,pad=0)
    ax.set_xlabel(labels[0],labelpad=11,color=MUTED)
    ax.set_ylabel(labels[1],labelpad=8,color=MUTED)
    ax.set_zlabel(labels[2],labelpad=5,color=MUTED)
    ax.view_init(elev=18,azim=-67)
    ax.set_proj_type("persp")

def as_triangle(points):
    if isinstance(points,dict):
        return np.array([points[k] for k in sorted(points)])
    return np.asarray(points)

def add_triangle(ax,points,color,alpha=.75):
    pts=as_triangle(points)
    ax.add_collection3d(Poly3DCollection([pts],facecolors=color,
                        edgecolors=color,linewidths=1.6,alpha=alpha))
    ax.scatter(pts[:,0],pts[:,1],pts[:,2],s=26,color=color,
               depthshade=False)

def edges(lo,hi):
    pts=np.array([[x,y,z] for x in (lo[0],hi[0])
                          for y in (lo[1],hi[1])
                          for z in (lo[2],hi[2])])
    return [[pts[i],pts[j]] for i in range(8)
           for j in range(i+1,8) if (i^j).bit_count()==1]

def plot_box(ax,lo,hi,color,alpha=.6,dashed=False,width=1.4):
    ax.add_collection3d(Line3DCollection(edges(lo,hi),colors=color,
       linewidths=width,alpha=alpha,
       linestyles="dashed" if dashed else "solid"))

def render_trajectory(out,p,en):
    fig=plt.figure(figsize=(14.3,7.7),dpi=170)
    ax=fig.add_axes([-.025,.08,.87,.80],projection="3d")
    scene_axis(ax)
    ax.set_xlim(-.3,4.5);ax.set_ylim(-.3,1.5);ax.set_zlim(-.3,1.5)
    ax.set_box_aspect((4.8,1.8,1.8),zoom=1.18)
    for k in range(3):
        trail=np.array([p[i][11][k] for i in sorted(p)])
        ax.plot(*trail.T,color=CYAN,lw=2.6,alpha=.9)
        ax.scatter(*trail[::8].T,color=CYAN,s=8,alpha=.35,
                   depthshade=False)
    add_triangle(ax,p[60][19],ORANGE,.72)
    add_triangle(ax,p[60][11],CYAN,.52)
    a=en[(11,"complete_control_hull")]
    b=en[(11,"endpoint_only")]
    plot_box(ax,a[:3],a[3:],CYAN,.70)
    plot_box(ax,b[:3],b[3:],RED,.95,True,2.3)
    ax.scatter([3],[.35],[.35],s=175,facecolors="none",
               edgecolors=GOLD,linewidths=2.6,depthshade=False)
    ax.text(3.1,1.1,1.1,"t=0.5 | vertices coincide",color=FG,size=10)
    fig.text(.055,.94,"01  /  THE MID-STEP COLLISION",size=18,
             color=FG,weight="bold")
    fig.text(.055,.899,"C++23 trajectories · complete cubic hull vs endpoint-only failure",
             color=MUTED,size=10)
    from matplotlib.lines import Line2D
    fig.legend(handles=[
        Line2D([0],[0],color=CYAN,lw=2.5,label="Cubic motion path"),
        Line2D([0],[0],color=ORANGE,lw=3.5,label="Stationary triangle"),
        Line2D([0],[0],color=CYAN,lw=1,label="Conservative hull"),
        Line2D([0],[0],color=RED,lw=2,ls="--",label="Wrong endpoint hull")
        ],loc="center right",bbox_to_anchor=(.97,.65),frameon=False,
        labelcolor=FG,fontsize=9)
    fig.text(.76,.23,"MATHEMATICAL PATH ENCLOSURE",color=CYAN,
             size=10,weight="bold")
    fig.text(.76,.19,"Not solver dynamics or general CCD",
             color=MUTED,size=8)
    fig.text(.055,.04,"Source: trajectory.csv + envelopes.csv | cubic_midstep_triangle_collision_v1",
             color=MUTED,size=9)
    fig.savefig(out/"01_trajectory_certificate.png",bbox_inches="tight",
                pad_inches=.15)
    plt.close(fig)

def render_timing(out,p,en):
    t=np.arange(121)/120.0
    x=np.array([p[i][11][0][0] for i in range(121)])
    fig,ax=plt.subplots(figsize=(12,6.7),dpi=165)
    fig.subplots_adjust(left=.105,right=.96,top=.83,bottom=.15)
    ax.plot(t,x,color=CYAN,lw=3.6,label="Exported moving triangle X")
    ax.axhline(3,color=ORANGE,lw=2.2,ls="--",label="Fixed target X = 3")
    ax.axhline(0,color=RED,lw=1.4,ls=":",label="Wrong endpoints X = 0")
    ax.axhline(en[(11,"complete_control_hull")][3],color=GREEN,
               lw=1.5,ls="-.",label="Control hull maximum X ≈ 4")
    ax.scatter([.5],[3],color=GOLD,s=110,zorder=10,
               edgecolors=FG,linewidth=1)
    ax.annotate("Exact mid-step coincidence",xy=(.5,3),
        xytext=(.57,2.4),color=FG,
        arrowprops=dict(arrowstyle="->",color=GOLD,lw=1.3))
    ax.fill_between(t,x,3,color=CYAN,alpha=.08)
    ax.set(xlim=(0,1),ylim=(-.25,4.5),
           xlabel="Normalized step time  t ∈ [0,1]",
           ylabel="X position / world units")
    ax.grid(True)
    ax.legend(loc="lower center",ncol=2,frameon=False,
              labelcolor=FG,fontsize=9)
    fig.text(.105,.935,"02  /  WHY ENDPOINT-ONLY BOUNDS FAIL",
             weight="bold",size=18)
    fig.text(.105,.885,"Declared cubic x(t)=12t(1−t); endpoints both at X=0, but actual path reaches X=3.",
             color=MUTED,size=10)
    fig.text(.105,.045,"Analytic special case, not a general CCD proof.",
             color=MUTED,size=9)
    fig.savefig(out/"02_time_position_gap.png",bbox_inches="tight",
                pad_inches=.18)
    plt.close(fig)

def render_spacetime(out,p):
    fig=plt.figure(figsize=(12.8,8),dpi=175)
    ax=fig.add_axes([.06,.10,.88,.76],projection="3d")
    scene_axis(ax,("X / space","Y / space","t / time"))
    ax.set_xlim(-.5,4.3);ax.set_ylim(-.2,1.3);ax.set_zlim(0,1)
    ax.set_box_aspect((4.8,1.7,2.3),zoom=.85)
    for k in range(3):
        pts=np.array([p[i][11][k] for i in range(121)])
        t=np.linspace(0,1,121)
        line=np.column_stack([pts[:,0],pts[:,1],t])
        segments=[line[i:i+2] for i in range(120)]
        coll=Line3DCollection(segments,cmap="viridis",linewidths=4,
                              alpha=.95)
        coll.set_array(t[:-1]);ax.add_collection3d(coll)
        ax.scatter([3],[pts[60,1]],[.5],color=GOLD,s=50,
                   depthshade=False)
    X=np.array([[3,3],[3,3]])
    Y=np.array([[-.1,1.15],[-.1,1.15]])
    T=np.array([[0,0],[1,1]])
    ax.plot_surface(X,Y,T,color=ORANGE,alpha=.16,edgecolor="none",
                    shade=False)
    fig.text(.07,.95,"03  /  SPACETIME GEOMETRY",
             weight="bold",size=18)
    fig.text(.07,.91,"The missing dimension of endpoint-only culling: every vertex follows a continuous path.",
             color=MUTED,size=10)
    fig.text(.07,.048,"C++ source: (x,y,t) embedding; spatial Z is projected out. Not a physical solver.",
             color=MUTED,size=9)
    fig.savefig(out/"03_spacetime_contact_tube.png",bbox_inches="tight",
                pad_inches=.16)
    plt.close(fig)

def render_phase_map(out):
    a=np.linspace(0,6,600)
    A,X=np.meshgrid(a,a)
    # Exact for C0=C3=0, C1=C2=A, t∈[0,1].
    # Curve maximum = 3A/4. Control-point hull maximum = A.
    zone=np.where(X<=.75*A,2,np.where(X<=A,1,0))
    fig,ax=plt.subplots(figsize=(10.8,7.6),dpi=170)
    fig.subplots_adjust(left=.12,right=.94,bottom=.12,top=.86)
    from matplotlib.colors import ListedColormap
    ax.pcolormesh(A,X,zone,shading="auto",
                  cmap=ListedColormap(["#192b42","#765439","#11796e"]),
                  vmin=-.5,vmax=2.5)
    ax.plot(a,a,color=ORANGE,lw=2,ls="--")
    ax.plot(a,.75*a,color=GREEN,lw=2.5)
    ax.scatter([4],[3],c=GOLD,s=160,edgecolors=FG,
               linewidths=1.5,zorder=10)
    ax.annotate("Analytic test (A=4, X=3)",xy=(4,3),xytext=(3.1,4.8),
                color=FG,arrowprops=dict(arrowstyle="->",color=GOLD))
    ax.text(1.35,4.8,"correctly rejected",color=FG,ha="center")
    ax.text(1.5,1.9,"broad-phase\nfalse positive",color=FG,ha="center")
    ax.text(4.8,1.1,"true collision\nduring step",color=FG,ha="center")
    ax.set(xlim=(0,6),ylim=(0,6),
           xlabel="Cubic control displacement A / world units",
           ylabel="Stationary target X / world units")
    ax.grid(alpha=.1)
    ax.legend(handles=[
        Patch(color="#11796e",label="Real contact during interval"),
        Patch(color="#765439",label="Conservative false positive"),
        Patch(color="#192b42",label="No candidate")
    ],loc="upper right",frameon=False,labelcolor=FG,fontsize=9)
    fig.text(.12,.96,"04  /  CONSERVATIVENESS PHASE DIAGRAM",
             weight="bold",size=17)
    fig.text(.12,.915,"Analytic parameter-space classification, not benchmark timings.",
             color=MUTED,size=10)
    fig.text(.12,.045,"x(t)=3A t(1−t). Trajectory range [0,0.75A]; control hull [0,A]. Thickness omitted for diagram clarity.",
             color=MUTED,size=9)
    fig.savefig(out/"04_broadphase_regime_map.png",
                bbox_inches="tight",pad_inches=.14)
    plt.close(fig)

def render_interactive(out,p,en):
    import plotly.graph_objects as go
    def mesh(points,name,color,alpha):
        x,y,z=as_triangle(points).T
        return go.Mesh3d(x=x,y=y,z=z,i=[0],j=[1],k=[2],
                         color=color,opacity=alpha,flatshading=True,
                         name=name)
    def pair(frame):
        return [mesh(p[frame][11],"Moving cubic triangle",CYAN,.80),
                mesh(p[frame][19],"Stationary triangle",ORANGE,.65)]
    frames=[go.Frame(name=str(i),data=pair(i),traces=[0,1])
            for i in range(121)]
    fig=go.Figure(data=pair(0),frames=frames)
    for v in range(3):
        pts=np.array([p[i][11][v] for i in range(121)])
        fig.add_trace(go.Scatter3d(x=pts[:,0],y=pts[:,1],z=pts[:,2],
                                  mode="lines",line=dict(color=CYAN,width=4),
                                  opacity=.5,showlegend=v==0,
                                  name="Cubic control-hull trajectory"))
    for mode,col in (("complete_control_hull",CYAN),
                     ("endpoint_only",RED)):
        b=en[(11,mode)]
        for j,line in enumerate(edges(b[:3],b[3:])):
            fig.add_trace(go.Scatter3d(
                x=[line[0][0],line[1][0]],y=[line[0][1],line[1][1]],
                z=[line[0][2],line[1][2]],mode="lines",opacity=.55,
                line=dict(color=col,width=2 if mode=="complete_control_hull" else 4),
                showlegend=j==0,name="Complete control hull" if mode=="complete_control_hull"
                else "Wrong endpoint-only box"))
    fig.update_layout(
        title=dict(text="SPACETIME CONTACT ATLAS | Cubic midpoint collision"
                   "<br><sup>121 actual C++23 kinematic frames — NOT a solver run</sup>",
                   font=dict(size=21,color=FG)),
        font=dict(color=FG),paper_bgcolor=BG,plot_bgcolor=BG,height=790,
        margin=dict(t=100,b=50,l=30,r=30),
        scene=dict(bgcolor=BG,aspectmode="manual",
           aspectratio=dict(x=2.2,y=1,z=1),
           xaxis=dict(range=[-.4,4.5],title="X / world units",
                      backgroundcolor=BG,gridcolor=GRID),
           yaxis=dict(range=[-.3,1.4],title="Y / world units",
                      backgroundcolor=BG,gridcolor=GRID),
           zaxis=dict(range=[-.3,1.4],title="Z / world units",
                      backgroundcolor=BG,gridcolor=GRID),
           camera=dict(eye=dict(x=1.5,y=-1.9,z=.9))),
        updatemenus=[dict(type="buttons",x=.06,y=-.07,buttons=[
          dict(label="▶ Play",method="animate",
               args=[None,dict(frame=dict(duration=50,redraw=True),
                               transition=dict(duration=0),
                               fromcurrent=True,mode="immediate")]),
          dict(label="❚❚ Pause",method="animate",
               args=[[None],dict(frame=dict(duration=0,redraw=False),
                                 mode="immediate")])])],
        sliders=[dict(x=.32,y=-.04,len=.62,currentvalue=dict(prefix="Frame / 120: "),
                steps=[dict(method="animate",label=str(i),
                            args=[[str(i)],dict(frame=dict(duration=0,redraw=True),
                                                transition=dict(duration=0),
                                                mode="immediate")])
                       for i in range(121)])])
    fig.write_html(out/"05_interactive_motion_lab.html",
                   include_plotlyjs=True,full_html=True,auto_play=False)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--data",required=True,type=Path)
    parser.add_argument("--output",required=True,type=Path)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    info=json.loads((args.data/"manifest.json").read_text())
    assert info["frames"]==121 and info["independent_sap_parity"] is True
    p=defaultdict(lambda:defaultdict(dict))
    for r in rows(args.data/"trajectory.csv"):
        p[int(r["frame"])][int(r["object"])][int(r["vertex"])]=np.array(
            [float(r[k]) for k in ("x","y","z")])
    en={}
    for r in rows(args.data/"envelopes.csv"):
        en[(int(r["object"]),r["mode"])]=np.array([
            float(r[k]) for k in
            ("xmin","ymin","zmin","xmax","ymax","zmax")])
    assert len(p)==121
    assert np.linalg.norm(p[60][11][0]-p[60][19][0]) < 1e-12
    theme()
    render_trajectory(args.output,p,en)
    render_timing(args.output,p,en)
    render_spacetime(args.output,p)
    render_phase_map(args.output)
    render_interactive(args.output,p,en)
    print("PASS: 4 C++ source-backed figures and offline interactive 3D")

if __name__=="__main__":main()
