#!/usr/bin/env python3
"""Independent implicit FEM: a connected concave U and an inserted elastic block.
Each of three U regions has a local separating plane against the insert.
No single separating plane exists for the entire U/insert pair at rest.
The constraints are deliberately restricted to this known decomposition;
this is not a general-purpose arbitrary nonconvex collision detector.
"""
import argparse, csv, json
from pathlib import Path
import casadi as ca
import numpy as np
from scipy.spatial import ConvexHull


def mesh(voxels, spacing, offset):
    # Share nodes across occupied voxels, including at the U's base/arm joints.
    rows, lookup, cells, groups = [], {}, [], [set() for _ in range(3)]
    for i,j,k in voxels:
        ids=[]
        for di,dj,dk in [(0,0,0),(1,0,0),(0,1,0),(1,1,0),(0,0,1),(1,0,1),(0,1,1),(1,1,1)]:
            key=(i+di,j+dj,k+dk)
            if key not in lookup:
                lookup[key]=len(rows); rows.append(np.asarray(key)*spacing+offset)
            ids.append(lookup[key])
        a,b,c,d,e,f,g,h=ids
        cells.extend([(a,b,d,h),(a,d,c,h),(a,c,g,h),(a,g,e,h),(a,e,f,h),(a,f,b,h)])
        if i==0: groups[0].update(ids)
        if i==2: groups[1].update(ids)
        if j==0: groups[2].update(ids)
    return np.asarray(rows),np.asarray(cells),[sorted(g) for g in groups]


