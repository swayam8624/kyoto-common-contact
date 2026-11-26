"""Different contact identities with identical shared-simplex signatures.
Compare independently compiled reference solves and validate exact paths.
"""
import json
from fractions import Fraction as Q
import numpy as np
from pass54_adaptive_discovery import geometry,AdaptiveDiscovery,same
from pass56_parametric import ParametricModel
from pass57_identity import IdentityModel
from pass50_mesh_gate import MeshGate

def run():
    x,t,v=geometry('replica2',True);a=ParametricModel(x,t,v,12);b=IdentityModel(x,t,v,12)
    keys=[((0,6),),((8,14),),((0,6),),((8,14),)]
    assert all(b.signature(k)==(1,) for k in keys)
    rows=[]
    for i,k in enumerate(keys):
        dt=(.001,.002,.002,.001)[i];a.dt=b.dt=dt;a.cache=a.models.view(None);b.cache=b.models.view(None)
        y,w,r=a.trial(x,v,k);z,u,s=b.trial(x,v,k)
        e=float(np.max(np.abs(y-z)));f=float(np.max(np.abs(w-u)));assert e<1e-10 and f<1e-8
        assert not MeshGate(t).check(x,y)['unresolved'] and not MeshGate(t).check(x,z)['unresolved']
        rows.append(dict(keys=k,dt=dt,position_error=e,velocity_error=f,reference_models=a.models.stats(),identity_models=b.models.stats()))
    assert a.models.misses==2 and b.models.misses==1 and b.models.hits==3
    x,t,v=geometry('vertex',True);m=IdentityModel(x,t,v,1);sim=AdaptiveDiscovery(m,x,v)
    assert sim.advance(.002)[0];before=sim.state;trace=sim.trace;cache=m.gate.cache.copy()
    ok,r,e=sim.advance(.002,depth=1,fault=lambda p:p in ('r','r1'))
    assert not ok and same(before,sim.state) and sim.trace is trace and any(i['path']=='r0' and i['status']=='accepted' for i in r)
    assert cache.keys()==m.gate.cache.keys() and all(np.array_equal(q,m.gate.cache[k]) for k,q in cache.items())
    assert sim.advance(.002,depth=3,fault=lambda p:p=='r')[0] and sim.state.time==before.time+Q(.002) and m.models.peak==1
    return dict(identity_switches=rows,rollback=True,recovery=True,one_entry_cache=m.models.stats())
if __name__=='__main__':print(json.dumps(run(),indent=2))
