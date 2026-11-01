from pathlib import Path
import csv
from PIL import Image
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
root=Path(__file__).resolve().parents[1]
out=root/'output/native'
for name in ('native-initial','native-adaptive','native-baseline'):
    Image.open(out/(name+'.ppm')).save(out/(name+'.png'))
a=list(csv.DictReader(open(out/'adaptive_trace.csv')))
b=list(csv.DictReader(open(out/'baseline_trace.csv')))
assert len(a)==len(b)==120
assert all(x['positionHash']==y['positionHash'] for x,y in zip(a,b))
assert all(int(x['oracleMisses'])==0 for x in a+b)
rows=list(csv.DictReader(open(out/'live_benchmark.csv')))
assert len(rows)==2700 and all(int(x['oracleMisses'])==0 for x in rows)
policies=['baseline_rebuild','guarded_reuse']
vals=[np.array([float(x['stepMs']) for x in rows if x['policy']==p]) for p in policies]
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':16,'axes.spines.top':False,'axes.spines.right':False})
fig,ax=plt.subplots(figsize=(10,4.8),layout='constrained')
x=np.arange(2); width=.24
for k,(label,fn,color) in enumerate([('Mean',np.mean,'#2667d8'),('95th percentile',lambda a:np.percentile(a,95,method='inverted_cdf'),'#589b9b'),('Maximum',np.max,'#b56b00')]):
    heights=[fn(v) for v in vals]
    bars=ax.bar(x+(k-1)*width,heights,width,label=label,color=color)
    ax.bar_label(bars,fmt='%.2f',padding=4,fontsize=14)
ax.set_xticks(x,['Rebuild each iteration','Guarded reuse'])
ax.set_ylabel('CPU simulation step (ms)')
ax.set_ylim(0,5);ax.legend(frameon=False,ncol=3,loc='upper right',fontsize=13)
ax.grid(axis='y',alpha=.15);ax.set_axisbelow(True)
fig.savefig(out/'benchmark.png',dpi=180,facecolor='white')
print('Evidence: 120 paired capture states identical; zero oracle omissions; 2700 benchmark rows; PNGs saved.')
