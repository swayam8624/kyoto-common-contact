"""Reuse a topology-fixed elastic energy function and avoid dense KKT Jacobians.
Contact identities remain statically specialized to preserve local sparsity.
"""
import numpy as np
import casadi as ca
from pass45_transaction import RejectedTrial
from pass56_parametric import ParametricModel

class SparseModel(ParametricModel):
    def __init__(self,x,t,v,capacity=12):
        super().__init__(x,t,v,capacity)
        # Shared material graph, compiled once per mesh, not per contact key.
        trial=ca.MX.sym('trial',3,self.n);energy=0;js=[];mu=1000/(2*1.32);lam=1000*.32/(1.32*(1-2*.32))
        for i,c in enumerate(self.t):
            F=ca.horzcat(*[trial[:,int(j)]-trial[:,int(c[0])] for j in c[1:]])@ca.DM(self.inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));js.append(J);log=ca.log(J);energy+=self.vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
        self.elastic=ca.Function('elastic',[trial],[energy,ca.vertcat(*js)])
    def build(self,keys):
        if keys in self.cache:return self.cache[keys]
        n=self.n;dt=ca.MX.sym('duration');w=ca.MX.sym('w',3,n);state=ca.MX.sym('state',3,n);vel=ca.MX.sym('vel',3,n);trial=state+dt*w;energy,jvec=self.elastic(trial);js=[jvec[i] for i in range(len(self.t))]
        obj=.5*ca.sum2(ca.sum1((w-vel)**2)*ca.DM(self.mass.reshape(1,-1)))+energy;z=[ca.vec(w)];p=[ca.vec(state),ca.vec(vel)];cons=[];lo=[];hi=[];force=[]
        for k,key in enumerate(keys):
            s,l,r=self.pairs[key];q=ca.MX.sym('q'+str(k),3);prior=ca.MX.sym('prior'+str(k),3);z.append(q);p.append(prior);obj+=1e-8*ca.sumsqr(q-prior)
            if not s:
                plane=ca.MX.sym('plane'+str(k));z.append(plane);expr=[plane-ca.dot(q,trial[:,i])/dt for i in l]+[ca.dot(q,trial[:,i])/dt-plane for i in r]
            else:expr=[-ca.dot(q,trial[:,i]-trial[:,s[0]])/dt for i in l]+[ca.dot(q,trial[:,i]-trial[:,s[0]])/dt for i in r]
            force.extend(range(len(cons),len(cons)+len(expr)));cons.extend(expr);lo.extend([0.]*len(expr));hi.extend([np.inf]*len(expr))
            if len(s)==2:cons.append(ca.dot(q,trial[:,s[1]]-trial[:,s[0]])/dt);lo.append(0.);hi.append(0.)
            cons.append(ca.sumsqr(q));lo.append(1.);hi.append(1.)
        count=len(cons);cons.extend(js);lo.extend([.1]*len(js));hi.extend([np.inf]*len(js));z=ca.vertcat(*z);p=ca.vertcat(*p,dt);g=ca.vertcat(*cons)
        solver=ca.nlpsol('step','ipopt',{'x':z,'p':p,'f':obj,'g':g},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.bound_relax_factor':0.,'ipopt.max_iter':200});dual=ca.MX.sym('dual',g.numel());grad=ca.Function('grad',[z,p,dual],[ca.gradient(obj,z)+ca.jtimes(g,z,dual,True)]);self.cache[keys]=(solver,grad,np.array(lo),np.array(hi),count,force);return self.cache[keys]
    def trial(self,x,v,keys):
        solver,grad,lo,hi,count,force=self.build(keys);guess=list(v.ravel());par=list(x.ravel())+list(v.ravel());lo=lo.copy();lo[force]=self.margin/self.dt
        for k in keys:
            q,p=self.plane(x,k);guess.extend(q);par.extend(q)
            if p is not None:guess.append(p/self.dt)
        par.append(self.dt);sol=solver(x0=guess,p=par,lbg=lo,ubg=hi)
        if not solver.stats()['success']:raise RejectedTrial('solver:'+solver.stats()['return_status'])
        zz=np.asarray(sol['x']).ravel();vn=zz[:3*self.n].reshape(self.n,3);y=x+self.dt*vn;val=np.asarray(sol['g']).ravel();mul=np.asarray(sol['lam_g']).ravel();res=grad(zz,par,mul);kkt=float(np.max(np.abs(np.asarray(res).ravel())));j=float(min(val[count:]));err=float(np.linalg.norm((self.mass[:,None]*vn).sum(axis=0)-self.mom));violation=max(float(np.max(lo-val)),float(np.max(val-hi)),0.)
        if not (np.isfinite(y).all() and j>.1 and kkt<1e-7 and err<1e-6 and violation<1e-7):raise RejectedTrial('physical residual')
        return y,vn,dict(j=j,kkt=kkt,momentum=err,violation=violation,impulse=float(-sum(mul[force])))
