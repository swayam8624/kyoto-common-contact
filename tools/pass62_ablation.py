"""One-factor ablation: retain versus clear verified separating-axis hints.
Same frozen accepted geometry, gate, orientation, AABB and fallback. Only
cross-interval cache reuse changes. No solver-time or physical accuracy claim.
"""
import argparse,json,time,math,statistics
from pathlib import Path
import numpy as np
from pass50_mesh_gate import MeshGate

def run(paths,blocks,out):
    rows=[]
    for path in paths:
        with np.load(path) as d: states=d['states'];cells=d['cells']
        reference=None
        for block in range(blocks):
            for arm in ('retained','cold') if block%2==0 else ('cold','retained'):
                gate=MeshGate(cells);receipts=[];proposals=0;start=time.perf_counter_ns()
                for a,b in zip(states,states[1:]):
                    if arm=='cold':gate.cache.clear()
                    r=gate.check(a,b);proposals+=r.pop('proposals');assert not r['unresolved'];receipts.append(r)
                ns=time.perf_counter_ns()-start
                if reference is None:reference=receipts
                assert receipts==reference,(str(path),arm,'certificate mismatch')
                row=dict(case=path.stem if path.stem!='trajectory' else path.parent.name,block=block,arm=arm,cells=len(cells),leaves=len(receipts),pairs=sum(r['total_pairs'] for r in receipts),ns=ns,proposals=proposals)
                rows.append(row);print(row,flush=True);out.write_text(json.dumps(rows,indent=2)+'\n')
    return rows
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('paths',nargs='+',type=Path);p.add_argument('--blocks',type=int,default=4);a=p.parse_args();run(a.paths,a.blocks,a.out)
