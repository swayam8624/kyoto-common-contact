"""Render measured research evidence only. Run: python pass63_figures.py inputs figures."""
import sys,json,math
from pathlib import Path
from fractions import Fraction
from collections import Counter
from itertools import combinations
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import TwoSlopeNorm, Normalize
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

I,O=map(Path,sys.argv[1:]);O.mkdir(parents=True,exist_ok=True)
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False,'svg.fonttype':'none','figure.dpi':130,'savefig.dpi':300})
blue='#176B87'; orange='#B45B32'; grey='#64748B'
labels={'exhaustive':'Exhaustive','rebuilt_sap':'Rebuilt SAP','rebuilt_bvh':'Rebuilt BVH','retained_refit':'Retained refit','retained_frontier':'Retained frontier','persistent_x':'Persistent X','adaptive_axis':'Adaptive axis','persistent_axis':'Persistent axis','cached_axis':'Cached axis','spatial_first':'Spatial-first','fused':'Fused SAP','ipc_lbvh':'IPC LBVH'}
def load(name):return json.loads((I/name).read_text())
def save(fig,name,caption):
    fig.text(.02,.015,caption,fontsize=8,color='#475569',wrap=True)
    fig.tight_layout(rect=(0,.065,1,1));fig.savefig(O/(name+'.png'),bbox_inches='tight',pad_inches=.15);fig.savefig(O/(name+'.svg'),bbox_inches='tight',pad_inches=.15);plt.close(fig)
def table(path):
    d=pd.read_csv(I/path);g=d.groupby(['scene','width','regime','block','arm']).ns.sum().unstack();return g

def ratios(g,base):return g.apply(lambda s:g[base]/s)
def gm(x):return np.exp(np.log(x).mean())
def configs(g,base):return np.exp(np.log(ratios(g,base)).groupby(level=[0,1,2]).mean())
stock=table('stock59.csv');held=table('stock/stock61.csv');assert len(stock)==204 and len(held)==240
c=configs(stock,'exhaustive');h=configs(held,'exhaustive')
# 01: all production backends on the same archived workload configurations.
a=np.exp(np.log(c).mean()).sort_values();fig,ax=plt.subplots(figsize=(7.4,5.4));ax.barh([labels[k] for k in a.index],a,color=[blue if k=='fused' else grey for k in a.index]);ax.axvline(1,color='black',lw=.8);ax.set(xlabel='Speedup vs exhaustive (×)',title='Original replay | 12 production backends',xlim=(0,3.15));ax.bar_label(ax.containers[0],fmt='%.2f×',padding=4);save(fig,'01_backend_ranking','Pass 59 • 17 configurations, 300 frames, 12 balanced blocks; paid query only. Equal-configuration geometric mean.')
# 02: fixed-suite intervals; not a population CI.
s=load('statistics.json');pairs=[('archive12','ipc_lbvh','Original / IPC'),('held_stock','ipc_lbvh','Held-out / IPC'),('archive12','persistent_axis','Original / persistent'),('held_gcc','persistent_axis','Held-out GCC / persistent'),('held_clang','persistent_axis','Held-out Clang / persistent')];v=[next(x for x in s[k] if x['baseline']==b and x['arm']=='fused') for k,b,_ in pairs];m=np.array([x['ratio'] for x in v]);lo,hi=np.array([x['fixed_suite_block_bootstrap_95'] for x in v]).T
fig,ax=plt.subplots(figsize=(8,3.8));y=np.arange(len(v));ax.errorbar(m,y,xerr=[m-lo,hi-m],fmt='o',color=blue,capsize=4);ax.axvline(1,color='black',ls='--',lw=.8);ax.set(yticks=y,yticklabels=[p[2] for p in pairs],xlabel='Fused SAP speedup (×)',title='Paired timing estimates and fixed-suite uncertainty',xlim=(.8,3.8));ax.invert_yaxis();save(fig,'02_fixed_suite_intervals','Pass 62 • 95% block bootstrap intervals measure repeat timing variability; not general-population confidence intervals.')
# 03 and 04: per-configuration method performance.
for mat,name,title in [(c,'03_original_heatmap','Original replay'),(h,'04_heldout_heatmap','Held-out cubic replay')]:
    order=list(labels);z=np.log2(mat[order].values);lim=max(abs(z.min()),abs(z.max()));fig,ax=plt.subplots(figsize=(11,7.2));im=ax.imshow(z,aspect='auto',cmap='RdBu',norm=TwoSlopeNorm(vmin=-lim,vcenter=0,vmax=lim));ax.set_xticks(range(len(order)),[labels[k] for k in order],rotation=45,ha='right');ax.set_yticks(range(len(mat)),[f'{i[0]} · {i[2]}' for i in mat.index]);ax.set_title(title+' | speed relative to exhaustive');fig.colorbar(im,ax=ax,label='log₂ speedup (0 = parity)',shrink=.8);save(fig,name,'Pass 59 / 61 • Matched block geometric means. Blue = faster, red = slower; each row is one configuration.')
