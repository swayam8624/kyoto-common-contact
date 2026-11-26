"""Local independent implicit neo-Hookean FEM validation of the region gate.
Uses the same objective and region construction as Pass 41B, with a new
rotated, three-dimensional stacked U and elbow fixture and a static control.
"""
import csv,json
from pathlib import Path
from itertools import combinations,product
import numpy as np
import casadi as ca
from scipy.spatial import ConvexHull
from pass42_region_certificate import axis_witness,audit

def mesh(voxels,h,offset):
    ids={};points=[];cells=[]
    for i,j,k in sorted(voxels):
        v=[]
        for di,dj,dk in [(0,0,0),(1,0,0),(0,1,0),(1,1,0),(0,0,1),(1,0,1),(0,1,1),(1,1,1)]:
            key=(i+di,j+dj,k+dk)
            if key not in ids:ids[key]=len(points);points.append(np.array(key)*h+offset)
            v.append(ids[key])
        a,b,c,d,e,f,g,h_=v;cells.extend([(a,b,d,h_),(a,d,c,h_),(a,c,g,h_),(a,g,e,h_),(a,e,f,h_),(a,f,b,h_)])
    return np.array(points),np.array(cells)

def regions(x,t):
    vols=np.linalg.det((x[t[:,1:]]-x[t[:,:1]]).transpose(0,2,1))/6
    assert min(vols)>0
    inc={};adj=[set() for _ in t]
    for i,c in enumerate(t):
        for face in combinations(c,3):inc.setdefault(tuple(sorted(face)),[]).append(i)
    for v in inc.values():
        if len(v)==2:a,b=v;adj[a].add(b);adj[b].add(a)
    g={i:({i},set(c),vols[i]) for i,c in enumerate(t)}
    while True:
        best=None
        for a,b in combinations(sorted(g),2):
            ca,na,va=g[a];cb,nb,vb=g[b]
            if not any(adj[i]&cb for i in ca):continue
            n=na|nb;v=va+vb
            if abs(ConvexHull(x[sorted(n)]).volume-v)>max(1e-14,1e-9*v):continue
            key=(-round(v,14),a,b)
            if best is None or key<best[0]:best=(key,a,b,ca|cb,n,v)
        if best is None:break
        _,a,b,c,n,v=best;g[a]=(c,n,v);del g[b]
    return [{'cells':list(map(int,sorted(c))),'nodes':list(map(int,sorted(n)))} for c,n,v in g.values()]

def scenario(s):
    u=[(i,j,k) for i in range(3) for j in range(3) for k in range(1+(s==2)) if i!=1 or j==0]
    l=[(i,j,0) for i in range(3) for j in range(3) if i==0 or j==0]
    elbow=[(0,0,0),(1,0,0),(0,1,0)]
    a,ta=mesh(l if s==1 else u,.04,np.zeros(3));b,tb=mesh(elbow if s else [(0,0,0)],.014 if s else .032,np.array([.046,.048,.013]) if s else np.array([.044,.048,.004]))
    v=np.zeros((len(a)+len(b),3));v[len(a):]=[-.32,-.42,0.] if s!=3 else [0,0,0]
    R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if s else np.eye(3)
    return a@R.T,ta,b@R.T,tb,v@R.T

