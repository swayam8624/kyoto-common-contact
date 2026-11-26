"""Discover pair constraints from uncertified provisional trajectories; retry from unchanged state."""
import json,sys
from pathlib import Path
import numpy as np
import casadi as ca
from pass50_mesh_gate import MeshGate,propose,sat
from pass50_edge_response import overlap
from pass45_transaction import RejectedTrial

def fixture(kind,rot):
    a=np.deg2rad(np.arange(5)*85.);r=.04;ring=np.column_stack((r*np.cos(a),r*np.sin(a),np.zeros(5)))
    if kind=='edge':
        x=np.vstack(([[0,0,0],[0,0,.04]],ring));t=np.array([[0,1,i,i+1] for i in range(2,6)]);v=np.zeros_like(x);v[2]=[0,-1.5,0];v[6]=1.5*np.array([-np.sin(a[-1]),np.cos(a[-1]),0])
    elif kind=='vertex':
        x=np.vstack(([[0,0,0]],ring+[0,0,-.02],ring+[0,0,.02]));t=np.array([q for i in range(4) for q in ([0,1+i,2+i,7+i],[0,1+i,7+i,6+i])]);v=np.zeros_like(x);v[[1,6]]=[0,-1.5,0];v[[5,10]]=1.5*np.array([-np.sin(a[-1]),np.cos(a[-1]),0])
    else:
        a=np.vstack((np.zeros(3),np.eye(3)))*.02;x=np.vstack((a,a+[.035,0,0]));t=np.array([[0,1,2,3],[4,5,6,7]]);v=np.zeros_like(x);v[:4,0]=.6;v[4:,0]=-.6
    for c in t:
        if np.linalg.det((x[c[1:]]-x[c[0]]).T)<0:c[2],c[3]=c[3],c[2]
    if rot:
        R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]);x=x@R.T;v=v@R.T
    return x,t,v