# 05: replicas only, avoid implying interpolated workloads.
fig,ax=plt.subplots(figsize=(7,4.4));
for regime,col in [('sparse',blue),('dense',orange)]:
    for arm,style in [('fused','-'),('ipc_lbvh','--')]:
        v=[c.loc[(f'replica{w}_{regime}',w,'half_mm'),arm] for w in (2,4,8)];ax.plot([2,4,8],v,style,marker='o',color=col,label=f'{labels[arm]} · {regime}')
ax.axhline(1,color='black',lw=.8);ax.set(xlabel='Replicated bodies (copies)',ylabel='Speedup vs exhaustive (×)',xticks=[2,4,8],title='Scaling under sparse and dense overlap');ax.legend(frameon=False);save(fig,'05_replica_scaling','Pass 59 • Actual archived replica workloads. Lines guide the eye; no intermediate sizes measured.')
# 06: distributions of block sums, one point per matched configuration/block.
fig,ax=plt.subplots(figsize=(8,4.6));sets=[ratios(stock,'ipc_lbvh').fused,ratios(held,'ipc_lbvh').fused,ratios(stock,'persistent_axis').fused,ratios(held,'persistent_axis').fused];ax.boxplot(sets,vert=False,tick_labels=['Original / IPC','Held-out / IPC','Original / persistent','Held-out / persistent'],showfliers=True);ax.axvline(1,color='black',ls='--');ax.set(xscale='log',xlabel='Paired configuration-block speedup (×; log scale)',title='Timing distributions | 204 original and 240 held-out blocks per comparison');save(fig,'06_paired_distributions','Pass 59 / 61 • Descriptive distributions, not independent draws; whiskers = 1.5× IQR.')
# 07: gate-only witness reuse, all measured repetitions shown.
abl=load('ablation_summary.json');cases=abl['cases'];fig,ax=plt.subplots(figsize=(8,6));y=np.arange(len(cases));ax.barh(y,[r['ratio'] for r in cases],color=blue,alpha=.78);ax.scatter([v for r in cases for v in r['ratios']],[j for j,r in enumerate(cases) for _ in r['ratios']],s=18,color='black',label='Matched block');ax.set(yticks=y,yticklabels=[r['case'] for r in cases],xlabel='Cold / retained certification time (×)',title='One-factor ablation | verified witness reuse');ax.axvline(1,color='black',ls='--');ax.legend(frameon=False);save(fig,'07_witness_ablation','Pass 62 • 50 blocks, 13 cases, identical certification decisions. Gate-only timing, not whole-solver speedup.')
# 08: fresh-process measured RSS and runtime trade-off.
mem=load('audit58.json');fig,ax=plt.subplots(figsize=(7,4.8));
for kind,marker in [('u78','o'),('comb120','s'),('u156','^')]:
    rr=[r for r in mem if r['kind']==kind];ax.scatter([100*(1-r['rss_ratio']) for r in rr],[r['speed_ratio'] for r in rr],marker=marker,s=80,label=kind)
ax.axhline(1,color='black',ls='--');ax.axvline(0,color=grey,lw=.8);ax.set(xlabel='Peak RSS reduction (%)',ylabel='Baseline / alternative runtime (×)',title='Shared-graph alternative | memory–runtime trade-off',ylim=(.5,1.08));ax.legend(frameon=False);save(fig,'08_memory_tradeoff','Pass 58 • Five fresh-process matched blocks; 156-cell mesh has one block. Below 1 = slower.')
# 09/10: actual accepted U mesh, discovered pair incidences, not physical contact force.
with np.load(I/'trajectory.npz') as d:x,t=d['states'],d['cells']
leaves=[r for block in load('attempts.json') for r in block if r['status']=='accepted'];assert len(x)==len(leaves)+1 and len(t)==156
keys=[r['attempts'][-1]['keys'] for r in leaves];degree=np.zeros((len(t),len(keys)),int)
for k,ps in enumerate(keys):
    for i,j in ps:degree[i,k]+=1;degree[j,k]+=1
