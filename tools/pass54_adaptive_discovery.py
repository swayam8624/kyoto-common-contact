"""Adaptive discovery with exact elapsed time and atomic macrostep publication.
Rejected trials restore their witness cache. Compiled NLPs are keyed by duration.
"""
from dataclasses import dataclass
from fractions import Fraction as Q
from pathlib import Path
import json
import numpy as np
from pass53_discovery import Model,fixture
from pass42_physical_replay import mesh
from pass45_transaction import RejectedTrial

class DurationModel(Model):
    def __init__(self,x,t,v):
        super().__init__(x,t,v);self.caches={}
    def solve(self,x,v,duration,budget):
        self.dt=float(duration);self.cache=self.caches.setdefault(duration,{})
        return self.step(x,v,True,budget=budget)

@dataclass(frozen=True)
class State:
    x:np.ndarray
    v:np.ndarray
    time:Q=Q(0)
    version:int=0

class Exhausted(RuntimeError):pass

class AdaptiveDiscovery:
    def __init__(self,m,x,v):
        self.model=m;self.state=State(x.copy(),v.copy());self.trace=[]
    def advance(self,dt,depth=3,budget=8,max_move=float('inf'),fault=None):
        if not np.isfinite(dt) or dt<=0 or type(depth)!=int or not 0<=depth<=12 or type(budget)!=int or budget<0 or not max_move>0:raise ValueError('invalid policy')
        d=Q(float(dt));start=self.state;leaves=[];attempts=[];cache=self.model.gate.cache.copy()
        if Q(float(d/2**depth))!=d/2**depth:raise ValueError('unrepresentable duration')
        def rec(s,h,k,path):
            old=self.model.gate.cache.copy()
            try:
                y,v,r=self.model.solve(s.x,s.v,h,budget)
                if not r['certified'] or not np.isfinite(y).all() or not np.isfinite(v).all() or not np.array_equal(y,s.x+float(h)*v):raise RejectedTrial('invalid accepted leaf')
                if np.max(np.linalg.norm(y-s.x,axis=1))>max_move:raise RejectedTrial('displacement budget')
                if fault and fault(path):raise RejectedTrial('injected rejection')
                row=dict(path=path,dt=str(h),status='accepted',**r);attempts.append(row)
                z=State(y,v,s.time+h,s.version+1);leaves.append((z,row));return z
            except RejectedTrial as e:
                self.model.gate.cache=old;attempts.append(dict(path=path,dt=str(h),status=str(e)))
                if k==depth:raise Exhausted(str(e)) from e
                return rec(rec(s,h/2,k+1,path+'0'),h/2,k+1,path+'1')
        try:end=rec(start,d,0,'r')
        except Exhausted as e:
            self.model.gate.cache=cache;return False,attempts,str(e)
        except BaseException:
            self.model.gate.cache=cache;raise
        assert end.time-start.time==d and sum((Q(r['dt']) for _,r in leaves),Q(0))==d
        self.state=end;self.trace=[*self.trace,*leaves];return True,attempts,''

def same(a,b):return a.time==b.time and a.version==b.version and np.array_equal(a.x,b.x) and np.array_equal(a.v,b.v)

def geometry(kind,rot=False):
    if kind in ('edge','vertex','disjoint'):return fixture(kind,rot)
    if kind.startswith('replica'):
        n=int(kind[7:]);x,t,v=fixture('vertex',rot);return np.vstack([x+[i*.25,0,0] for i in range(n)]),np.vstack([t+i*len(x) for i in range(n)]),np.tile(v,(n,1))
    if kind in ('u78','u156','comb120'):
        width=5 if kind=='comb120' else 3;layers=2 if kind=='u156' else 1;h=.02
        x,t=mesh([(i,j,k) for i in range(width) for j in range(6) for k in range(layers) if i%2==0 or j==0],h,np.zeros(3));v=np.zeros_like(x);v[:,0]=np.clip((width*h/2-x[:,0])/(.025 if width==5 else .01),-1,1)*np.clip((x[:,1]-h)/(5*h),0,1)*1.5
        if rot:
            R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]);x=x@R.T;v=v@R.T
        return x,t,v
    raise ValueError(kind)

def run_case(out,kind,rot,dt,macros,cap,depth=3,controls=True):
    out.mkdir(exist_ok=False);x,t,v=geometry(kind,rot);m=DurationModel(x,t,v);a=AdaptiveDiscovery(m,x,v);attempts=[];error='';done=0
    for i in range(macros):
        ok,rows,error=a.advance(dt,depth=depth,max_move=cap);attempts.append(dict(macro=i,ok=ok,attempts=rows,error=error));print(kind,rot,i,ok,[r['status'] for r in rows],flush=True)
        if not ok:break
        done+=1
    trace=list(a.trace);reference=AdaptiveDiscovery(DurationModel(x,t,v),x,v);parity=True
    for expected,row in trace:
        ok,_,err=reference.advance(float(Q(row['dt'])),depth=0);parity=parity and ok and same(expected,reference.state)
        if not ok:break
    assert a.state.time==done*Q(dt)
    rollback=None;recovery=None
    if controls and done==macros:
        before=State(a.state.x.copy(),a.state.v.copy(),a.state.time,a.state.version);old_trace=a.trace;cache=m.gate.cache.copy()
        ok,r,err=a.advance(dt,depth=1,fault=lambda p:p in ('r','r1'));rollback=not ok and any(i['path']=='r0' and i['status']=='accepted' for i in r) and same(before,a.state) and a.trace is old_trace and cache.keys()==m.gate.cache.keys() and all(np.array_equal(q,m.gate.cache[k]) for k,q in cache.items());assert rollback
        (out/'rollback.json').write_text(json.dumps(r,indent=2)+'\n')
        ok,r,err=a.advance(dt,depth=1,fault=lambda p:p=='r');recovery=ok and a.state.time==before.time+Q(dt);assert recovery
        (out/'recovery.json').write_text(json.dumps(r,indent=2)+'\n')
    np.savez_compressed(out/'trajectory.npz',states=np.array([x,*[s.x for s,_ in a.trace]]),velocities=np.array([v,*[s.v for s,_ in a.trace]]),cells=t)
    (out/'attempts.json').write_text(json.dumps(attempts,indent=2)+'\n');(out/'leaves.json').write_text(json.dumps([r for _,r in a.trace],indent=2)+'\n')
    result=dict(kind=kind,rot=rot,cells=len(t),nodes=len(x),macros_requested=macros,macros_completed=done,dt=dt,main_leaves=len(trace),total_committed_leaves=len(a.trace),main_time=str(done*Q(dt)),pair_intervals=sum(r['pair_checks'] for _,r in a.trace),active_leaves=sum(r['impulse']>1e-7 for _,r in a.trace),discovery_retries=sum(len(r['attempts'])-1 for _,r in a.trace),max_discovered_pairs=max((len(r['attempts'][-1]['keys']) for _,r in a.trace),default=0),rejected_macro_trials=sum(r['status']!='accepted' for row in attempts for r in row['attempts']),reference_parity=parity,rollback=rollback,recovery=recovery,error=error)
    (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(result,flush=True);return result
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('kind');p.add_argument('--rot',action='store_true');p.add_argument('--dt',type=float,default=.004);p.add_argument('--macros',type=int,default=5);p.add_argument('--cap',type=float,default=.0032);p.add_argument('--depth',type=int,default=3);p.add_argument('--no-controls',action='store_true');a=p.parse_args();run_case(a.out,a.kind,a.rot,a.dt,a.macros,a.cap,a.depth,not a.no_controls)
