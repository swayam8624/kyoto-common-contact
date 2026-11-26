"""Adaptive implicit FEM: recompute actual smaller physical solves, stage all leaves,
reject unsafe paths, publish only complete macrosteps. Disjoint region scope."""
import argparse,json
from dataclasses import dataclass
from fractions import Fraction as Q
from itertools import combinations
from pathlib import Path
import numpy as np
import casadi as ca
from pass42_physical_replay import mesh,regions
from pass42_region_certificate import axis_witness
from pass45_transaction import RejectedTrial
from pass48_integer_region import IntegerRegionGate

@dataclass(frozen=True)
class State:
    x: np.ndarray
    v: np.ndarray
    normals: np.ndarray
    clock: Q=Q(0)
    version: int=0

class Model:
    def __init__(self,sid):
        rot=sid==2;enabled=True;static=False;gap=.0008
        h=.02
        # Thick U and three-finger comb, both connected volumetric meshes.
        comb=sid>=2; width=5 if comb else 3; thickness=1 if comb else 2
        x,t=mesh([(i,j,k) for i in range(width) for j in range(6) for k in range(thickness) if i%2==0 or j==0],h,np.zeros(3));v=np.zeros_like(x)
        center=width*h/2
        v[:,0]=np.clip((center-x[:,0])/(.01 if not comb else .025),-1.,1.)*np.clip((x[:,1]-h)/(5*h),0,1)*1.5
        if static:v[:]=0
        R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if rot else np.eye(3);x=x@R.T;v=v@R.T;n=len(x);g=regions(x,t)
        pairs=[(a,b) for a,b in combinations(g,2) if not set(a['nodes'])&set(b['nodes'])];assert pairs,g
        group=[(a['nodes'],b['nodes']) for a,b in pairs];m=len(group);normals=np.array([axis_witness(x[a],x[b]) for a,b in group])
        print('START',sid,len(t),len(g),m,flush=True)
        dm=(x[t[:,1:]]-x[t[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;inv=np.linalg.inv(dm);mass=np.zeros(n);np.add.at(mass,t.ravel(),np.repeat(vol*480/4,4));mom=(mass[:,None]*v).sum(axis=0)
        dt=ca.MX.sym('dt');w=ca.MX.sym('w',3,n);state=ca.MX.sym('state',3,n);vel=ca.MX.sym('vel',3,n);trial=state+dt*w;energy=0;js=[];mu=2500/(2*1.32);lam=2500*.32/(1.32*(1-2*.32))
        for i,c in enumerate(t):
            F=ca.horzcat(*[trial[:,int(j)]-trial[:,int(c[0])] for j in c[1:]])@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));js.append(J);log=ca.log(J);energy+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
        obj=.5*ca.sum2(ca.sum1((w-vel)**2)*ca.DM(mass.reshape(1,-1)))+energy;z=ca.vec(w);p=ca.vertcat(ca.vec(state),ca.vec(vel));ineq=[];count=0
        if enabled:
            planes=ca.MX.sym('planes',m);axes=ca.MX.sym('axes',3,m);prior=ca.MX.sym('prior',3,m);z=ca.vertcat(z,planes,ca.vec(axes));p=ca.vertcat(p,ca.vec(prior));obj+=1e-7*ca.sumsqr(axes-prior)
            for k,(a,b) in enumerate(group):
                proj=axes[:,k].T@trial/dt;ineq.extend([ca.vec(planes[k]-proj[:,a]-gap/(2*dt)),ca.vec(proj[:,b]-planes[k]-gap/(2*dt))])
            count=sum(len(a)+len(b) for a,b in group)
        p=ca.vertcat(p,dt);cons=ca.vertcat(*ineq,*js,*([ca.sumsqr(axes[:,k]) for k in range(m)] if enabled else []));lower=np.r_[np.zeros(count),np.full(len(t),.1),np.ones(m if enabled else 0)];upper=np.r_[np.full(count+len(t),np.inf),np.ones(m if enabled else 0)]
        solver=ca.nlpsol('step','ipopt',{'x':z,'p':p,'f':obj,'g':cons},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':200,'ipopt.bound_relax_factor':0.});grad=ca.Function('grad',[z,p],[ca.gradient(obj,z),ca.jacobian(cons,z)])

        self.t=t;self.initial=State(x,v,normals);self.gate=IntegerRegionGate(x,v,t,g)
        def integrate(s,duration):
            x,v,normals=s.x,s.v,s.normals;dt=float(duration);step=s.version
            par=np.r_[x.ravel(),v.ravel()];guess=v.ravel()
            if enabled:
                par=np.r_[par,normals.ravel()];offset=[((x[a]@normals[k]).max()+(x[b]@normals[k]).min())/(2*dt) for k,(a,b) in enumerate(group)];guess=np.r_[guess,offset,normals.ravel()]
            par=np.r_[par,dt];sol=solver(x0=guess,p=par,lbg=lower,ubg=upper);
            if not solver.stats()['success']:raise RejectedTrial('solver:'+solver.stats()['return_status'])
            zz=np.asarray(sol['x']).ravel();vn=zz[:3*n].reshape(n,3);y=x+dt*vn;val=np.asarray(sol['g']).ravel();mul=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,par);kkt=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mul)));j=float(min(val[count:count+len(t)]));err=float(np.linalg.norm((mass[:,None]*vn).sum(axis=0)-mom));imp=0.
            if enabled:
                normals=zz[-3*m:].reshape(m,3);off=0
                for a,b in group:
                    ia=-sum(mul[off:off+len(a)]);off+=len(a);ib=-sum(mul[off:off+len(b)]);off+=len(b);
                    if not abs(ia-ib)<1e-7:raise RejectedTrial('impulse imbalance')
                    imp+=ia

            clear=min(float((y[b]@normals[k]).min()-(y[a]@normals[k]).max()) for k,(a,b) in enumerate(group))
            if not (np.isfinite(y).all() and np.isfinite(vn).all() and j>.1 and err<1e-6 and kkt<1e-7 and clear>=gap-1e-10 and np.max(np.abs(np.linalg.norm(normals,axis=1)-1))<1e-8):raise RejectedTrial('physical residual')
            return y,vn,normals,dict(min_j=j,clearance=clear,momentum=err,kkt=kkt,impulse=float(imp),displacement=float(np.max(np.linalg.norm(y-x,axis=1))))
        self.solve=integrate

