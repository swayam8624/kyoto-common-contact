#!/usr/bin/env python3
"""Native C++ Stage-2 data -> inspectable figures and interactive 3D.
No guessed timing or external solver data. Every empirical dot is a CSV row.
"""
import argparse, json
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
from matplotlib.collections import LineCollection
from matplotlib.colors import ListedColormap
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

BG="#07121f";PANEL="#0d2030";FG="#e7f1fa";MUTED="#9ab1c6"
GRID="#294256";TEAL="#48dfc1";CYAN="#51caff"
AMBER="#f8ba75";RED="#e97179"

def style():
    plt.rcParams.update({"figure.facecolor":BG,"savefig.facecolor":BG,
       "axes.facecolor":PANEL,"axes.edgecolor":GRID,"axes.labelcolor":MUTED,
       "text.color":FG,"xtick.color":MUTED,"ytick.color":MUTED,
       "grid.color":GRID,"grid.alpha":.45,"font.family":"DejaVu Sans",
       "axes.titlesize":14,"axes.titleweight":"bold"})
def title(fig,heading,subtitle,footer):
    fig.text(.055,.965,heading,size=17,weight="bold",va="top")
    fig.text(.055,.925,subtitle,size=9,color=MUTED,va="top")
    fig.text(.055,.03,footer,size=8,color=MUTED)
def save(fig,out,name):
    fig.savefig(out/name,dpi=185,bbox_inches="tight",pad_inches=.16)
    plt.close(fig)
def load(inp):
    a=pd.read_csv(inp/"temporal_analytical_study.csv")
    b=pd.read_csv(inp/"candidate_graph.csv")
    c=pd.read_csv(inp/"slab_envelopes.csv")
    m=json.loads((inp/"manifest.json").read_text())
    assert len(a)==120 and len(b)==160 and len(c)==31
    assert set(a.method)=={"global_slab","selective_pair"}
    assert (a.missed_true==0).all()
    assert not a.isna().any().any()
    assert m["independent_exact_candidate_oracle"]
    assert not m["independent_general_CCD_oracle"]
    for _,g in a.groupby(["count","amplitude","depth"]):
        assert len(g)==2 and g.candidate_unique.nunique()==1
        assert g.false_positive.nunique()==1
    return a,b,c

def precision(data,out):
    d=data[(data["count"]==2048)&(data.amplitude==4)&(data.method=="global_slab")]
    x=d.depth.to_numpy();t=d.true_events.to_numpy();f=d.false_positive.to_numpy()
    fig,ax=plt.subplots(figsize=(11,6.7))
    fig.subplots_adjust(top=.82,bottom=.16,left=.11,right=.94)
    ax.bar(x,t,width=.64,color=TEAL,label="Analytically confirmed contacts")
    ax.bar(x,f,bottom=t,width=.64,color=RED,label="False-positive candidates")
    for i,(a,b) in enumerate(zip(t,f)):
        ax.text(i,a+b+25,f"{a+b:,}",ha="center",color=FG)
    ax.set_ylim(0,1950);ax.set_xticks(x,["1","2","4","8","16"])
    ax.set_xlabel("Synchronized time slabs")
    ax.set_ylabel("Unique candidate pairs")
    ax.grid(True,alpha=.27);ax.set_axisbelow(True)
    ax.legend(frameon=False,labelcolor=FG)
    title(fig,"01 / CANDIDATE PRECISION",
        "2,048-target 3D analytic geometry; all actual constructed contacts preserved.",
        "C++ source CSV; these are NOT CCD event timings.")
    save(fig,out,"01_candidate_precision.png")

