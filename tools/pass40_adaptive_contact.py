#!/usr/bin/env python3
"""Independent implicit FEM using CasADi/IPOPT, three deformable bodies with independently optimized unit separating normals at every step."""
import argparse, csv, json
from pathlib import Path
import casadi as ca
import numpy as np

def block(nx,ny,nz,offset):
    x=np.array([(i*.04+offset,j*.04,k*.04) for i in range(nx+1) for j in range(ny+1) for k in range(nz+1)])
    def idx(i,j,k): return (i*(ny+1)+j)*(nz+1)+k
    cells=[]
    for i in range(nx):
      for j in range(ny):
       for k in range(nz):
        a,b,c,d,e,f,g,h=[idx(i+u,j+v,k+w) for u,v,w in [(0,0,0),(1,0,0),(0,1,0),(1,1,0),(0,0,1),(1,0,1),(0,1,1),(1,1,1)]]
        cells.extend([(a,b,d,h),(a,d,c,h),(a,c,g,h),(a,g,e,h),(a,e,f,h),(a,f,b,h)])
    return x,np.array(cells)

def run(out):
    out.mkdir(parents=True,exist_ok=False)
    with (out/'trajectory.txt').open('w') as trace,(out/'ipopt_receipts.csv').open('w',newline='') as raw:
      writer=csv.writer(raw);writer.writerow(['scene','step','nodes','cells','iterations','energy','kinetic','elastic','min_j','gap','impulse01','impulse02','impulse12','momentum_error','stationarity_inf','complementarity_inf','active_pairs','normal_change_max_deg','normal_from_initial_max_deg','unit_normal_error'])
      scenes=[(1,1,1,False,False),(1,1,1,True,False),(1,1,2,True,False),(1,1,1,False,True)]
      for sid,(nx,ny,nz,rot,remote) in enumerate(scenes):
        gap=.0015; initial_gap=.008 if not remote else .08;dt=.001;steps=32
        a,ta=block(nx,ny,nz,0.);nb=len(a);n=3*nb
        cells=np.vstack([ta,ta+nb,ta+2*nb]);x=np.vstack([a,a+[nx*.04+initial_gap,0,0],a+[0,ny*.04+initial_gap,0]])
        v=np.repeat(np.array([[.15,.15,0],[-.25,0,0],[0,-.25,0]]),nb,axis=0)
        R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if rot else np.eye(3)
        x=x@R.T;v=v@R.T;rest=x.copy();pairs=[(0,1),(0,2),(1,2)]
        normals=np.array([[1,0,0],[0,1,0],[-1/np.sqrt(2),1/np.sqrt(2),0]])@R.T
        # Add spin to challenge the fixed separating directions.
        if not remote:
          for body,omega in enumerate([np.array([0.,0.,12.]),np.array([0.,0.,-8.]),np.array([0.,0.,5.])]):
            sl=slice(body*nb,(body+1)*nb);v[sl]+=np.cross(omega@R.T,x[sl]-x[sl].mean(axis=0))
        initial_normals=normals.copy()
        dm=(rest[cells[:,1:]]-rest[cells[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;assert np.all(vol>0)
        inv=np.linalg.inv(dm);mass=np.zeros(n);rho=np.repeat([360.,480.,420.],len(ta))
        np.add.at(mass,cells.ravel(),np.repeat(vol*rho/4,4))
        # Minimize discrete inertia plus elastic energy using independent AD.
        # Plane coordinate is also optimized, so contact conserves total momentum.
        u=ca.MX.sym('u',3,n);p=ca.MX.sym('plane',3);axis=ca.MX.sym('axis',3,3);prior=ca.MX.sym('prior',3,3);state=ca.MX.sym('state',3,n);velocity=ca.MX.sym('velocity',3,n)
        trial=state+dt*u;mu=12000/(2*1.32);lam=12000*.32/(1.32*(1-2*.32));E=0;Js=[]
        for i,t in enumerate(cells):
          ds=ca.horzcat(*[trial[:,int(k)]-trial[:,int(t[0])] for k in t[1:]])
          F=ds@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));Js.append(J)
          log=ca.log(J);E+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
        inertial=.5*ca.sum2(ca.sum1((u-velocity)**2)*ca.DM(mass.reshape(1,-1)))
        constraints=[]
        for k,(i,j) in enumerate(pairs):
          proj=axis[:,k].T@trial/dt
          constraints.extend([ca.vec(p[k]-proj[:,i*nb:(i+1)*nb]-gap/(2*dt)),ca.vec(proj[:,j*nb:(j+1)*nb]-p[k]-gap/(2*dt))])
        constraints=ca.vertcat(*constraints,*Js,*[ca.sumsqr(axis[:,k]) for k in range(3)])
        z=ca.vertcat(ca.vec(u),p,ca.vec(axis));par=ca.vertcat(ca.vec(state),ca.vec(velocity),ca.vec(prior))
        # Normal-only regularizer selects a reproducible feasible plane.
        obj=inertial+E+1e-8*ca.sumsqr(axis-prior)
        solver=ca.nlpsol('step','ipopt',{'x':z,'p':par,'f':obj,'g':constraints},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':100,'ipopt.bound_relax_factor':0.0})
        grad=ca.Function('kkt',[z,par],[ca.gradient(obj,z),ca.jacobian(constraints,z)])
        diagnostics=ca.Function('diag',[z,par],[E,ca.vertcat(*Js)])
        lower=np.r_[np.zeros(6*nb),np.full(len(cells),.1),np.ones(3)];upper=np.r_[np.full(6*nb+len(cells),np.inf),np.ones(3)]
        momentum=(mass[:,None]*v).sum(axis=0);total_imp=np.zeros(3);simultaneous=0;minj=1.;maxres=0.
        trace.write(f'{sid} {n} {len(cells)} {steps}\n');np.savetxt(trace,cells,fmt='%d');np.savetxt(trace,x,fmt='%.17g')
        for step in range(steps):
          parval=np.r_[x.ravel(),v.ravel(),normals.ravel()];guess=np.r_[v.ravel(),[((x[i*nb:(i+1)*nb]@normals[k]).max()+(x[j*nb:(j+1)*nb]@normals[k]).min())/(2*dt) for k,(i,j) in enumerate(pairs)],normals.ravel()]
          sol=solver(x0=guess,p=parval,lbg=lower,ubg=upper);stats=solver.stats()
          if not stats['success']:raise RuntimeError((sid,step,stats['return_status']))
          zz=np.asarray(sol['x']).ravel();newv=zz[:3*n].reshape(n,3);newx=x+dt*newv
          newnormals=zz[-9:].reshape(3,3);unit_error=float(np.max(np.abs(np.linalg.norm(newnormals,axis=1)-1)))
          change=float(np.degrees(np.arccos(np.clip(np.sum(newnormals*normals,axis=1),-1,1))).max())
          change_initial=float(np.degrees(np.arccos(np.clip(np.sum(newnormals*initial_normals,axis=1),-1,1))).max())
          normals=newnormals
          g=np.asarray(sol['g']).ravel();multipliers=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,parval)
          residual=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@multipliers)))
          comp=float(np.max(np.abs((g[:-3]-lower[:-3])*multipliers[:-3])))
          elastic,j=diagnostics(zz,parval);min_j=float(np.min(j));kinetic=float(.5*np.sum(mass[:,None]*newv**2));energy=kinetic+float(elastic)
          gaps=[];imp=[]
          for k,(i,j) in enumerate(pairs):
            gaps.append(float((newx[j*nb:(j+1)*nb]@normals[k]).min()-(newx[i*nb:(i+1)*nb]@normals[k]).max()))
            ia=-multipliers[2*k*nb:(2*k+1)*nb].sum();ib=-multipliers[(2*k+1)*nb:(2*k+2)*nb].sum()
            if abs(ia-ib)>1e-7:raise RuntimeError('unbalanced pair impulse')
            imp.append(float(ia))
          active=sum(i>1e-7 for i in imp);simultaneous+=active>=2;total_imp+=imp
          mom_error=float(np.linalg.norm((mass[:,None]*newv).sum(axis=0)-momentum))
          if unit_error>1e-8 or min_j<=.1 or min(gaps)<gap-1e-10 or residual>1e-7 or comp>1e-7 or mom_error>1e-6:raise RuntimeError((sid,step,min_j,gaps,residual,comp,mom_error))
          writer.writerow([sid,step,n,len(cells),stats['iter_count'],energy,kinetic,float(elastic),min_j,min(gaps),*imp,mom_error,residual,comp,active,change,change_initial,unit_error]);np.savetxt(trace,newx,fmt='%.17g');x,v=newx,newv;minj=min(minj,min_j);maxres=max(maxres,residual)
        if remote:assert max(total_imp)<1e-6,(sid,total_imp)
        else:assert simultaneous>0 and total_imp[0]>1e-4 and total_imp[1]>1e-4,(sid,total_imp,simultaneous)
        print(f'scene={sid} cells={len(cells)} steps={steps} simultaneous={simultaneous} impulse={total_imp} minJ={minj:.9g} maxKKT={maxres:.3g}',flush=True)
    (out/'environment.json').write_text(json.dumps({'casadi':ca.__version__,'numpy':np.__version__,'solver':'IPOPT','dt_s':.001,'gap_m':.0015,'scope':'independent implicit FEM, three-body dynamically optimized pairwise unit separating planes, not arbitrary mesh contact'},indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);run(p.parse_args().output)
