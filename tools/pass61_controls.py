"""Fail-closed budget exhaustion on the same held-out physical fixtures."""
import json
from fractions import Fraction as Q
import numpy as np
from pass61_physical import fixture,SEEDS,KINDS
from pass54_adaptive_discovery import AdaptiveDiscovery,same
from pass56_parametric import ParametricModel

def run():
    rows=[]
    for kind in KINDS:
        for seed in SEEDS:
            x,t,v,dt,_=fixture(kind,seed);m=ParametricModel(x,t,v,12);a=AdaptiveDiscovery(m,x,v)
            before=a.state;trace=a.trace;cache=m.gate.cache.copy()
            ok,attempts,error=a.advance(dt,depth=0,max_move=1e-20)
            assert not ok and error and same(a.state,before) and a.trace is trace and a.state.time==Q(0)
            assert cache.keys()==m.gate.cache.keys() and all(np.array_equal(q,m.gate.cache[k]) for k,q in cache.items())
            ok,recovery,error2=a.advance(dt,depth=4,max_move=.0032)
            assert ok and a.state.time==Q(dt) and all(r['certified'] for _,r in a.trace)
            rows.append(dict(kind=kind,seed=seed,rejection=error,rollback=True,recovery=True,recovery_leaves=len(a.trace)))
    return rows
if __name__=='__main__':print(json.dumps(run(),indent=2))
