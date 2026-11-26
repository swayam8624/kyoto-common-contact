#!/usr/bin/env python3
"""Visualize recorded nonconvex FEM motion, never interpolated test results.

The rotated impact and rotated stationary control have identical camera,
spatial extent and time labels. All geometry comes from the actual accepted
step endpoints. A separate volume-change map uses measured tetrahedra.
"""
import argparse
from collections import Counter
from itertools import combinations
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.colors import Normalize
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def read(path):
    scenes=[]
    with path.open() as f:
        for sid in range(4):
            k,n,t,steps,nu=map(int,f.readline().split());assert k==sid and steps==32 and 0<nu<n
            cells=np.asarray([list(map(int,f.readline().split())) for _ in range(t)])
            p=np.asarray([[list(map(float,f.readline().split())) for _ in range(n)] for _ in range(steps+1)])
            assert cells.shape==(t,4) and p.shape==(steps+1,n,3) and np.isfinite(p).all()
            assert (cells>=0).all() and (cells<n).all()
            count=Counter(tuple(sorted(face)) for cell in cells for face in combinations(cell,3))
            assert set(count.values()) <= {1,2}
            faces=np.asarray([f for f,c in count.items() if c==1]);owners=faces[:,0]>=nu
            assert np.all((faces>=nu)==owners[:,None])
            scenes.append((p,cells,faces,owners,nu))
        assert not f.read().strip()
    return scenes


def run(path,out):
    out.mkdir(parents=True,exist_ok=False)
    scenes=read(path);selected=[scenes[1],scenes[3]]
    p=np.concatenate([s[0].reshape(-1,3) for s in selected]);lo=p.min(axis=0);hi=p.max(axis=0);mid=(lo+hi)/2;radius=.55*max(hi-lo)
    fig,axes=plt.subplots(1,2,figsize=(11,5),subplot_kw={'projection':'3d'})
    collections=[]
    for ax,(pos,cells,faces,owner,nu),title in zip(axes,selected,('Rotated impact','Stationary control')):
        c=Poly3DCollection(pos[0][faces],facecolors=np.where(owner[:,None],np.array([.85,.48,.17,.85]),np.array([.16,.46,.72,.45])),edgecolors=(.15,.18,.22,.55),linewidths=.35)
        ax.add_collection3d(c);collections.append(c)
        ax.set(xlim=(mid[0]-radius,mid[0]+radius),ylim=(mid[1]-radius,mid[1]+radius),zlim=(mid[2]-radius,mid[2]+radius),xlabel='x (m)',ylabel='y (m)',zlabel='z (m)',title=title);ax.set_box_aspect((1,1,1));ax.view_init(elev=27,azim=-58)
    label=fig.suptitle('Recorded independent FEM states • 0 ms')
    def update(i):
        for c,(pos,cells,faces,owner,nu) in zip(collections,selected):c.set_verts(pos[i][faces])
        label.set_text(f'Recorded independent FEM states • {i} ms');return [*collections,label]
    for i in (0,16,32):update(i);fig.savefig(out/f'impact_control_{i:02d}ms.png',dpi=150,bbox_inches='tight')
    animation=FuncAnimation(fig,update,frames=33,interval=120,blit=False);animation.save(out/'impact_vs_control.gif',writer=PillowWriter(fps=8),dpi=100);plt.close(fig)
    # Each tetrahedron's current-to-initial volume ratio is calculated
    # directly from its four vertices. Compare both scenes on one scale.
    ratios=[]
    for pos,cells,*_ in selected:
        def det(x):return np.linalg.det((x[cells[:,1:]]-x[cells[:,:1]]).transpose(0,2,1))
        ratios.append(det(pos[-1])/det(pos[0]))
    low=min(float(r.min()) for r in ratios);high=max(float(r.max()) for r in ratios)
    if low==high:low-=1e-12;high+=1e-12
    norm=Normalize(low,high);fig,axes=plt.subplots(1,2,figsize=(11,5),subplot_kw={'projection':'3d'})
    for ax,(pos,cells,*_),r,title in zip(axes,selected,ratios,('Impact at 32 ms','Control at 32 ms')):
        # Scatter centers rather than painting an arbitrary surface value.
        centers=pos[-1][cells].mean(axis=1);m=ax.scatter(*centers.T,c=r,cmap='viridis',norm=norm,s=50,depthshade=False)
        ax.set(xlim=(mid[0]-radius,mid[0]+radius),ylim=(mid[1]-radius,mid[1]+radius),zlim=(mid[2]-radius,mid[2]+radius),xlabel='x (m)',ylabel='y (m)',zlabel='z (m)',title=title);ax.set_box_aspect((1,1,1));ax.view_init(elev=27,azim=-58)
    fig.colorbar(m,ax=axes.tolist(),shrink=.65,label='Current / initial tetrahedron volume');fig.savefig(out/'element_volume_ratio.png',dpi=160,bbox_inches='tight');plt.close(fig)
    # Actual original tetrahedral connectivity, including interior edges.
    pos,cells,faces,owner,nu=selected[0];fig=plt.figure(figsize=(7,6));ax=fig.add_subplot(projection='3d');edges={tuple(sorted(e)) for t in cells for e in combinations(t,2)}
    for a,b in sorted(edges):
        q=pos[0,[a,b]];ax.plot(*q.T,color='#236aa1' if a<nu else '#c76d29',linewidth=.7,alpha=.65)
    ax.scatter(*pos[0].T,s=11,color='#222');ax.set(xlabel='x (m)',ylabel='y (m)',zlabel='z (m)',title=f'Actual topology: {pos.shape[1]} nodes, {len(cells)} tetrahedra');ax.set_box_aspect((1,1,1));ax.view_init(elev=27,azim=-58);fig.savefig(out/'tetrahedral_topology.png',dpi=160,bbox_inches='tight');plt.close(fig)
    print('Rendered 33 actual accepted frames, paired controls, volume map and connectivity')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('trajectory',type=Path);p.add_argument('output',type=Path);a=p.parse_args();run(a.trajectory,a.output)
