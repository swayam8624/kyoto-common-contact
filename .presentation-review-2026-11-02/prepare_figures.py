from pathlib import Path
import csv,json,statistics as st
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[1]; base=root/'output/review-2026-11-02'; data=base/'after'; private=Path(__file__).parent
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':16,'axes.spines.top':False,'axes.spines.right':False,'axes.titleweight':'bold'})
policies=['sap_rebuild','global_guarded','region_fixed','region_adaptive','region_sweep_fixed','region_sweep_adaptive','online_contact_work']
labels=['SAP rebuild','Global guard','Flat / fixed','Flat / adaptive','Sweep / fixed','Sweep / adaptive','Online controller']
trials=[list(csv.DictReader(open(data/f'multicloth-{t}.csv'))) for t in (1,2,3)]
assert all(len(rows)==21 for rows in trials)
assert all(float(r['maxPositionError'])==0 for rows in trials for r in rows)
summary={}
for sheets in (1,2,3):
 summary[str(sheets)]={}
 for p in policies:
  rows=[next(r for r in trial if r['policy']==p and int(r['sheets'])==sheets) for trial in trials]
  values=[float(r['meanStepMs']) for r in rows]
  summary[str(sheets)][p]={'mean':st.fmean(values),'min':min(values),'max':max(values),'broad':st.fmean(float(r['meanBroadMs']) for r in rows),'narrow':st.fmean(float(r['meanNarrowMs']) for r in rows),'candidates':st.fmean(float(r['meanCandidates']) for r in rows),'interCorrections':st.fmean(float(r['meanInterContacts']) for r in rows)}
fig,ax=plt.subplots(figsize=(11.5,5.6),layout='constrained')
vals=[summary['3'][p]['mean'] for p in policies]; lo=[v-summary['3'][p]['min'] for p,v in zip(policies,vals)]; hi=[summary['3'][p]['max']-v for p,v in zip(policies,vals)]
colors=['#536779','#8c6d3b','#9aa8b6','#9aa8b6','#66969e','#66969e','#167c75']
bars=ax.barh(labels,vals,xerr=np.array([lo,hi]),color=colors,capsize=4)
ax.invert_yaxis();ax.set_xlabel('Complete CPU step (ms; lower is better)');ax.grid(axis='x',alpha=.15);ax.set_axisbelow(True)
ax.set_xlim(0,max(summary['3'][p]['max'] for p in policies)*1.20)
ax.bar_label(bars,labels=[f'  {v:.2f}' for v in vals],padding=7)
fig.savefig(private/'policies.png',dpi=180,facecolor='white');plt.close(fig)
trace=list(csv.DictReader(open(data/'online-1.csv')))
fig,axes=plt.subplots(2,1,figsize=(11.5,6.1),sharex=True,layout='constrained',gridspec_kw={'height_ratios':[1.2,1]})
x=[int(r['frame']) for r in trace]
for key,label,color in [('sapStepMs','SAP','#a2783e'),('onlineStepMs','Online','#167c75')]: axes[0].plot(x,[float(r[key]) for r in trace],label=label,color=color,lw=1.6)
axes[0].set_ylabel('CPU step (ms)');axes[0].legend(frameon=False,ncol=2,loc='upper right')
axes[1].step(x,[int(r['onlinePatches']) for r in trace],where='post',color='#167c75');axes[1].set_ylabel('Region leaves');axes[1].set_xlabel('Physics timestep')
for ax in axes:
 ax.grid(alpha=.15)
 for r in trace:
  if int(r['onlineSplits']): ax.axvline(int(r['frame']),color='#2667b8',alpha=.2,lw=1)
fig.savefig(private/'online-trace.png',dpi=180,facecolor='white');plt.close(fig)
fig,ax=plt.subplots(figsize=(11,2),layout='constrained');ax.axis('off');ax.text(.5,.55,r'$\forall i:\ B_i(t)\subseteq E_i\quad\Longrightarrow\quad C_{\mathrm{current}}(t)\subseteq C_{\mathrm{cached}}$',ha='center',va='center',fontsize=25,color='#172536');fig.savefig(private/'certificate.png',dpi=200,facecolor='white');plt.close(fig)
trace_stats=[]
for t in (1,2,3):
 rows=list(csv.DictReader(open(data/f'online-{t}.csv')))
 trace_stats.append({'trial':t,'splits':sum(int(r['onlineSplits']) for r in rows),'merges':sum(int(r['onlineMerges']) for r in rows),'switches':sum(int(r['onlineStrategySwitches']) for r in rows),'auditedFrames':sum(int(r['coverageChecked']) for r in rows),'maxError':max(float(r['maxPositionError']) for r in rows),'onlineMean':st.fmean(float(r['onlineStepMs']) for r in rows[12:]),'sapMean':st.fmean(float(r['sapStepMs']) for r in rows[12:])})
summary['trace']=trace_stats
(private/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
lines=['# Fresh local research measurements — 2 November 2026','','Raw source: `output/review-2026-11-02/after/`. Three local repetitions; Apple M2 Pro, Release build. Complete CPU step includes periodic exhaustive oracle; rendering excluded. Seven-policy comparisons advance 60 steps, exclude first eight; the controller trace advances 160, excludes first twelve for averages. Ranges below are ranges of trial means, not confidence intervals.','','| Sheets | Policy | Mean CPU step (ms) | Trial-mean range (ms) |','|---|---|---:|---:|']
for s in ('1','2','3'):
 for p in policies:
  q=summary[s][p];lines.append(f"| {s} | {p} | {q['mean']:.3f} | {q['min']:.3f}–{q['max']:.3f} |")
lines+=['','All 63 coupled result rows report maximum position error 0 against SAP. The online trace separately compares every frame and periodically audits candidate coverage.','',json.dumps(trace_stats,indent=2),'','These are one small deterministic family of fixtures, not independent scene samples. Timing-guided strategy decisions can differ between repetitions. No generic speedup, physical accuracy or collision-free guarantee follows.','The 480-frame stability probe reports damping/velocity clamps as well as geometric bounds; these stabilizing interventions must be disclosed.']
(base/'RESULTS.md').write_text('\n'.join(lines)+'\n')
print(json.dumps({'three_cloth':summary['3'],'trace':trace_stats},indent=2))