def latency(data,out):
    d=data[(data["count"]==2048)&(data.amplitude==4)]
    fig,ax=plt.subplots(figsize=(11,6.7))
    fig.subplots_adjust(top=.82,bottom=.16,left=.12,right=.94)
    for method,color,label in [
        ("global_slab",AMBER,"Global time-slab rebuild"),
        ("selective_pair",CYAN,"Selective candidate-pair refinement")]:
        sub=d[d.method==method].sort_values("depth")
        x=2**sub.depth.to_numpy();y=sub.mean_paid_us.to_numpy()/1000.
        ax.plot(x,y,color=color,lw=2.8,marker="o",ms=8,label=label)
        for xx,yy in zip(x,y):
            ax.annotate(f"{yy:.1f} ms" if yy>1 else f"{yy*1000:.0f} μs",
              (xx,yy),xytext=(0,9),textcoords="offset points",
              color=color,fontsize=8,ha="center")
    ax.set_xscale("log",base=2);ax.set_yscale("log")
    ax.set_xticks([1,2,4,8,16],["1","2","4","8","16"])
    ax.set_xlabel("Synchronized time slabs")
    ax.set_ylabel("Paid broad-phase time (milliseconds, log scale)")
    ax.grid(True,alpha=.3);ax.legend(frameon=False,labelcolor=FG)
    title(fig,"02 / THE COST OF PRECISION",
        "Measured full query including certificates, filtering, sorting and normalization",
        "11 repeats on one native CPU session; NOT official IPC LBVH or a whole-solver result.")
    save(fig,out,"02_total_paid_latency.png")

def surface(data,out):
    from mpl_toolkits.mplot3d import Axes3D
    d=data[data.amplitude==4]
    xs=np.array([32,128,512,2048])
    ys=np.array([1,2,4,8,16])
    X,Y=np.meshgrid(np.log2(xs),np.log2(ys))
    fig=plt.figure(figsize=(12,8))
    fig.subplots_adjust(top=.82,left=.03,right=.93,bottom=.09)
    ax=fig.add_subplot(111,projection="3d")
    for method,col in [("global_slab",AMBER),("selective_pair",CYAN)]:
        m=d[d.method==method]
        vals=np.array([[m[(m["count"]==int(n))&
                          (m.depth==int(np.log2(k)))].mean_paid_us.iloc[0]/1000.
                        for n in xs] for k in ys])
        z=np.log10(vals)
        ax.plot_wireframe(X,Y,z,color=col,rstride=1,cstride=1,lw=1.5)
        ax.scatter(X,Y,z,color=col,s=35,depthshade=False)
    for a in [ax.xaxis,ax.yaxis,ax.zaxis]:
        a.pane.set_facecolor((.03,.07,.12,1))
        a.pane.set_edgecolor(GRID)
    ax.set_xticks(np.log2(xs),[str(int(x)) for x in xs])
    ax.set_yticks(np.log2(ys),[str(int(y)) for y in ys])
    ax.set_xlabel("Triangle targets",labelpad=11)
    ax.set_ylabel("Time slabs",labelpad=11)
    ax.set_zlabel("log10 time (ms)",labelpad=10)
    ax.set_box_aspect((1.2,1.1,.7));ax.view_init(elev=24,azim=-62)
    ax.legend(handles=[Line2D([0],[0],color=AMBER,lw=2,label="Global"),
                       Line2D([0],[0],color=CYAN,lw=2,label="Selective")],
              loc="upper right",frameon=False,labelcolor=FG)
    title(fig,"03 / MEASURED 3D COST SURFACE",
          "Every vertex is an actual native measurement; no interpolated metric claims.",
          "X/Y axes are log2-indexed, Z is log10(milliseconds).")
    save(fig,out,"03_cost_surface_3d.png")

def crossover(data,out):
    vals=[]
    for depth in range(5):
        row=[]
        for n in [32,128,512,2048]:
            g=data[(data["count"]==n)&(data.amplitude==4)&(data.depth==depth)]
            m=g.set_index("method").mean_paid_us
            row.append(m["global_slab"]/m["selective_pair"])
        vals.append(row)
    ratios=np.array(vals)
    fig,ax=plt.subplots(figsize=(11,6.8))
    fig.subplots_adjust(top=.81,left=.11,right=.91,bottom=.17)
    pic=ax.imshow(np.log2(ratios),origin="lower",cmap="coolwarm",vmin=-1.5,vmax=1.5,aspect="auto")
    for i in range(5):
        for j in range(4):
            ax.text(j,i,f"{ratios[i,j]:.2f}×",ha="center",va="center",color=FG,weight="bold")
    ax.set_xticks(range(4),["32","128","512","2,048"])
    ax.set_yticks(range(5),["1","2","4","8","16"])
    ax.set_xlabel("Target triangle count");ax.set_ylabel("Time slabs")
    cb=fig.colorbar(pic,ax=ax,pad=.025)
    cb.set_label("log2(Global / Selective) paid time",color=MUTED)
    title(fig,"04 / SELECTIVE METHOD CROSSOVER",
          "Values above 1×: selective beats global at equal refinement depth.",
          "Depth zero remains much faster than both deeply refined options.")
    save(fig,out,"04_selective_regime_map.png")

