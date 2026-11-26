"""Compare matched fresh-process receipts; independently recertify saved geometry."""
import argparse,json,math
from pathlib import Path
import numpy as np
from pass56_benchmark import sig
from pass50_mesh_gate import MeshGate

def run(root,kinds,blocks):
    rows=[]
    for kind in kinds:
        for block in range(blocks):
            paths=[root/f'{kind}_{block}_{arm}' for arm in ('base','sparse')]
            receipts=[json.loads((p/'summary.json').read_text()) for p in paths]
            a,b=[np.load(p/'trajectory.npz') for p in paths]
            assert np.array_equal(a['cells'],b['cells']) and a['states'].shape==b['states'].shape
            assert sig(json.loads((paths[0]/'attempts.json').read_text()))==sig(json.loads((paths[1]/'attempts.json').read_text()))
            pos=float(np.max(np.abs(a['states']-b['states'])));vel=float(np.max(np.abs(a['velocities']-b['velocities'])))
            assert pos<1e-10 and vel<1e-8
            count=0
            for d,r in zip((a,b),receipts):
                g=MeshGate(d['cells']);n=0
                for x,y in zip(d['states'],d['states'][1:]):
                    z=g.check(x,y);assert not z['unresolved'];n+=z['total_pairs']
                assert n==r['pairs'];count+=n
            r,s=receipts
            row=dict(kind=kind,block=block,speed_ratio=r['ns']/s['ns'],rss_ratio=s['peak_rss_bytes']/r['peak_rss_bytes'],position_error=pos,velocity_error=vel,decision_parity=True,leaves=r['leaves']+s['leaves'],recertified_pairs=count,base=r,sparse=s)
            rows.append(row);print(row,flush=True)
            (root/'audit58.json').write_text(json.dumps(rows,indent=2)+'\n')
    return rows
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('--kinds',nargs='+',default=['comb120','u78','u156']);p.add_argument('--blocks',type=int,default=2);a=p.parse_args();run(a.root,a.kinds,a.blocks)
