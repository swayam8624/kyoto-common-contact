import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from pathlib import Path
p=Path(__file__).parent
for name,formula in {
'complete':r'$C_{\mathrm{required}} \subseteq C_{\mathrm{broad}}$',
'bound':r'$B_P = \operatorname{AABB}\!\left(P(t_0)\cup P(t_1)\right)\oplus[-\delta/2,\,\delta/2]^3$',
'proximity':r'$d\!\left(v(t),T\right)\leq\delta$',
}.items():
 fig=plt.figure(figsize=(12,1.5));fig.text(.5,.5,formula,ha='center',va='center',fontsize=32,color='#17202a');plt.axis('off');fig.savefig(p/(name+'.svg'),transparent=True,bbox_inches='tight',pad_inches=.1);plt.close(fig)
