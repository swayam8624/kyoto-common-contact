#!/usr/bin/env python3
"""Render measured source states; no geometry is procedurally animated."""
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, FFMpegWriter, PillowWriter
from matplotlib.colors import Normalize
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def render(source, out):
    out.mkdir(parents=True, exist_ok=True)
    st=pd.read_csv(source/'steps.csv'); nodes=pd.read_csv(source/'states.csv'); topo=pd.read_csv(source/'topology.csv')
    tm=pd.read_csv(source/'cpu_candidates.csv')
    plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})
    # Each physical resolution is a single trajectory, not 120 independent samples.
    fig, ax=plt.subplots(figsize=(8,4.5),layout='constrained')
    for size,g in st[st.regime=='impact'].groupby('size'):
        ax.plot(g.time*1000,g.min_j,label=f'{size*6} tetrahedra')
    ax.set(xlabel='Simulation time (ms)',ylabel='Minimum endpoint determinant J',title='Native FEM: orientation during floor contact'); ax.legend(); ax.grid(alpha=.2)
    fig.savefig(out/'orientation.png',dpi=170);plt.close(fig)
    fig, ax=plt.subplots(figsize=(8,4.5),layout='constrained')
    for size,g in st[st.regime=='impact'].groupby('size'):
        ax.plot(g.time*1000,g.impulse.cumsum(),label=f'{size*6} tetrahedra')
    ax.set(xlabel='Simulation time (ms)',ylabel='Cumulative normal impulse (N·s)',title='Measured floor-contact impulse');ax.legend();ax.grid(alpha=.2)
    fig.savefig(out/'impulse.png',dpi=170);plt.close(fig)
    paired=tm.groupby(['size','regime','step','block','arm']).ns.sum().unstack(); paired['ratio']=paired.exhaustive/paired.sap
    fig,ax=plt.subplots(figsize=(8,4.5),layout='constrained')
    for regime,g in paired.reset_index().groupby('regime'):
        q=g.groupby('size').ratio.agg(lambda x:np.exp(np.log(x).mean()))
        ax.plot(q.index*6,q.values,'o-',label=regime)
    ax.axhline(1,linestyle='--',color='gray');ax.set(xlabel='Tetrahedra',ylabel='Exhaustive / SAP time (×)',title='Paid surface broad-phase replay (one compiler)');ax.legend();ax.grid(alpha=.2)
    fig.savefig(out/'candidate_speed.png',dpi=170);plt.close(fig)
    # Fixed camera, scale and stress range for all frames and both panels.
    size=20; faces=topo[(topo['size']==size)&(topo.kind=='face')][['i','j','k']].to_numpy()
    sub=nodes[nodes['size']==size]; frames={(regime,int(step)):g.sort_values('node') for (regime,step),g in sub.groupby(['regime','step'])}
    vmax=max(1.,float(sub.stress.max())); norm=Normalize(0,vmax); cmap=plt.colormaps['viridis']
    fig=plt.figure(figsize=(12,5.8)); axes=[fig.add_axes([.055+k*.43,.09,.39,.80],projection='3d') for k in range(2)]
    polys=[];titles=[]
    for ax,regime in zip(axes,['free','impact']):
        ax.set(xlim=(-1,1),ylim=(-1,1),zlim=(0,.85),xlabel='x (m)',ylabel='y (m)',zlabel='z (m)');ax.set_xticks([-1,-.5,0,.5,1]);ax.set_yticks([-1,-.5,0,.5,1]);ax.set_zticks([0,.2,.4,.6,.8]);ax.set_box_aspect((2,2,.85));ax.view_init(elev=24,azim=-57)
        poly=Poly3DCollection([],edgecolor=(.14,.16,.18,.4),linewidth=.35);ax.add_collection3d(poly);polys.append(poly)
        titles.append(ax.set_title(regime))
        if regime=='impact': ax.plot_surface(*np.meshgrid([-1.,1.],[-1.,1.]),np.zeros((2,2)),alpha=.09,color='gray')
    fig.colorbar(plt.cm.ScalarMappable(norm=norm,cmap=cmap),cax=fig.add_axes([.91,.20,.018,.59]),label='Nodal von Mises stress (Pa)')
    fig.suptitle('Native FEM toroidal solid · recorded states · 120 tetrahedra',fontsize=14)
    def update(step):
        for poly,title,regime in zip(polys,titles,['free','impact']):
            g=frames[(regime,step)];x=g[['x','y','z']].to_numpy();sigma=g.stress.to_numpy()[faces].mean(axis=1)
            poly.set_verts(x[faces]);poly.set_facecolor(cmap(norm(sigma)));title.set_text(f'{regime.title()} · {step} ms · {int((g.impulse>0).sum())} nodal impacts')
        return polys+titles
    update(60);fig.savefig(out/'trajectory_frame_60ms.png',dpi=160)
    ani=FuncAnimation(fig,update,frames=range(0,121,3),blit=False)
    ani.save(out/'native_fem_trajectories.mp4',writer=FFMpegWriter(fps=12,bitrate=2200),dpi=110)
    ani.save(out/'native_fem_trajectories.gif',writer=PillowWriter(fps=10),dpi=80)
    plt.close(fig)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();render(a.source,a.output)
