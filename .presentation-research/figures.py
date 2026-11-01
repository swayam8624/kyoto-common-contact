import json,re,pathlib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon,Rectangle
root=pathlib.Path(__file__).resolve().parent.parent
h=(root/'output/Common_Broad_Phase_Demo.html').read_text()
d=json.loads(re.search(r'const DATA=(.*?);\nconst svg',h,re.S).group(1))
for idx,name in [(64,'false-positive'),(40,'within-margin'),(0,'pruned')]:
 q=d['samples'][idx]; tri=d['triangle'];p=q['point'];delta=d['delta'];col='#18834b' if q['withinDelta'] else '#b56b00' if q['candidate'] else '#596575'
 fig,ax=plt.subplots(figsize=(6,4),layout='constrained'); ax.set_aspect('equal');ax.set_xlim(-1.15,1.15);ax.set_ylim(-.65,.72)
 ax.add_patch(Polygon([(v[0],v[1]) for v in tri],facecolor='#edf1f5',edgecolor='#17202a',lw=2))
 b=d['bounds'][1];ax.add_patch(Rectangle(b['min'][:2],b['max'][0]-b['min'][0],b['max'][1]-b['min'][1],fill=False,edgecolor='#2667d8',lw=2))
 ax.add_patch(Rectangle((p[0]-delta/2,p[1]-delta/2),delta,delta,fill=False,edgecolor=col,lw=2))
 ax.plot(p[0],p[1],'o',ms=9,mfc='white',mec=col,mew=2)
 ax.annotate('moving vertex',(p[0],p[1]),xytext=(p[0]-.25,.38),fontsize=12,color=col,arrowprops={'arrowstyle':'-','color':col})
 ax.text(-.18,-.25,'triangle',fontsize=12,color='#17202a');ax.axhline(.08,color='#aab4c0',ls='--',lw=.8,zorder=0)
 ax.set_xlabel('x',fontsize=12);ax.set_ylabel('y',fontsize=12);ax.tick_params(labelsize=10);ax.spines[['top','right']].set_visible(False)
 ax.set_title('XY projection · vertex remains above the triangle plane',fontsize=11,color='#596575')
 fig.savefig(root/'.presentation-research'/f'{name}.svg',format='svg');fig.savefig(root/'.presentation-research'/f'{name}.png',dpi=180);plt.close(fig)
print('Rendered three figures from C++ snapshot data; no browser capture or synthetic result data.')
