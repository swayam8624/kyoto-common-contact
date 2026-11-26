import sys,json,time
from pathlib import Path
import numpy as np
from pass50_mesh_gate import MeshGate

def run(root,out):
 rows=[]
 for path in sorted(root.glob('committed*.npz')):
  d=np.load(path);s,t=d['states'],d['cells'];g=MeshGate(t)
  for i,(a,b) in enumerate(zip(s,s[1:])):
   start=time.perf_counter();r=g.check(a,b);r.update(scene=path.stem,step=i,seconds=time.perf_counter()-start);rows.append(r);print({k:v if k!='unresolved' else len(v) for k,v in r.items()},flush=True)
  out.write_text(json.dumps(rows,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]),Path(sys.argv[2]))
