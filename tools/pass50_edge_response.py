"""Connected volumetric fan: first/last tetrahedra share an edge, not a face.
A moving plane through that edge supplies explicit same-body contact forces.
The other tetrahedra retain the original elastic connectivity.
"""
import csv,json,sys
from pathlib import Path
import numpy as np
import casadi as ca
from pass50_mesh_gate import MeshGate,propose
from pass46_integer_orientation import batch_orientation
from fractions import Fraction as Q
from scipy.optimize import linprog

def interior(p,t):
    from pass50_mesh_gate import det,sub
    p=tuple(Q(float(x)) for x in p);t=[tuple(Q(float(x)) for x in v) for v in t]
    a,b,c=[sub(t[i],t[0]) for i in (1,2,3)];d=sub(p,t[0]);den=det(a,b,c)
    if not den:return False
    w=[det(d,b,c)/den,det(a,d,c)/den,det(a,b,d)/den]
    return min(*w,1-sum(w))>0

def overlap(a,b):
    eq=np.zeros((5,9));eq[:3,:4]=a.T;eq[:3,4:8]=-b.T;eq[3,:4]=eq[4,4:8]=1
    ub=np.zeros((8,9));ub[:,:8]=-np.eye(8);ub[:,8]=1
    sol=linprog([0]*8+[-1],A_ub=ub,b_ub=np.zeros(8),A_eq=eq,b_eq=[0,0,0,1,1],bounds=[(0,1)]*9,method='highs')
    if sol.success and sol.x[8]>1e-7:
        p=sol.x[:4]@a
        if interior(p,a) and interior(p,b):return p.tolist()
    return None

def simulate(enabled,rot,steps=40):
    angles=np.deg2rad(np.arange(5)*85.);radius=.04
    x=np.vstack(([[0,0,0],[0,0,.04]],np.column_stack((radius*np.cos(angles),radius*np.sin(angles),np.zeros(5)))))
    t=np.array([[0,1,i,i+1] for i in range(2,6)]);v=np.zeros_like(x);v[2]=[0,-1.5,0];v[6]=1.5*np.array([-np.sin(angles[-1]),np.cos(angles[-1]),0])
    R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if rot else np.eye(3);x=x@R.T;v=v@R.T;n=len(x);initial=x.copy();gap=.0004;dt=.0005
    dm=(x[t[:,1:]]-x[t[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;assert min(vol)>0;inv=np.linalg.inv(dm);mass=np.zeros(n);np.add.at(mass,t.ravel(),np.repeat(vol*480/4,4));mom=(mass[:,None]*v).sum(axis=0)
    left=[2,3];right=[5,6];aux=propose(x,[0,1],left,right);normal=np.cross(x[1]-x[0],aux);normal/=np.linalg.norm(normal)
    w=ca.MX.sym('w',3,n);state=ca.MX.sym('state',3,n);vel=ca.MX.sym('vel',3,n);trial=state+dt*w;energy=0;js=[];mu=1000/(2*1.32);lam=1000*.32/(1.32*(1-2*.32))
    for i,c in enumerate(t):
        F=ca.horzcat(*[trial[:,int(j)]-trial[:,int(c[0])] for j in c[1:]])@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));js.append(J);log=ca.log(J);energy+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
    obj=.5*ca.sum2(ca.sum1((w-vel)**2)*ca.DM(mass.reshape(1,-1)))+energy;z=ca.vec(w);p=ca.vertcat(ca.vec(state),ca.vec(vel));constraints=[];lo=[];hi=[]
    if enabled:
        q=ca.MX.sym('normal',3);prior=ca.MX.sym('prior',3);z=ca.vertcat(z,q);p=ca.vertcat(p,prior);obj+=1e-8*ca.sumsqr(q-prior)
        for i in left+right:
            constraints.append(ca.dot(q,trial[:,i]-trial[:,0])*(1 if i in right else -1)/dt);lo.append(gap/dt);hi.append(np.inf)
        constraints.extend([ca.dot(q,trial[:,1]-trial[:,0])/dt,ca.sumsqr(q)]);lo.extend([0,1]);hi.extend([0,1])
    count=len(lo);constraints.extend(js);lo.extend([.1]*len(t));hi.extend([np.inf]*len(t));cons=ca.vertcat(*constraints)
    solver=ca.nlpsol('step','ipopt',{'x':z,'p':p,'f':obj,'g':cons},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.bound_relax_factor':0.,'ipopt.max_iter':200});grad=ca.Function('grad',[z,p],[ca.gradient(obj,z),ca.jacobian(cons,z)])
    g=MeshGate(t);assert not g.check(x,x)['unresolved'];states=[x.copy()];rows=[]
    for step in range(steps):
        par=np.r_[x.ravel(),v.ravel()];guess=v.ravel()
        if enabled:par=np.r_[par,normal];guess=np.r_[guess,normal]
        sol=solver(x0=guess,p=par,lbg=lo,ubg=hi);assert solver.stats()['success'],(step,solver.stats())
        zz=np.asarray(sol['x']).ravel();vn=zz[:3*n].reshape(n,3);y=x+dt*vn;val=np.asarray(sol['g']).ravel();mul=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,par);kkt=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mul)));j=float(min(val[count:]));err=float(np.linalg.norm((mass[:,None]*vn).sum(axis=0)-mom));assert j>.1 and kkt<1e-7 and err<1e-6
        impulse=0.;clear=None
        if enabled:
            normal=zz[-3:];impulse=float(-sum(mul[:4]));clear=float(min(val[:4])*dt);assert clear>=gap-1e-10 and abs(val[4])<1e-8 and abs(val[5]-1)<1e-8
        audit=g.check(x,y);wit=overlap(y[t[0]],y[t[-1]])
        if enabled:assert not audit['unresolved'] and wit is None,(step,audit,wit)
        rows.append(dict(step=step,j=j,kkt=kkt,momentum=err,impulse=impulse,clearance=clear,unresolved=audit['unresolved'],overlap_witness=wit));states.append(y.copy());x,v=y,vn
    if enabled:assert any(r['impulse']>1e-7 for r in rows)
    else:assert any(r['overlap_witness'] is not None for r in rows)
    return t,states,rows

def run(out):
    out.mkdir(exist_ok=False);summary=[]
    for rot in (False,True):
        for enabled in (True,False):
            t,s,r=simulate(enabled,rot);name=f'rot{int(rot)}_enabled{int(enabled)}';np.savez_compressed(out/(name+'.npz'),states=s,cells=t);(out/(name+'.json')).write_text(json.dumps(r,indent=2)+'\n')
            row=dict(case=name,steps=len(r),active_steps=sum(x['impulse']>1e-7 for x in r),overlap_states=sum(x['overlap_witness'] is not None for x in r),unresolved_intervals=sum(bool(x['unresolved']) for x in r),min_j=min(x['j'] for x in r),min_clearance=min((x['clearance'] for x in r),default=None) if enabled else None);summary.append(row);print(row,flush=True)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]))
