#!/usr/bin/env python3
"""Independent implicit FEM using CasADi/IPOPT, restricted planar contact."""
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
      writer=csv.writer(raw);writer.writerow(['scene','step','nodes','cells','iterations','energy','kinetic','elastic','min_j','gap','impulse_a','impulse_b','momentum_error','stationarity_inf','complementarity_inf','contact_nodes'])
      scenes=[(1,1,1,False,False),(1,1,1,True,False),(1,2,2,False,False),(1,2,2,True,False),(2,2,2,True,False),(1,2,2,False,True)]
      for sid,(nx,ny,nz,rot,remote) in enumerate(scenes):
        gap=.0015; initial_gap=.008 if not remote else .08;dt=.001;steps=32
        a,ta=block(nx,ny,nz,0.);b,tb=block(nx,ny,nz,nx*.04+initial_gap)
        na=len(a);cells=np.vstack([ta,tb+na]);x=np.vstack([a,b]);n=len(x)
        v=np.zeros_like(x);v[:na,0]=.32;v[na:,0]=-.24
        if rot:v[:na,1]=.08;v[na:,1]=-.06
        R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if rot else np.eye(3)
        x=x@R.T;v=v@R.T;normal=R[:,0];rest=x.copy()
        dm=(rest[cells[:,1:]]-rest[cells[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;assert np.all(vol>0)
        inv=np.linalg.inv(dm);mass=np.zeros(n);rho=np.r_[np.full(len(ta),360.),np.full(len(tb),480.)]
        np.add.at(mass,cells.ravel(),np.repeat(vol*rho/4,4))
        # Minimize discrete inertia plus elastic energy using independent AD.
        # Plane coordinate is also optimized, so contact conserves total momentum.
        u=ca.MX.sym('u',3,n);p=ca.MX.sym('plane');state=ca.MX.sym('state',3,n);velocity=ca.MX.sym('velocity',3,n)
        trial=state+dt*u;mu=12000/(2*1.32);lam=12000*.32/(1.32*(1-2*.32));E=0;Js=[]
        for i,t in enumerate(cells):
          ds=ca.horzcat(*[trial[:,int(k)]-trial[:,int(t[0])] for k in t[1:]])
          F=ds@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));Js.append(J)
          log=ca.log(J);E+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
        inertial=.5*ca.sum2(ca.sum1((u-velocity)**2)*ca.DM(mass.reshape(1,-1)))
        proj=ca.DM(normal.reshape(1,3))@trial/dt
        constraints=ca.vertcat(ca.vec(p-proj[:,:na]-gap/(2*dt)),ca.vec(proj[:,na:]-p-gap/(2*dt)),*Js)
        z=ca.vertcat(ca.vec(u),p);par=ca.vertcat(ca.vec(state),ca.vec(velocity));obj=inertial+E
        solver=ca.nlpsol('step','ipopt',{'x':z,'p':par,'f':obj,'g':constraints},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':100,'ipopt.bound_relax_factor':0.0})
        grad=ca.Function('kkt',[z,par],[ca.gradient(obj,z),ca.jacobian(constraints,z)])
        diagnostics=ca.Function('diag',[z,par],[E,ca.vertcat(*Js)])
        lower=np.r_[np.zeros(n),np.full(len(cells),.1)];upper=np.full(n+len(cells),np.inf)
        momentum=(mass[:,None]*v).sum(axis=0);total_imp=0.;minj=1.;maxres=0.
        trace.write(f'{sid} {n} {len(cells)} {steps}\n');np.savetxt(trace,cells,fmt='%d');np.savetxt(trace,x,fmt='%.17g')
        for step in range(steps):
          parval=np.r_[x.ravel(),v.ravel()];guess=np.r_[v.ravel(),((x[:na]@normal).max()+(x[na:]@normal).min())/(2*dt)]
          sol=solver(x0=guess,p=parval,lbg=lower,ubg=upper);stats=solver.stats()
          if not stats['success']:raise RuntimeError((sid,step,stats['return_status']))
          zz=np.asarray(sol['x']).ravel();newv=zz[:-1].reshape(n,3);newx=x+dt*newv
          g=np.asarray(sol['g']).ravel();multipliers=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,parval)
          residual=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@multipliers)))
          comp=float(np.max(np.abs((g-lower)*multipliers)))
          elastic,j=diagnostics(zz,parval);min_j=float(np.min(j));kinetic=float(.5*np.sum(mass[:,None]*newv**2));energy=kinetic+float(elastic)
          measured_gap=float((newx[na:]@normal).min()-(newx[:na]@normal).max())
          ia=float(-multipliers[:na].sum());ib=float(-multipliers[na:n].sum());total_imp+=ia
          mom_error=float(np.linalg.norm((mass[:,None]*newv).sum(axis=0)-momentum))
          if min_j<=.1 or measured_gap<gap-1e-10 or residual>1e-7 or comp>1e-7 or abs(ia-ib)>1e-7 or mom_error>1e-6:raise RuntimeError((sid,step,min_j,measured_gap,residual,comp,ia,ib,mom_error))
          writer.writerow([sid,step,n,len(cells),stats['iter_count'],energy,kinetic,float(elastic),min_j,measured_gap,ia,ib,mom_error,residual,comp,int(np.sum(multipliers[:n]<-1e-7))]);np.savetxt(trace,newx,fmt='%.17g');x,v=newx,newv;minj=min(minj,min_j);maxres=max(maxres,residual)
        if remote:assert total_imp<1e-6,(sid,total_imp)
        else:assert total_imp>1e-4,(sid,total_imp)
        print(f'scene={sid} cells={len(cells)} steps={steps} impulse={total_imp:.9g} minJ={minj:.9g} maxKKT={maxres:.3g}',flush=True)
    (out/'environment.json').write_text(json.dumps({'casadi':ca.__version__,'numpy':np.__version__,'solver':'IPOPT','dt_s':.001,'gap_m':.0015,'scope':'independent implicit FEM, planar two-body contact'},indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);run(p.parse_args().output)
