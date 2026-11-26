"""Frozen Pass56 solver: predeclared unseen seeds, proper rotations, remapped IDs,
velocity/shape perturbation, exact clock and fresh certification of accepted leaves.
A rejected macrostep is recorded, not silently replaced with an easier fixture.
"""
import argparse,json,math
from pathlib import Path
from fractions import Fraction as Q
import numpy as np
from pass54_adaptive_discovery import AdaptiveDiscovery,geometry,same
from pass56_parametric import ParametricModel
from pass50_mesh_gate import MeshGate

SEEDS=(61013,61027,61031,61043)
KINDS=('edge','vertex','disjoint')

def fixture(kind,seed):
    rng=np.random.default_rng(seed);x,t,v=geometry(kind,False)
    R,_=np.linalg.qr(rng.normal(size=(3,3)))
    if np.linalg.det(R)<0:R[:,0]*=-1
    scale=rng.uniform(.85,1.15,3);speed=float(rng.uniform(.75,1.8))
    x=(x*scale)@R.T+rng.uniform(-.07,.07,3);v=(v*speed)@R.T
    order=rng.permutation(len(x));inverse=np.argsort(order);x,v=x[order],v[order];t=inverse[t][rng.permutation(len(t))]
    dt=float(rng.choice([.002,.004,.006]));return x,t,v,dt,speed

def run(out):
    out.mkdir(exist_ok=False);rows=[]
    for kind in KINDS:
        for seed in SEEDS:
            x,t,v,dt,speed=fixture(kind,seed);m=ParametricModel(x,t,v,12);a=AdaptiveDiscovery(m,x,v);attempts=[];done=0;error=''
            for _ in range(8):
                before=a.state;trace=a.trace;cache=m.gate.cache.copy()
                ok,r,error=a.advance(dt,depth=4,max_move=.0032);attempts.append(dict(ok=ok,attempts=r,error=error))
                if not ok:
                    assert same(before,a.state) and a.trace is trace and cache.keys()==m.gate.cache.keys() and all(np.array_equal(z,m.gate.cache[k]) for k,z in cache.items());break
                done+=1
            assert a.state.time==done*Q(dt)
            gate=MeshGate(t);prev=x;checks=0
            for state,r in a.trace:
                assert np.array_equal(state.x,prev+float(Q(r['dt']))*state.v)
                z=gate.check(prev,state.x);assert not z['unresolved'];checks+=z['total_pairs'];prev=state.x
            name=f'{kind}_{seed}';np.savez_compressed(out/(name+'.npz'),states=[x,*[s.x for s,_ in a.trace]],velocities=[v,*[s.v for s,_ in a.trace]],cells=t)
            (out/(name+'.json')).write_text(json.dumps(attempts,indent=2))
            row=dict(kind=kind,seed=seed,dt=dt,speed=speed,requested=8,completed=done,leaves=len(a.trace),pairs=checks,active=sum(r['impulse']>1e-7 for _,r in a.trace),rejected_trials=sum(z['status']!='accepted' for q in attempts for z in q['attempts']),max_discovered=max((len(r['attempts'][-1]['keys']) for _,r in a.trace),default=0),error=error,rollback_verified=done<8)
            rows.append(row);print(row,flush=True);(out/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert len(rows)==len(KINDS)*len(SEEDS)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);run(p.parse_args().out)
