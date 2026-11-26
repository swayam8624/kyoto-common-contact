"""Recertify completed matched runs and independently compare recorded decisions."""
import json
from pathlib import Path
import numpy as np
from pass57_benchmark import sig
from pass50_mesh_gate import MeshGate

def run(root):
    results=[]
    for folder,block in [('vertex57',0),('vertex57',1),('replica57',0),('replica57_reverse',0)]:
        p=root/folder;rows=[r for r in json.loads((p/'rows.json').read_text()) if r['block']==block];assert len(rows)==2
        a=np.load(p/f'{block}_fixed.npz');b=np.load(p/f'{block}_parametric.npz');assert a['states'].shape==b['states'].shape
        assert sig(json.loads((p/f'{block}_fixed.json').read_text()))==sig(json.loads((p/f'{block}_parametric.json').read_text()))
        count=0
        for d in (a,b):
            g=MeshGate(d['cells']);states=d['states']
            for x,y in zip(states,states[1:]):
                z=g.check(x,y);assert not z['unresolved'];count+=z['total_pairs']
        times={r['arm']:r['ns'] for r in rows};result=dict(folder=folder,block=block,ratio=times['fixed']/times['parametric'],pair_intervals=count,leaves=sum(r['leaves'] for r in rows),position_error=float(np.max(np.abs(a['states']-b['states']))),velocity_error=float(np.max(np.abs(a['velocities']-b['velocities']))));results.append(result);print(result,flush=True)
    (root/'audit.json').write_text(json.dumps(results,indent=2));print('TOTAL',sum(r['pair_intervals'] for r in results),sum(r['leaves'] for r in results))
if __name__=='__main__':run(Path('.'))