def graph_view(graph,out):
    d=graph[graph.depth==1].sort_values("target_x")
    fig=plt.figure(figsize=(12,7.5))
    fig.subplots_adjust(top=.82,left=.01,right=.94,bottom=.08)
    ax=fig.add_subplot(111,projection="3d")
    for _,r in d.iterrows():
        x=float(r.target_x);y=.45*np.cos(1.4*x);z=.42*np.sin(.85*x)
        true=bool(r.analytical_contact)
        coarse=bool(r.coarse_candidate)
        refined=bool(r.selective_candidate)
        c=TEAL if true else RED if coarse and not refined else MUTED
        alpha=.85 if true else .6 if coarse else .2
        if coarse:ax.plot([0,x],[0,y],[0,z],color=c,alpha=alpha,
                          lw=1.4,ls="-" if true else "--")
        ax.scatter([x],[y],[z],s=48 if true else 30,color=c,alpha=alpha,depthshade=False)
    ax.scatter([0],[0],[0],s=300,color=CYAN,edgecolors=FG,linewidth=1.3)
    ax.text(0,.2,.1,"MOVING FEATURE",color=CYAN,fontsize=9)
    ax.set(xlim=(-.25,5),ylim=(-.9,.9),zlim=(-.65,.8))
    ax.set_box_aspect((3.2,1.2,1));ax.view_init(elev=22,azim=-67)
    ax.set_xlabel("Target X / world units",labelpad=10)
    ax.set_ylabel("Graph-layout Y",labelpad=10)
    ax.set_zlabel("Graph-layout Z",labelpad=10)
    ax.legend(handles=[
        Line2D([0],[0],color=TEAL,lw=2,label="Analytic contact"),
        Line2D([0],[0],color=RED,lw=2,ls="--",label="Coarse false positive"),
        Line2D([0],[0],color=MUTED,lw=0,marker="o",label="Rejected")],
        loc="upper right",frameon=False,labelcolor=FG,fontsize=9)
    title(fig,"05 / 3D CONTACT CANDIDATE NETWORK",
          "32 targets and native C++ candidate edges; directed graph layout is a visual abstraction.",
          "Only node X is a physical coordinate; layout Y/Z are NOT simulated geometry.")
    save(fig,out,"05_contact_topology_3d.png")

def tubes(boxes,out):
    fig=plt.figure(figsize=(12,8))
    fig.subplots_adjust(top=.82,left=.02,right=.94,bottom=.08)
    ax=fig.add_subplot(111,projection="3d")
    palette=plt.get_cmap("turbo",5)
    for depth in range(5):
        for _,r in boxes[boxes.depth==depth].iterrows():
            x0,x1=float(r.t0),float(r.t1)
            lo,hi=float(r.xmin),float(r.xmax)
            poly=[(x0,lo,depth),(x1,lo,depth),
                  (x1,hi,depth),(x0,hi,depth)]
            ax.add_collection3d(Poly3DCollection([poly],
                facecolor=palette(depth),edgecolor=FG,linewidth=.5,alpha=.35))
    t=np.linspace(0,1,300)
    ax.plot(t,12*t*(1-t),4.2*np.ones_like(t),color=FG,lw=3,
            label="Analytic trajectory")
    ax.plot([0,1],[3.5,3.5],[4.3,4.3],color=RED,ls="--",lw=2,
            label="Noncontact target X=3.5")
    ax.set(xlim=(0,1),ylim=(-.15,4.5),zlim=(-.2,4.5))
    ax.set_box_aspect((1.5,1.3,.8));ax.view_init(elev=27,azim=-60)
    ax.set_xlabel("Normalized time t",labelpad=10)
    ax.set_ylabel("X motion envelope",labelpad=10)
    ax.set_zlabel("Subdivision depth",labelpad=8)
    ax.set_zticks(range(5),["1","2","4","8","16"])
    ax.legend(frameon=False,labelcolor=FG,loc="upper right")
    title(fig,"06 / CERTIFIED SPACETIME BOXES",
          "C++ outward-rounded de Casteljau interval bounds, synchronized over the same t.",
          "Boxes are candidates, not continuous-collision event certificates.")
    save(fig,out,"06_spacetime_envelope_layers_3d.png")