class Model:
    def __init__(self,x,t,v):
        self.t=t;self.dt=.0005;self.margin=.0002;self.cache={};self.gate=MeshGate(t);self.pairs={(i,j):(s,l,r) for i,j,s,l,r in self.gate.pairs};self.n=len(x)
        dm=(x[t[:,1:]]-x[t[:,:1]]).transpose(0,2,1);self.vol=np.linalg.det(dm)/6;assert min(self.vol)>0;self.inv=np.linalg.inv(dm);self.mass=np.zeros(len(x));np.add.at(self.mass,t.ravel(),np.repeat(self.vol*480/4,4));self.mom=(self.mass[:,None]*v).sum(axis=0);assert not self.gate.check(x,x)['unresolved']
    def plane(self,x,key):
        s,l,r=self.pairs[key]
        if not s:q=sat(x[l],x[r]);p=((x[l]@q).max()+(x[r]@q).min())/2
        else:
            aux=propose(x,s,l,r)
            if aux is None:raise RejectedTrial('no starting plane')
            q=np.cross(x[s[1]]-x[s[0]],aux) if len(s)==2 else aux;p=None
        return q/np.linalg.norm(q),p
    def build(self,keys):
        if keys in self.cache:return self.cache[keys]
        n=self.n;dt=self.dt;w=ca.MX.sym('w',3,n);state=ca.MX.sym('state',3,n);vel=ca.MX.sym('vel',3,n);trial=state+dt*w;energy=0;js=[];mu=1000/(2*1.32);lam=1000*.32/(1.32*(1-2*.32))
        for i,c in enumerate(self.t):
            F=ca.horzcat(*[trial[:,int(j)]-trial[:,int(c[0])] for j in c[1:]])@ca.DM(self.inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));js.append(J);log=ca.log(J);energy+=self.vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
        obj=.5*ca.sum2(ca.sum1((w-vel)**2)*ca.DM(self.mass.reshape(1,-1)))+energy;z=[ca.vec(w)];p=[ca.vec(state),ca.vec(vel)];cons=[];lo=[];hi=[];force=[]
        for k,key in enumerate(keys):
            s,l,r=self.pairs[key];q=ca.MX.sym('q'+str(k),3);prior=ca.MX.sym('prior'+str(k),3);z.append(q);p.append(prior);obj+=1e-8*ca.sumsqr(q-prior)
            if not s:
                plane=ca.MX.sym('plane'+str(k));z.append(plane);expr=[plane-ca.dot(q,trial[:,i])/dt for i in l]+[ca.dot(q,trial[:,i])/dt-plane for i in r]
            else:expr=[-ca.dot(q,trial[:,i]-trial[:,s[0]])/dt for i in l]+[ca.dot(q,trial[:,i]-trial[:,s[0]])/dt for i in r]
            force.extend(range(len(cons),len(cons)+len(expr)));cons.extend(expr);lo.extend([self.margin/dt]*len(expr));hi.extend([np.inf]*len(expr))
            if len(s)==2:cons.append(ca.dot(q,trial[:,s[1]]-trial[:,s[0]])/dt);lo.append(0.);hi.append(0.)
            cons.append(ca.sumsqr(q));lo.append(1.);hi.append(1.)
        count=len(cons);cons.extend(js);lo.extend([.1]*len(js));hi.extend([np.inf]*len(js));z=ca.vertcat(*z);p=ca.vertcat(*p);g=ca.vertcat(*cons)
        solver=ca.nlpsol('step','ipopt',{'x':z,'p':p,'f':obj,'g':g},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.bound_relax_factor':0.,'ipopt.max_iter':200});grad=ca.Function('grad',[z,p],[ca.gradient(obj,z),ca.jacobian(g,z)]);self.cache[keys]=(solver,grad,np.array(lo),np.array(hi),count,force);return self.cache[keys]
    def trial(self,x,v,keys):
        solver,grad,lo,hi,count,force=self.build(keys);guess=list(v.ravel());par=list(x.ravel())+list(v.ravel())
        for k in keys:
            q,p=self.plane(x,k);guess.extend(q);par.extend(q)
            if p is not None:guess.append(p/self.dt)
        sol=solver(x0=guess,p=par,lbg=lo,ubg=hi)
        if not solver.stats()['success']:raise RejectedTrial('solver:'+solver.stats()['return_status'])
        zz=np.asarray(sol['x']).ravel();vn=zz[:3*self.n].reshape(self.n,3);y=x+self.dt*vn;val=np.asarray(sol['g']).ravel();mul=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,par);kkt=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mul)));j=float(min(val[count:]));err=float(np.linalg.norm((self.mass[:,None]*vn).sum(axis=0)-self.mom));violation=max(float(np.max(lo-val)),float(np.max(val-hi)),0.)
        if not (np.isfinite(y).all() and j>.1 and kkt<1e-7 and err<1e-6 and violation<1e-7):raise RejectedTrial('physical residual')
        return y,vn,dict(j=j,kkt=kkt,momentum=err,violation=violation,impulse=float(-sum(mul[force])))
    def step(self,x,v,enabled,budget=8):
        keys=();attempts=[]
        for _ in range(budget+1):
            y,vn,diag=self.trial(x,v,keys);audit=self.gate.check(x,y);bad=[tuple(z[:2]) for z in audit['unresolved']];attempts.append(dict(keys=[list(k) for k in keys],unresolved=audit['unresolved'],**diag))
            if not enabled or not bad:return y,vn,dict(attempts=attempts,certified=not bad,pair_checks=len(self.pairs),**diag)
            fresh=set(bad)-set(keys)
            if not fresh or any(len(self.pairs[k][0])>=3 for k in fresh):raise RejectedTrial('unresolved after discovery')
            keys=tuple(sorted(set(keys)|fresh))
        raise RejectedTrial('discovery budget')

def run(out):
    out.mkdir(exist_ok=False);summary=[]
    for kind in ('edge','vertex','disjoint'):
        for rot in (False,True):
            for enabled in (True,False):
                x,t,v=fixture(kind,rot);m=Model(x,t,v);states=[x.copy()];rows=[];name=f'{kind}_r{int(rot)}_e{int(enabled)}'
                for step in range(40):
                    y,vn,r=m.step(x,v,enabled);wit=None
                    if not r['certified']:
                        for i,j,_ in r['attempts'][-1]['unresolved']:
                            p=overlap(y[t[i]],y[t[j]])
                            if p is not None:wit=[i,j,p];break
                    r.update(step=step,overlap=wit);rows.append(r);states.append(y.copy());x,v=y,vn
                found=sorted({(i,j,k) for r in rows for a in r['attempts'][:-1] for i,j,k in a['unresolved']});result=dict(case=name,cells=len(t),steps=len(rows),pair_checks=sum(r['pair_checks'] for r in rows),certified_steps=sum(r['certified'] for r in rows),overlap_states=sum(r['overlap'] is not None for r in rows),active_steps=sum(r['impulse']>1e-7 for r in rows),rejected_trials=sum(len(r['attempts'])-1 for r in rows),discovered=found,min_j=min(r['j'] for r in rows),max_kkt=max(r['kkt'] for r in rows),max_momentum=max(r['momentum'] for r in rows));print(result,flush=True)
                (out/(name+'.json')).write_text(json.dumps(rows,indent=2)+'\n');np.savez_compressed(out/(name+'.npz'),states=states,cells=t);summary.append(result);(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
                if enabled:assert result['certified_steps']==40 and result['active_steps']>0 and found
                else:assert result['overlap_states']>0
if __name__=='__main__':run(Path(sys.argv[1]))
