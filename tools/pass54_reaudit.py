"""Re-run whole-mesh certificates from saved binary64 geometry, using fresh gates."""
import argparse,json
from pathlib import Path
import numpy as np
from pass50_mesh_gate import MeshGate

def run(path):
    with np.load(path/'trajectory.npz') as data:
        states,t=data['states'],data['cells']
    gate=MeshGate(t)
    counts=[]
    for a,b in zip(states,states[1:]):
        r=gate.check(a,b)
        assert not r['unresolved'],(path,r['unresolved'])
        assert r['total_pairs']==sum(r[k] for k in ('aabb_culled','strict_disjoint','shared_vertex','shared_edge','shared_face'))
        counts.append(r['total_pairs'])
    return dict(case=str(path),leaves=len(counts),certified_pairs=sum(counts))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('cases',nargs='+',type=Path);a=p.parse_args()
    rows=[]
    for path in a.cases:
        rows.append(run(path));print(rows[-1],flush=True)
        a.out.write_text(json.dumps(rows,indent=2)+'\n')