def main(out):
    out.mkdir(exist_ok=False);metadata=[];dt=.001;gap=.0015;steps=32
    with (out/'trajectory.txt').open('w') as trace,(out/'physical.csv').open('w',newline='') as file:
        writer=csv.writer(file);writer.writerow(['scene','step','cells','regions','min_j','min_gap','momentum_error','kkt','active'])
        for sid in range(4):
            a,ta,b,tb,v=scenario(sid);na=len(a);x=np.vstack((a,b));t=np.vstack((ta,tb+na));n=len(x)
            ga,gb=regions(a,ta),regions(b,tb);groups=[(r['nodes'],[i+na for i in s['nodes']]) for r,s in product(ga,gb)];m=len(groups)
            metadata.append({'scene':sid,'regions_a':ga,'regions_b':gb})
            normals=np.array([axis_witness(x[l],x[r]) for l,r in groups]);assert all((x[r]@q).min()-(x[l]@q).max()>gap for (l,r),q in zip(groups,normals))
            dm=(x[t[:,1:]]-x[t[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;inv=np.linalg.inv(dm);mass=np.zeros(n);rho=np.r_[np.full(len(ta),360.),np.full(len(tb),480.)];np.add.at(mass,t.ravel(),np.repeat(vol*rho/4,4));mom=(mass[:,None]*v).sum(axis=0)
            w=ca.MX.sym('w',3,n);planes=ca.MX.sym('p',m);axes=ca.MX.sym('a',3,m);prior=ca.MX.sym('prior',3,m);state=ca.MX.sym('state',3,n);vel=ca.MX.sym('vel',3,n);trial=state+dt*w
            energy=0;js=[];mu=12000/(2*1.32);lam=12000*.32/(1.32*(1-2*.32))
            for i,c in enumerate(t):
                F=ca.horzcat(*[trial[:,int(j)]-trial[:,int(c[0])] for j in c[1:]])@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));js.append(J);log=ca.log(J);energy+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
            inertia=.5*ca.sum2(ca.sum1((w-vel)**2)*ca.DM(mass.reshape(1,-1)));ineq=[]
            for k,(l,r) in enumerate(groups):
                proj=axes[:,k].T@trial/dt;ineq.extend([ca.vec(planes[k]-proj[:,l]-gap/(2*dt)),ca.vec(proj[:,r]-planes[k]-gap/(2*dt))])
            count=sum(len(l)+len(r) for l,r in groups);cons=ca.vertcat(*ineq,*js,*[ca.sumsqr(axes[:,k]) for k in range(m)]);z=ca.vertcat(ca.vec(w),planes,ca.vec(axes));p=ca.vertcat(ca.vec(state),ca.vec(vel),ca.vec(prior));obj=inertia+energy+1e-7*ca.sumsqr(axes-prior)
            solver=ca.nlpsol('step','ipopt',{'x':z,'p':p,'f':obj,'g':cons},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':200,'ipopt.bound_relax_factor':0.});grad=ca.Function('grad',[z,p],[ca.gradient(obj,z),ca.jacobian(cons,z)])
            lower=np.r_[np.zeros(count),np.full(len(t),.1),np.ones(m)];upper=np.r_[np.full(count+len(t),np.inf),np.ones(m)]
            trace.write(f'{sid} {n} {len(t)} {steps} {na}\n');np.savetxt(trace,t,fmt='%d');np.savetxt(trace,x,fmt='%.17g');active_steps=0
            for s in range(steps):
                par=np.r_[x.ravel(),v.ravel(),normals.ravel()];offsets=[((x[l]@normals[k]).max()+(x[r]@normals[k]).min())/(2*dt) for k,(l,r) in enumerate(groups)];sol=solver(x0=np.r_[v.ravel(),offsets,normals.ravel()],p=par,lbg=lower,ubg=upper);assert solver.stats()['success'],solver.stats()
                zz=np.asarray(sol['x']).ravel();v=zz[:3*n].reshape(n,3);x=x+dt*v;normals=zz[-3*m:].reshape(m,3);g=np.asarray(sol['g']).ravel();mul=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,par);kkt=np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mul));j=min(g[count:count+len(t)]);mg=min((x[r]@normals[k]).min()-(x[l]@normals[k]).max() for k,(l,r) in enumerate(groups));err=np.linalg.norm((mass[:,None]*v).sum(axis=0)-mom);active=0;off=0
                for l,r in groups:
                    active+=int(-sum(mul[off:off+len(l)])>1e-7);off+=len(l)+len(r)
                active_steps+=active>0;assert j>.1 and mg>=gap-1e-10 and err<1e-6 and kkt<1e-7,(sid,s,j,mg,err,kkt)
                writer.writerow([sid,s,len(t),m,j,mg,err,kkt,active]);np.savetxt(trace,x,fmt='%.17g')
            assert active_steps>0 if sid!=3 else active_steps==0
            print(f'PASS physical scene {sid}, {len(t)} tetrahedra, {m} region pairs, {active_steps} active steps',flush=True)
    (out/'regions.json').write_text(json.dumps(metadata));assert audit(out/'trajectory.txt',out/'regions.json',out/'certified.csv',.0005)
if __name__=='__main__':main(Path('physical'))
