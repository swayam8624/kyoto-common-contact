#!/usr/bin/env python3
"""Render accepted and rejected source leaves exported by the pinned IPC audit.
Rejected proposals are never depicted as committed physical trajectories.
"""
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

KINDS = {'accepted_connected': ('connected', True), 'rejected_self_positive': ('connected', False),
         'accepted_cross': ('cross_tetra', True), 'rejected_cross_positive': ('cross_tetra', False)}


def render(source, output):
    audit = pd.read_csv(source/'connected_native_ipc_event_audit.csv')
    motion = pd.read_csv(source/'native_leaf_surface_endpoints.csv', float_precision='round_trip')
    topology = pd.read_csv(source/'native_surface_connectivity.csv')
    if set(audit.type) != set(KINDS) or set(motion.type) != set(KINDS):
        raise ValueError('all four accepted/rejected physical source types required')
    if audit.duplicated(['type','leaf']).any() or motion.duplicated(['type','leaf','row']).any():
        raise ValueError('duplicate source identity')
    output.mkdir(parents=True, exist_ok=False)
    for kind, (scene, accepted) in KINDS.items():
        af = audit[audit.type==kind].sort_values('leaf')
        mf = motion[motion.type==kind]
        faces = topology[(topology.scene==scene)&(topology.feature=='face')].sort_values('row')[['v0','v1','v2']].to_numpy(dtype=int)
        if set(af.leaf)!=set(mf.leaf) or len(faces)==0 or (af.dt_s<=0).any() or (af.candidate_misses!=0).any():
            raise ValueError('source or audit coverage incomplete')
        if accepted and ((af.self_hits!=0)|(af.cross_hits!=0)).any():
            raise ValueError('accepted source contains upstream event')
        if not accepted and (af['self_hits' if scene=='connected' else 'cross_hits']<=0).any():
            raise ValueError('rejected positive source lacks event')
        frames=[]; face_owners=None; all_points=[]
        for _, receipt in af.iterrows():
            g=mf[mf.leaf==receipt.leaf].sort_values('row')
            if not np.array_equal(g.row.to_numpy(),np.arange(len(g))) or faces.min()<0 or faces.max()>=len(g):
                raise ValueError('invalid compact source row')
            owners=g.body.to_numpy(dtype=int)[faces]
            if not np.all(owners==owners[:,:1]):
                raise ValueError('face crosses body namespaces')
            owners=owners[:,0]
            if face_owners is None: face_owners=owners
            elif not np.array_equal(owners,face_owners): raise ValueError('body topology changed')
            a=g[['x0','y0','z0']].to_numpy();b=g[['x1','y1','z1']].to_numpy()
            if not np.isfinite(a).all() or not np.isfinite(b).all(): raise ValueError('nonfinite source coordinates')
            if accepted and frames and not np.array_equal(frames[-1][1], a):
                raise ValueError('accepted leaf endpoint discontinuity')
            # Remote torus is deliberately outside the U-solid view.
            # Source geometry still contains and validates both bodies.
            visible = owners==0 if scene=='connected' else np.ones(len(owners),dtype=bool)
            all_points.extend((a[faces[visible]].reshape(-1,3),b[faces[visible]].reshape(-1,3)))
            for t in np.linspace(0,1,7 if accepted else 19):
                x=a if t==0 else b if t==1 else (1-t)*a+t*b
                frames.append((receipt.start_s+t*receipt.dt_s,x,receipt.leaf,t))
        pts=np.concatenate(all_points); lo=pts.min(axis=0); hi=pts.max(axis=0)
        mid=(lo+hi)/2; span=max(float(np.max(hi-lo))*1.12,1e-6)
        fig=plt.figure(figsize=(7.2,6.0),layout='constrained'); ax=fig.add_subplot(111,projection='3d')
        ax.set(xlim=(mid[0]-span/2,mid[0]+span/2),ylim=(mid[1]-span/2,mid[1]+span/2),zlim=(mid[2]-span/2,mid[2]+span/2),xlabel='x (m)',ylabel='y (m)',zlabel='z (m)')
        ax.set_box_aspect((1,1,1));ax.view_init(elev=24,azim=-54)
        poly=Poly3DCollection([],edgecolor='0.3',linewidth=.5,alpha=.8);ax.add_collection3d(poly)
        colors=np.array(['C0','C1'])[np.clip(face_owners[visible],0,1)]
        label='ACCEPTED linear leaf' if accepted else 'REJECTED trial — never committed'
        focus=' | U-solid view; remote torus omitted' if scene=='connected' else ''
        def update(i):
            time,x,leaf,t=frames[i];poly.set_verts(x[faces[visible]]);poly.set_facecolor(colors)
            ax.set_title(f'{label}\nleaf {leaf} · source time {time*1000:.4f} ms · fraction {t:.2f}{focus}',fontsize=10)
            return [poly]
        update(len(frames)//2);fig.savefig(output/(kind+'.png'),dpi=150)
        anim=FuncAnimation(fig,update,frames=len(frames),blit=False)
        anim.save(output/(kind+'.gif'),writer=PillowWriter(fps=10),dpi=85);plt.close(fig)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();render(a.source,a.output)
