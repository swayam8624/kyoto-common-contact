"""Fresh counterbalanced adaptive comparison with strict state and receipt parity."""
import argparse,csv,json,math,time
from pathlib import Path
from fractions import Fraction as Q
import numpy as np
from pass54_adaptive_discovery import DurationModel,AdaptiveDiscovery,geometry,same
from pass55_bounded import BoundedDurationModel,BoundedModels

def controls():
    c=BoundedModels(2);a=c.view(Q(1,4));b=c.view(Q(1,8));k=((0,1),)
    a[k]='a';b[k]='b';assert a[k]=='a';a[()]='empty';assert k not in b and a[()]=='empty'
    b[k]='rebuilt';assert k not in a and b[k]=='rebuilt' and c.peak==2 and c.evictions==2
    for invalid in (0,-1,True,1.5):
        try:BoundedModels(invalid)
        except ValueError:pass
        else:raise AssertionError('invalid capacity')
    x,t,v=geometry('vertex',True);m=BoundedDurationModel(x,t,v,1);a=AdaptiveDiscovery(m,x,v)
    assert a.advance(.002)[0];before=a.state;trace=a.trace;cache=m.gate.cache.copy()
    ok,rows,err=a.advance(.002,depth=1,fault=lambda p:p in ('r','r1'))
    assert not ok and a.state is before and a.trace is trace and any(r['path']=='r0' and r['status']=='accepted' for r in rows)
    assert m._planes is None and m.models.peak==1 and cache.keys()==m.gate.cache.keys() and all(np.array_equal(q,m.gate.cache[k]) for k,q in cache.items())
    assert a.advance(.002,depth=1,fault=lambda p:p=='r')[0] and a.state.time==before.time+Q(.002)
    return dict(lru=True,invalid_capacities=4,late_rollback=True,recovery=True,stats=m.models.stats())

def run(out,kinds,blocks,capacity):
    out.mkdir(exist_ok=False);ctrl=controls();print('CONTROLS',ctrl,flush=True);rows=[];results=[]
    for kind in kinds:
        x,t,v=geometry(kind,kind in ('vertex','comb120'));macros=5 if kind in ('vertex','replica8') else 4;ref=None;refattempts=None
        for block in range(blocks):
            for slot,arm in enumerate(('base','bounded') if block%2==0 else ('bounded','base')):
                start=time.perf_counter_ns();m=DurationModel(x,t,v) if arm=='base' else BoundedDurationModel(x,t,v,capacity);a=AdaptiveDiscovery(m,x,v);attempts=[]
                for _ in range(macros):
                    ok,r,err=a.advance(.004,max_move=.0032);assert ok,(kind,arm,err);attempts.append(r)
                ns=time.perf_counter_ns()-start;assert a.state.time==macros*Q(.004)
                if ref is None:ref=list(a.trace);refattempts=attempts
                assert len(ref)==len(a.trace) and all(same(s,z) and r==q for (s,r),(z,q) in zip(ref,a.trace)) and attempts==refattempts,(kind,arm,'parity')
                stats=m.models.stats() if arm=='bounded' else dict(capacity=None,resident=sum(map(len,m.caches.values())),peak=sum(map(len,m.caches.values())),hits=None,misses=None,evictions=None)
                row=dict(kind=kind,cells=len(t),block=block,slot=slot,arm=arm,ns=ns,leaves=len(a.trace),pairs=sum(r['pair_checks'] for _,r in a.trace),**stats,plane_hits=getattr(m,'plane_hits',None),plane_misses=getattr(m,'plane_misses',None));rows.append(row);print(row,flush=True)
                with (out/'timing.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
                np.savez_compressed(out/f'{kind}_{arm}_{block}.npz',states=[x,*[s.x for s,_ in a.trace]],velocities=[v,*[s.v for s,_ in a.trace]],cells=t)
                (out/f'{kind}_{arm}_{block}.json').write_text(json.dumps(attempts,indent=2)+'\n')
                del m,a
        ratios=[]
        for block in range(blocks):
            times={r['arm']:r['ns'] for r in rows if r['kind']==kind and r['block']==block};ratios.append(times['base']/times['bounded'])
        results.append(dict(kind=kind,ratio=math.exp(sum(map(math.log,ratios))/len(ratios)),ratios=ratios,bitwise_state_and_receipt_parity=True))
        (out/'summary.json').write_text(json.dumps(dict(controls=ctrl,scenes=results,scope='fresh model construction + adaptive solves; same geometry gate; entry bound not RSS'),indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('--kinds',nargs='+',default=['vertex','replica8','u78','comb120','u156']);p.add_argument('--blocks',type=int,default=2);p.add_argument('--capacity',type=int,default=12);a=p.parse_args();run(a.out,a.kinds,a.blocks,a.capacity)
