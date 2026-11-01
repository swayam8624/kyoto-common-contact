import pathlib,matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=pathlib.Path(__file__).resolve().parent
for name,formula in {'complete':r'$C_{\mathrm{required}}\subseteq C_{\mathrm{broad}}$','bound':r'$B_P=\operatorname{AABB}(P)\oplus[-\delta/2,\delta/2]^3$'}.items():
 fig=plt.figure(figsize=(12,1.5));fig.text(.5,.5,formula,ha='center',va='center',fontsize=32,color='#17202a');plt.axis('off');fig.savefig(root/(name+'.png'),dpi=250,transparent=True,bbox_inches='tight',pad_inches=.1);plt.close(fig)
