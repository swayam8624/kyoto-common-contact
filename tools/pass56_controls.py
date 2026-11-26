"""Exercise duration reuse, eviction and transactional recovery."""
from fractions import Fraction as Q
import json
import numpy as np
from pass54_adaptive_discovery import AdaptiveDiscovery,geometry,same
from pass56_parametric import ParametricModel

def run():
    x,t,v=geometry('vertex',True);m=ParametricModel(x,t,v,1);a=AdaptiveDiscovery(m,x,v)
    for dt in (.002,.001,.0005):
        assert a.advance(dt,depth=0)[0]
    assert m.models.stats()['misses']==1 and m.models.stats()['hits']==2
    before=a.state;trace=a.trace;cache=m.gate.cache.copy()
    ok,rows,err=a.advance(.002,depth=1,fault=lambda p:p in ('r','r1'))
    assert not ok and same(before,a.state) and a.trace is trace and any(r['path']=='r0' and r['status']=='accepted' for r in rows)
    assert cache.keys()==m.gate.cache.keys() and all(np.array_equal(q,m.gate.cache[k]) for k,q in cache.items())
    ok,rows,err=a.advance(.002,depth=3,fault=lambda p:p=='r')
    print('recovery',ok,err,[r['status'] for r in rows],flush=True)
    assert ok and a.state.time==before.time+Q(.002)
    assert m._planes is None and m.models.peak<=1
    return dict(duration_reuse=True,late_rollback=True,recovery=True,stats=m.models.stats())
if __name__=='__main__':print(json.dumps(run(),indent=2))
