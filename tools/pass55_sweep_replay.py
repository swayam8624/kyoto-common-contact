"""Exact receipt parity for archived accepted paths with sparse sweep culling."""
import json,sys,time
from pathlib import Path
import numpy as np
from pass50_mesh_gate import MeshGate
from pass55_sweep_gate import SweepMeshGate

def run(root,out,names):
    rows=[]
    for name in names:
        with np.load(root/name/'trajectory.npz') as d:s,t=d['states'],d['cells']
        refs=None;times={}
        for cls in (MeshGate,SweepMeshGate):
            g=cls(t);start=time.perf_counter_ns();rr=[g.check(a,b) for a,b in zip(s,s[1:])];times[cls.__name__]=time.perf_counter_ns()-start
            if refs is None:refs=rr
            else:assert rr==refs,(name,'receipt mismatch')
            assert all(not r['unresolved'] for r in rr)
        row=dict(case=name,steps=len(refs),pairs=sum(r['total_pairs'] for r in refs),culled=sum(r['aabb_culled'] for r in refs),timings_ns=times,ratio=times['MeshGate']/times['SweepMeshGate']);rows.append(row);print(row,flush=True);out.write_text(json.dumps(rows,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]),Path(sys.argv[2]),sys.argv[3:])