class Exhausted(RuntimeError):pass

class Adaptive:
    def __init__(self,model):self.model=model;self.state=model.initial;self.trace=[]
    def advance(self,dt,max_depth=4,max_move=float('inf'),fault=None):
        if not np.isfinite(dt) or dt<=0 or not isinstance(max_depth,int) or not 0<=max_depth<=12 or not max_move>0:raise ValueError('invalid budget')
        duration=Q(float(dt));start=self.state;attempts=[];leaves=[]
        if Q(float(duration/(2**max_depth)))!=duration/(2**max_depth):raise ValueError('unrepresentable leaf duration')
        def recurse(s,d,depth,path):
            try:
                y,v,n,diag=self.model.solve(s,d)
                if fault is not None:y=fault(path,s,y)
                self.model.gate._check(s.x,y,n)
                if diag['displacement']>max_move:raise RejectedTrial('displacement budget')
                # Ensure the accepted trajectory really is the solver's drift.
                if not np.array_equal(y,s.x+float(d)*v):raise RejectedTrial('inconsistent drift')
                attempts.append(dict(path=path,dt=str(d),result='accepted',**diag))
                out=State(y,v,n,s.clock+d,s.version+1);leaves.append((out,dict(path=path,dt=str(d),**diag)));return out
            except RejectedTrial as e:
                attempts.append(dict(path=path,dt=str(d),result=str(e)))
                if depth==max_depth:raise Exhausted(str(e)) from e
                return recurse(recurse(s,d/2,depth+1,path+'0'),d/2,depth+1,path+'1')
        try:end=recurse(start,duration,0,'r')
        except Exhausted as e:return False,attempts,str(e)
        assert end.clock-start.clock==duration and sum((Q(r['dt']) for _,r in leaves),Q(0))==duration
        self.state=end;self.trace=[*self.trace,*leaves];return True,attempts,''

def same(a,b):return a.clock==b.clock and a.version==b.version and all(np.array_equal(getattr(a,k),getattr(b,k)) for k in ('x','v','normals'))

def run(out):
    out.mkdir(exist_ok=False);report=[]
    for scene in (0,2):
        model=Model(scene);a=Adaptive(model);baseline=Adaptive(model);dt=.004
        for macro in range(4):
            ok,attempts,error=a.advance(dt,max_move=.0032);assert ok,error
            report.append(dict(scene=scene,macro=macro,mode='adaptive',attempts=attempts));print(scene,macro,[r['result'] for r in attempts],flush=True)
        assert any(r['result']=='displacement budget' for row in report if row['scene']==scene for r in row['attempts'])
        for expected,r in a.trace:
            ok,_,error=baseline.advance(float(Q(r['dt'])),max_depth=0);assert ok,error
            assert same(expected,baseline.state)
        assert same(a.state,baseline.state) and a.state.clock==4*Q(dt)
        np.savez_compressed(out/f'scene{scene}.npz',states=np.array([model.initial.x,*[s.x for s,_ in a.trace]]),cells=model.t)
        def collapse(path,s,y):
            if path=='r':
                y=y.copy();y[model.t[0,1]]=y[model.t[0,0]]
            return y
        old=a.state;ok,attempts,error=a.advance(.002,fault=collapse);assert ok,error
        assert attempts[0]['result']=='unresolved continuous orientation' and a.state.clock-old.clock==Q(.002)
        report.append(dict(scene=scene,mode='injected_recovery',attempts=attempts))
        before=State(a.state.x.copy(),a.state.v.copy(),a.state.normals.copy(),a.state.clock,a.state.version);trace=a.trace
        trace_copy=[(State(s.x.copy(),s.v.copy(),s.normals.copy(),s.clock,s.version),r.copy()) for s,r in trace]
        def late(path,s,y):
            if path in ('r','r1'):
                y=y.copy();y[model.t[0,1]]=y[model.t[0,0]]
            return y
        ok,attempts,error=a.advance(.002,max_depth=1,fault=late);assert not ok and any(r['path']=='r0' and r['result']=='accepted' for r in attempts);assert same(a.state,before) and a.trace is trace and len(a.trace)==len(trace_copy)
        assert all(same(s,t) and r==q for (s,r),(t,q) in zip(a.trace,trace_copy))
        np.savez_compressed(out/f'committed{scene}.npz',states=np.array([model.initial.x,*[s.x for s,_ in a.trace]]),velocities=np.array([model.initial.v,*[s.v for s,_ in a.trace]]),normals=np.array([model.initial.normals,*[s.normals for s,_ in a.trace]]),cells=model.t)
        (out/f'clock{scene}.json').write_text(json.dumps({'time':str(a.state.clock),'version':a.state.version,'leaf_dt':[r['dt'] for _,r in a.trace],'reference_parity':'bitwise at every scheduled leaf','late_rollback':True},indent=2)+'\n')
        report.append(dict(scene=scene,mode='late_rollback',attempts=attempts));print('PASS recovery and late rollback',scene,flush=True)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);run(p.parse_args().out)
