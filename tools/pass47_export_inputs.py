import numpy as np
from pathlib import Path
import sys
root=Path(sys.argv[1]); out=Path(sys.argv[2]); out.mkdir(exist_ok=True)
for path in sorted(root.glob('scene*.npz')):
 d=np.load(path); t=d['cells']; s=d['states']; counts={}
 for c in t:
  for k in range(4):
   f=tuple(sorted(np.delete(c,k)));counts[f]=counts.get(f,0)+1
 faces=sorted(f for f,n in counts.items() if n==1); assert max(counts.values())<=2
 edges=sorted({tuple(sorted((f[i],f[(i+1)%3]))) for f in faces for i in range(3)})
 ids=sorted({i for f in faces for i in f}); remap={i:j for j,i in enumerate(ids)}
 with (out/(path.stem+'.txt')).open('w') as w:
  print(len(ids),len(edges),len(faces),len(s),file=w)
  for f in edges+faces: print(*(remap[i] for i in f),file=w)
  np.savetxt(w,s[:,ids,:].reshape(-1,3),fmt='%.17g')
