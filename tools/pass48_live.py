"""Execute the unchanged independent FEM model with the optimized gate.
Compare accepted states and physical receipts with a separately integrated
original Gate reference, not merely with replayed candidate geometry.
"""
import argparse,json,time
from pathlib import Path
import numpy as np
import pass45_live as live
from pass45_transaction import Gate
from pass48_integer_region import IntegerRegionGate

def run(out,steps=16,scene=2):
    out.mkdir(exist_ok=False)
    results=[]
    for name,cls in [('reference',Gate),('integer',IntegerRegionGate)]:
        live.Gate=cls
        start=time.perf_counter_ns();live.run(out/name,steps,scene);elapsed=time.perf_counter_ns()-start
        results.append(dict(arm=name,ns=elapsed))
    a=np.load(out/'reference'/f'scene{scene}.npz');b=np.load(out/'integer'/f'scene{scene}.npz')
    assert np.array_equal(a['states'],b['states']) and np.array_equal(a['cells'],b['cells'])
    assert (out/'reference'/'steps.csv').read_bytes()==(out/'integer'/'steps.csv').read_bytes()
    ref=json.loads((out/'reference'/'summary.json').read_text());opt=json.loads((out/'integer'/'summary.json').read_text());assert ref==opt and opt[0]['transaction_steps']==steps and opt[0]['unresolved']==0
    report=dict(scene=scene,steps_per_arm=steps,bitwise_state_parity=True,byte_identical_receipts=True,physical=opt[0],timings=results,scope='single sequential fresh run each; exploratory timing, no whole-solver speedup claim')
    (out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('--steps',type=int,default=16);p.add_argument('--scene',type=int,default=2);a=p.parse_args();run(a.out,a.steps,a.scene)