def run(out):
    out.mkdir(parents=True,exist_ok=False)
    scenes=[(False,False,.032),(True,False,.032),(False,False,.036),(True,True,.032)]
    gap=.0015;dt=.001;steps=32
    with (out/'trajectory.txt').open('w') as trace,(out/'receipts.csv').open('w',newline='') as raw:
        writer=csv.writer(raw);writer.writerow(['scene','step','nodes','cells','iterations','min_j','min_gap','impulse_left','impulse_right','impulse_base','active_planes','momentum_error','stationarity_inf','complementarity_inf','normal_from_initial_deg','unit_normal_error'])
        for sid,(rot,remote,width) in enumerate(scenes):
            u,tu,groups=mesh([(i,j,0) for i in range(3) for j in range(3) if i!=1 or j==0],.04,np.zeros(3))
            b,tb,_=mesh([(0,0,0)],width,np.array([.06-width/2,.048,(.04-width)/2]));nu=len(u);nb=len(b);n=nu+nb
            x=np.vstack([u,b]);cells=np.vstack([tu,tb+nu]);insert=list(range(nu,n));normals=np.array([[1.,0,0],[-1.,0,0],[0,1.,0]])
            hull=ConvexHull(u);assert np.max(b@hull.equations[:,:3].T+hull.equations[:,3])<-1e-8,'insert not strictly inside convex hull'
            v=np.zeros_like(x)
            if not remote: v[nu:]=[-.32,-.42,0.]
            R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if rot else np.eye(3)
            x=x@R.T;v=v@R.T;normals=normals@R.T;initial_normals=normals.copy();rest=x.copy()
            dm=(rest[cells[:,1:]]-rest[cells[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;assert np.all(vol>0)
            inv=np.linalg.inv(dm);mass=np.zeros(n);rho=np.r_[np.full(len(tu),360.),np.full(len(tb),480.)];np.add.at(mass,cells.ravel(),np.repeat(vol*rho/4,4))
            w=ca.MX.sym('w',3,n);plane=ca.MX.sym('plane',3);axis=ca.MX.sym('axis',3,3);prior=ca.MX.sym('prior',3,3);state=ca.MX.sym('state',3,n);velocity=ca.MX.sym('velocity',3,n)
            trial=state+dt*w;E=0;Js=[];mu=12000/(2*1.32);lam=12000*.32/(1.32*(1-2*.32))
            for i,t in enumerate(cells):
                ds=ca.horzcat(*[trial[:,int(k)]-trial[:,int(t[0])] for k in t[1:]])
                F=ds@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));Js.append(J);log=ca.log(J);E+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
            inertia=.5*ca.sum2(ca.sum1((w-velocity)**2)*ca.DM(mass.reshape(1,-1)))
            constraints=[];lengths=[]
            for k,group in enumerate(groups):
                proj=axis[:,k].T@trial/dt
                constraints.extend([ca.vec(plane[k]-proj[:,group]-gap/(2*dt)),ca.vec(proj[:,insert]-plane[k]-gap/(2*dt))]);lengths.extend([len(group),nb])
            m=sum(lengths);constraints=ca.vertcat(*constraints,*Js,*[ca.sumsqr(axis[:,k]) for k in range(3)])
            z=ca.vertcat(ca.vec(w),plane,ca.vec(axis));par=ca.vertcat(ca.vec(state),ca.vec(velocity),ca.vec(prior));obj=inertia+E+1e-7*ca.sumsqr(axis-prior)
            solver=ca.nlpsol('step','ipopt',{'x':z,'p':par,'f':obj,'g':constraints},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':150,'ipopt.bound_relax_factor':0.0})
            grad=ca.Function('kkt',[z,par],[ca.gradient(obj,z),ca.jacobian(constraints,z)]);diag=ca.Function('diag',[z,par],[ca.vertcat(*Js)])
            lower=np.r_[np.zeros(m),np.full(len(cells),.1),np.ones(3)];upper=np.r_[np.full(m+len(cells),np.inf),np.ones(3)]
            momentum=(mass[:,None]*v).sum(axis=0);total=np.zeros(3);simultaneous=0
            # Explicit owner count avoids assuming equal-size components.
            trace.write(f'{sid} {n} {len(cells)} {steps} {nu}\n');np.savetxt(trace,cells,fmt='%d');np.savetxt(trace,x,fmt='%.17g')
            for step in range(steps):
                parval=np.r_[x.ravel(),v.ravel(),normals.ravel()];guess=np.r_[v.ravel(),[((x[group]@normals[k]).max()+(x[insert]@normals[k]).min())/(2*dt) for k,group in enumerate(groups)],normals.ravel()]
                sol=solver(x0=guess,p=parval,lbg=lower,ubg=upper);stats=solver.stats();assert stats['success'],(sid,step,stats['return_status'])
                zz=np.asarray(sol['x']).ravel();newv=zz[:3*n].reshape(n,3);newx=x+dt*newv;normals=zz[-9:].reshape(3,3)
                unit=float(np.max(np.abs(np.linalg.norm(normals,axis=1)-1)));angle=float(np.degrees(np.arccos(np.clip(np.sum(normals*initial_normals,axis=1),-1,1))).max())
                g=np.asarray(sol['g']).ravel();mult=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,parval);res=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mult)));comp=float(np.max(np.abs((g[:-3]-lower[:-3])*mult[:-3])));minj=float(np.min(diag(zz,parval)))
                gaps=[];imp=[];off=0
                for k,group in enumerate(groups):
                    gaps.append(float((newx[insert]@normals[k]).min()-(newx[group]@normals[k]).max()))
                    ia=-mult[off:off+len(group)].sum();off+=len(group);ib=-mult[off:off+nb].sum();off+=nb;assert abs(ia-ib)<1e-7;imp.append(float(ia))
                active=sum(i>1e-7 for i in imp);simultaneous+=active>=2;total+=imp;mom=float(np.linalg.norm((mass[:,None]*newv).sum(axis=0)-momentum))
                assert unit<1e-8 and minj>.1 and min(gaps)>=gap-1e-10 and res<1e-7 and comp<1e-7 and mom<1e-6,(sid,step,unit,minj,gaps,res,comp,mom)
                writer.writerow([sid,step,n,len(cells),stats['iter_count'],minj,min(gaps),*imp,active,mom,res,comp,angle,unit]);np.savetxt(trace,newx,fmt='%.17g');x,v=newx,newv
            if remote:assert max(total)<1e-6
            else:assert simultaneous>0 and total[0]>1e-4 and total[2]>1e-4,(sid,total,simultaneous)
            print(f'scene={sid} cells={len(cells)} simultaneous={simultaneous} impulse={total}',flush=True)
    (out/'environment.json').write_text(json.dumps({'casadi':ca.__version__,'numpy':np.__version__,'scope':'independent nonconvex U/insert local region separating-plane implicit FEM','dt':dt,'gap':gap},indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);run(p.parse_args().output)