def interactive(data,boxes,out):
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    fig=make_subplots(rows=1,cols=2,
        specs=[[{"type":"scene"},{"type":"xy"}]],
        column_widths=[.56,.44],
        subplot_titles=["3D time-slab certificates","Native paid broad-phase timings"])
    cols=["#51caff","#48dfc1","#f8ba75","#c59afb","#ed7684"]
    for depth in range(5):
        for _,r in boxes[boxes.depth==depth].iterrows():
            t0,t1=float(r.t0),float(r.t1)
            lo,hi=float(r.xmin),float(r.xmax)
            fig.add_trace(go.Scatter3d(x=[t0,t1,t1,t0,t0],
                y=[lo,lo,hi,hi,lo],z=[depth]*5,
                mode="lines",line=dict(color=cols[depth],width=4),
                showlegend=bool(r.slab==0),name=f"{2**depth} slab(s)"),row=1,col=1)
    t=np.linspace(0,1,250)
    fig.add_trace(go.Scatter3d(x=t,y=12*t*(1-t),
        z=np.repeat(4.3,len(t)),mode="lines",
        line=dict(color="white",width=6),name="Analytic cubic"),row=1,col=1)
    sub=data[(data.amplitude==4)&(data["count"]==2048)]
    for method,color in [("global_slab",AMBER),("selective_pair",CYAN)]:
        m=sub[sub.method==method].sort_values("depth")
        fig.add_trace(go.Scatter(x=2**m.depth,y=m.mean_paid_us/1000,
            mode="lines+markers",line=dict(color=color,width=3),
            marker=dict(size=10),name=method),row=1,col=2)
    fig.update_scenes(bgcolor=BG,xaxis_title="time",
        yaxis_title="X range",zaxis_title="subdivision depth",
        camera=dict(eye=dict(x=1.8,y=-1.65,z=1.2)))
    fig.update_xaxes(type="log",dtick=.30103,title="Number of time slabs")
    fig.update_yaxes(type="log",title="Paid time (ms)")
    fig.update_layout(height=760,paper_bgcolor=BG,plot_bgcolor=BG,
        font=dict(color=FG),margin=dict(t=105,l=35,r=30,b=55),
        title="SPACETIME CONTACT ATLAS | Stage 2"
              "<br><sup>Synthetic analytical triangle data — NO solver/IPC-LBVH result</sup>")
    fig.write_html(out/"07_interactive_spacetime_observatory.html",
                   full_html=True,include_plotlyjs=True)

def overview(out):
    from PIL import Image,ImageDraw,ImageFont
    names=["01_candidate_precision.png","02_total_paid_latency.png",
       "03_cost_surface_3d.png","04_selective_regime_map.png",
       "05_contact_topology_3d.png","06_spacetime_envelope_layers_3d.png"]
    canvas=Image.new("RGB",(1500,1730),BG)
    for k,name in enumerate(names):
        im=Image.open(out/name).convert("RGB")
        im.thumbnail((740,530))
        canvas.paste(im,((k%2)*750+(750-im.width)//2,
                         130+(k//2)*530+(530-im.height)//2))
    d=ImageDraw.Draw(canvas)
    try:f=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",34)
    except OSError:f=ImageFont.load_default()
    d.text((38,30),"SPACETIME CONTACT ATLAS  /  PASS 02",fill=FG,font=f)
    d.text((38,83),"Source-backed analytic contact and CPU timing: six scientific figures",fill=MUTED)
    canvas.save(out/"STAGE2_SCIENTIFIC_ATLAS.png")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    data,graph,boxes=load(a.input)
    style()
    precision(data,a.output)
    latency(data,a.output)
    surface(data,a.output)
    crossover(data,a.output)
    graph_view(graph,a.output)
    tubes(boxes,a.output)
    interactive(data,boxes,a.output)
    overview(a.output)
    print("PASS: 6 scientific charts + 3D interactive observatory + atlas")

if __name__=="__main__":main()
