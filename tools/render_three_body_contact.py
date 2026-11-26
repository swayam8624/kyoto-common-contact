#!/usr/bin/env python3
"""Visualize actual independent implicit solver states and contact impulses."""
import argparse
from collections import Counter
from itertools import combinations
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, FFMpegWriter
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

def run(source,out):
    out.mkdir(parents=True,exist_ok=False)
    scenes=[]
    with (source/'trajectory.txt').open() as f:
        for k in range(4):
            sid,n,t,steps=map(int,f.readline().split());assert sid==k and n%3==0
            cells=np.array([list(map(int,f.readline().split())) for _ in range(t)])
            pos=np.array([[list(map(float,f.readline().split())) for _ in range(n)] for _ in range(steps+1)])
            faces=Counter(tuple(sorted(face)) for cell in cells for face in combinations(cell,3))
            boundary=np.array([face for face,count in faces.items() if count==1]);owners=boundary[:,0]//(n//3)
            assert np.all(boundary//(n//3)==owners[:,None]);scenes.append((pos,boundary,owners))
        assert not f.read().strip()
    receipts=pd.read_csv(source/'ipopt_receipts.csv');assert len(receipts)==128
    labels=['Aligned 3-body','Rotated 3-body','Larger rotated 3-body','Separated control']
    fig=plt.figure(figsize=(10,8),layout='constrained');axes=[fig.add_subplot(2,2,i+1,projection='3d') for i in range(4)];polys=[];titles=[]
    colors=np.array(['#3779b0','#e28e39','#479a76'])
    for ax,(pos,faces,owners),label in zip(axes,scenes,labels):
        # Fixed spatial extent per scene, identical camera and no exaggerated deformation.
        xyz=pos.reshape(-1,3);lo=xyz.min(axis=0);hi=xyz.max(axis=0);mid=(hi+lo)/2;radius=max(hi-lo)*.58
        ax.set(xlim=(mid[0]-radius,mid[0]+radius),ylim=(mid[1]-radius,mid[1]+radius),zlim=(mid[2]-radius,mid[2]+radius),xlabel='x (m)',ylabel='y (m)',zlabel='z (m)');ax.set_box_aspect((1,1,1));ax.view_init(elev=27,azim=-60)
        poly=Poly3DCollection(pos[0][faces],facecolors=colors[owners],edgecolor='#30343b',linewidth=.45,alpha=.86);ax.add_collection3d(poly);polys.append(poly);titles.append(ax.set_title(label))
    fig.suptitle('Independent implicit FEM · actual recorded 3-body states')
    def update(step):
        for i,(pos,faces,owners) in enumerate(scenes):
            polys[i].set_verts(pos[step][faces]);row=receipts[(receipts.scene==i)&(receipts.step==step-1)]
            active=int(row.active_pairs.iloc[0]) if step else 0
            titles[i].set_text(f'{labels[i]} | {step} ms | active pairs: {active}')
        return polys+titles
    update(24);fig.savefig(out/'three_body_frame.png',dpi=150)
    anim=FuncAnimation(fig,update,frames=range(33),blit=False);anim.save(out/'three_body_contact.mp4',writer=FFMpegWriter(fps=8,bitrate=1800),dpi=100);plt.close(fig)
    fig,axes=plt.subplots(2,2,figsize=(10,6),layout='constrained')
    for sid,ax in enumerate(axes.ravel()):
        r=receipts[receipts.scene==sid];time=r.step+1
        for k in ['01','02','12']:ax.plot(time,r['impulse'+k].cumsum(),label='Pair '+k)
        ax.set(title=labels[sid],xlabel='Time (ms)',ylabel='Cumulative impulse (N s)');ax.grid(alpha=.2);ax.legend()
    fig.suptitle('Independent contact solver — measured pair impulses');fig.savefig(out/'pair_impulses.png',dpi=160);plt.close(fig)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();run(a.source,a.output)