assert np.array_equal(degree.sum(0),2*np.array([len(p) for p in keys]))
time=np.r_[0,np.cumsum([float(Fraction(r['dt']))*1000 for r in leaves])];assert np.isclose(time[-1],16)
faces=Counter(tuple(sorted(face)) for tet in t for face in combinations(tet,3));boundary=np.array([f for f,n in faces.items() if n==1]);norm=Normalize(0,max(1,degree.max()));fig=plt.figure(figsize=(11,8));limits=np.array([x.min((0,1)),x.max((0,1))])*1000
for k,j in enumerate([0,3,5,7],1):
    ax=fig.add_subplot(2,2,k,projection='3d');p=x[j]*1000;ax.add_collection3d(Poly3DCollection(p[boundary],facecolors='#C9D9E5',edgecolors='#49677B',linewidths=.3,alpha=.23));ax.set(xlim=limits[:,0],ylim=limits[:,1],zlim=limits[:,2],xlabel='x (mm)',ylabel='y (mm)',zlabel='z (mm)',title=f't = {time[j]:.1f} ms · {len(keys[j-1]) if j else 0} discovered pairs');ax.set_box_aspect(np.ptp(limits,axis=0));ax.view_init(57,-70)
    if j:
        mask=degree[:,j-1]>0;centers=p[t].mean(1);ax.scatter(*centers[mask].T,c=degree[mask,j-1],cmap='magma',norm=norm,s=14+degree[mask,j-1]*1.6,depthshade=False)
cax=fig.add_axes([.91,.28,.018,.45]);fig.colorbar(plt.cm.ScalarMappable(norm=norm,cmap='magma'),cax=cax,label='Discovered pair incidence per tetrahedron');fig.suptitle('Accepted 156-tetrahedron U-shaped solid | physical trajectory',fontsize=14);fig.subplots_adjust(left=.02,right=.84,top=.90,bottom=.09,wspace=.13,hspace=.20);fig.text(.02,.02,'Pass 58 baseline • Actual accepted endpoints; markers are constraint incidences, not measured forces or penetration.',fontsize=8);fig.savefig(O/'09_u156_trajectory.png',dpi=300,bbox_inches='tight');fig.savefig(O/'09_u156_trajectory.svg',bbox_inches='tight');plt.close(fig)
fig,ax=plt.subplots(figsize=(8,5));im=ax.imshow(degree,aspect='auto',cmap='magma',interpolation='nearest');ax.set(xticks=np.arange(len(keys)),xticklabels=[f'{z:.1f}' for z in time[1:]],xlabel='Accepted endpoint time (ms; categorical steps)',ylabel='Tetrahedron ID',title='Discovered constraint incidence over accepted substeps');fig.colorbar(im,ax=ax,label='Number of discovered pair constraints');save(fig,'10_contact_incidence','Pass 58 baseline • Counts from final accepted solve per substep; not a contact-pressure or penetration map.')
# 11: actual discovery pair adjacency at final endpoint.
mat=np.zeros((len(t),len(t)),int)
for i,j in keys[-1]:mat[i,j]=mat[j,i]=1
assert mat.sum()==2*len(keys[-1]);fig,ax=plt.subplots(figsize=(6.6,6));ax.imshow(mat,cmap='Blues',vmin=0,vmax=1,interpolation='nearest');ax.set(xlabel='Tetrahedron ID',ylabel='Tetrahedron ID',title=f'Final accepted constraint graph | {len(keys[-1])} pairs');save(fig,'11_constraint_adjacency','Pass 58 baseline • Symmetric discovered constraint graph at t = 16 ms; white = no listed constraint, not proof of no physical contact.')
# 12: held-out failure/recovery pressure, not failed physical durations.
phys=pd.DataFrame(load('held_physical.json'));assert len(phys)==12 and phys.completed.sum()==96
fig,ax=plt.subplots(figsize=(8,5));y=np.arange(len(phys));ax.barh(y,phys.leaves,label='Accepted leaves',color=blue);ax.barh(y,phys.rejected_trials,left=phys.leaves,label='Rejected trials',color=orange);ax.set(yticks=y,yticklabels=phys.kind+' '+phys.seed.astype(str),xlabel='Trial count',title='Held-out adaptive integration | 96/96 macrosteps completed');ax.legend(frameon=False);save(fig,'12_adaptive_rejections','Pass 61 • Rejections are subdivided trials, not failed completed macrosteps; 12 independent fixture/seed combinations.')
print('Rendered',len(list(O.glob('*.png'))),'figure pairs')
