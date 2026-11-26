#!/usr/bin/env python3
"""Independent implicit FEM contact with geometry-derived convex regions.

There are no hand-assigned contact regions or normal directions. Start with
one convex tetrahedron per region, greedily merge face-connected regions
whose rest-volume sum matches their convex hull, and constrain the moving
convex hull of every cross-body region pair. The region partition covers
all tetrahedra; even if a region becomes concave in motion, constraining
its hull stays conservative (can be overrestrictive). SAT supplies initial
separating normals; IPOPT optimizes their directions each implicit step.
Endpoint constraints alone do not certify continuous motion: the exported
linear paths require independent CCD and whole-volume replay. Cross-body
contact only; arbitrary same-body self-contact response is not claimed.
"""
import argparse
import csv
from itertools import combinations, product
import json
from pathlib import Path
import casadi as ca
import numpy as np
from scipy.spatial import ConvexHull


def voxel_mesh(voxels, spacing, offset):
    """Fixtures only: build conforming six-tetrahedron voxel cells."""
    lookup={};points=[];cells=[]
    for i,j,k in sorted(voxels):
        ids=[]
        for di,dj,dk in [(0,0,0),(1,0,0),(0,1,0),(1,1,0),(0,0,1),(1,0,1),(0,1,1),(1,1,1)]:
            key=(i+di,j+dj,k+dk)
            if key not in lookup:
                lookup[key]=len(points);points.append(np.asarray(key)*spacing+offset)
            ids.append(lookup[key])
        a,b,c,d,e,f,g,h=ids
        cells.extend([(a,b,d,h),(a,d,c,h),(a,c,g,h),(a,g,e,h),(a,e,f,h),(a,f,b,h)])
    return np.asarray(points),np.asarray(cells,dtype=int)


def determinants(x,cells):
    return np.linalg.det((x[cells[:,1:]]-x[cells[:,:1]]).transpose(0,2,1))


def convex_regions(x,cells):
    """Deterministic face-adjacent merging using topology and rest geometry.

    Hull comparison is a decomposition heuristic, NOT a safety predicate.
    The final planes are always checked against all nodes in each region.
    """
    vols=determinants(x,cells)/6
    if np.any(vols<=0):raise ValueError('nonpositive input tetrahedron')
    incidence={}
    for i,t in enumerate(cells):
        for face in combinations(t,3):incidence.setdefault(tuple(sorted(face)),[]).append(i)
    if any(len(v)>2 for v in incidence.values()):raise ValueError('nonmanifold face')
    adjacency={i:set() for i in range(len(cells))}
    for v in incidence.values():
        if len(v)==2:a,b=v;adjacency[a].add(b);adjacency[b].add(a)
    groups={i:({i},set(map(int,t)),float(vols[i])) for i,t in enumerate(cells)}
    while True:
        best=None
        for a,b in combinations(sorted(groups),2):
            ca_,na,va=groups[a];cb,nb,vb=groups[b]
            if not any(adjacency[t]&cb for t in ca_):continue
            ids=sorted(na|nb);volume=va+vb;hull=ConvexHull(x[ids])
            if abs(hull.volume-volume)>max(1e-14,1e-9*volume):continue
            # Prefer larger contiguous unions, break ties by cell identity.
            key=(-round(volume,14),a,b)
            if best is None or key<best[0]:best=(key,a,b,ca_|cb,set(ids),volume)
        if best is None:break
        _,a,b,c,n,v=best;groups[a]=(c,n,v);del groups[b]
    result=[{'cells':sorted(g[0]),'nodes':sorted(g[1]),'volume':g[2]} for _,g in sorted(groups.items())]
    assert sorted(i for g in result for i in g['cells'])==list(range(len(cells)))
    assert all(set(g['nodes'])==set(cells[g['cells']].ravel()) for g in result)
    return result


def separating_axis(a,b):
    """Initial separating-axis search using hull faces and crossed edges."""
    ha,hb=ConvexHull(a),ConvexHull(b)
    def edges(p,h):
        return [p[j]-p[i] for i,j in sorted({tuple(sorted(e)) for f in h.simplices for e in combinations(f,2)})]
    axes=[*ha.equations[:,:3],*hb.equations[:,:3]]
    axes.extend(np.cross(e,f) for e,f in product(edges(a,ha),edges(b,hb)))
    axes=np.asarray(axes);norm=np.linalg.norm(axes,axis=1);axes=axes[norm>1e-15]/norm[norm>1e-15,None]
    axes=np.vstack([axes,-axes]);gaps=(b@axes.T).min(axis=0)-(a@axes.T).max(axis=0)
    k=int(np.argmax(gaps));return axes[k],float(gaps[k])


def scene(sid):
    # Two different enclosing topologies and two different insert topologies;
    # geometry is prescribed, but decomposition and directions are automatic.
    u=[(i,j,0) for i in range(3) for j in range(3) if i!=1 or j==0]
    l=[(i,j,0) for i in range(3) for j in range(3) if i==0 or j==0]
    elbow=[(0,0,0),(1,0,0),(0,1,0)]
    if sid==0:
        a,ta=voxel_mesh(u,.04,np.zeros(3));b,tb=voxel_mesh([(0,0,0)],.032,np.array([.044,.048,.004]));rot=False
    elif sid==2:
        a,ta=voxel_mesh(u,.04,np.zeros(3));b,tb=voxel_mesh(elbow,.014,np.array([.046,.048,.013]));rot=True
    else:
        a,ta=voxel_mesh(l,.04,np.zeros(3));b,tb=voxel_mesh(elbow,.014,np.array([.048,.048,.013]));rot=True
    v=np.zeros((len(a)+len(b),3))
    if sid!=3:v[len(a):]=[-.32,-.42,0.]
    R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]) if rot else np.eye(3)
    return a@R.T,ta,b@R.T,tb,v@R.T


def run(out):
    out.mkdir(parents=True,exist_ok=False);gap=.0015;dt=.001;steps=32;metadata=[]
    with (out/'trajectory.txt').open('w') as trace,(out/'receipts.csv').open('w',newline='') as raw:
        writer=csv.writer(raw);writer.writerow(['scene','step','nodes','cells','regions_a','regions_b','region_pairs','iterations','min_j','min_gap','impulse_total','active_pairs','momentum_error','stationarity_inf','complementarity_inf','normal_from_initial_deg','unit_normal_error'])
        for sid in range(4):
            a,ta,b,tb,v=scene(sid);na=len(a);x=np.vstack([a,b]);cells=np.vstack([ta,tb+na]);n=len(x)
            ga=convex_regions(a,ta);gb=convex_regions(b,tb)
            groups=[(r['nodes'],[i+na for i in s['nodes']]) for r,s in product(ga,gb)]
            normals=[];initial_gaps=[]
            for left,right in groups:
                axis,clearance=separating_axis(x[left],x[right]);assert clearance>gap,(sid,clearance);normals.append(axis);initial_gaps.append(clearance)
            normals=np.asarray(normals);initial=normals.copy();m=len(groups)
            print(f'scene {sid}: automatic regions {len(ga)} x {len(gb)} = {m} pair constraints',flush=True)
            metadata.append({'scene':sid,'regions_a':ga,'regions_b':gb,'initial_pair_gaps':initial_gaps,'initial_normals':initial.tolist()})
            dm=(x[cells[:,1:]]-x[cells[:,:1]]).transpose(0,2,1);vol=np.linalg.det(dm)/6;inv=np.linalg.inv(dm);mass=np.zeros(n)
            assert np.all(vol>0);rho=np.r_[np.full(len(ta),360.),np.full(len(tb),480.)];np.add.at(mass,cells.ravel(),np.repeat(vol*rho/4,4))
            w=ca.MX.sym('w',3,n);planes=ca.MX.sym('planes',m);axes=ca.MX.sym('axes',3,m);prior=ca.MX.sym('prior',3,m);state=ca.MX.sym('state',3,n);velocity=ca.MX.sym('velocity',3,n)
            trial=state+dt*w;energy=0;Js=[];mu=12000/(2*1.32);lam=12000*.32/(1.32*(1-2*.32))
            for i,t in enumerate(cells):
                ds=ca.horzcat(*[trial[:,int(j)]-trial[:,int(t[0])] for j in t[1:]])
                F=ds@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));Js.append(J);log=ca.log(J);energy+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
            inertia=.5*ca.sum2(ca.sum1((w-velocity)**2)*ca.DM(mass.reshape(1,-1)))
            inequalities=[]
            for k,(left,right) in enumerate(groups):
                proj=axes[:,k].T@trial/dt;inequalities.extend([ca.vec(planes[k]-proj[:,left]-gap/(2*dt)),ca.vec(proj[:,right]-planes[k]-gap/(2*dt))])
            count=sum(len(l)+len(r) for l,r in groups)
            cons=ca.vertcat(*inequalities,*Js,*[ca.sumsqr(axes[:,k]) for k in range(m)])
            z=ca.vertcat(ca.vec(w),planes,ca.vec(axes));par=ca.vertcat(ca.vec(state),ca.vec(velocity),ca.vec(prior));obj=inertia+energy+1e-7*ca.sumsqr(axes-prior)
            solver=ca.nlpsol('step','ipopt',{'x':z,'p':par,'f':obj,'g':cons},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':200,'ipopt.bound_relax_factor':0.})
            grad=ca.Function('kkt',[z,par],[ca.gradient(obj,z),ca.jacobian(cons,z)]);diag=ca.Function('diag',[z,par],[ca.vertcat(*Js)])
            lower=np.r_[np.zeros(count),np.full(len(cells),.1),np.ones(m)];upper=np.r_[np.full(count+len(cells),np.inf),np.ones(m)]
            momentum=(mass[:,None]*v).sum(axis=0);total=0.;simultaneous=0
            trace.write(f'{sid} {n} {len(cells)} {steps} {na}\n');np.savetxt(trace,cells,fmt='%d');np.savetxt(trace,x,fmt='%.17g')
            for step in range(steps):
                p=np.r_[x.ravel(),v.ravel(),normals.ravel()]
                offsets=[((x[l]@normals[k]).max()+(x[r]@normals[k]).min())/(2*dt) for k,(l,r) in enumerate(groups)]
                guess=np.r_[v.ravel(),offsets,normals.ravel()];sol=solver(x0=guess,p=p,lbg=lower,ubg=upper);stats=solver.stats()
                if not stats['success']:raise RuntimeError((sid,step,stats['return_status']))
                zz=np.asarray(sol['x']).ravel();newv=zz[:3*n].reshape(n,3);newx=x+dt*newv;normals=zz[-3*m:].reshape(m,3)
                mult=np.asarray(sol['lam_g']).ravel();g=np.asarray(sol['g']).ravel();gr,jac=grad(zz,p)
                residual=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mult)));comp=float(np.max(np.abs((g[:-m]-lower[:-m])*mult[:-m])));minj=float(np.min(diag(zz,p)))
                unit=float(np.max(np.abs(np.linalg.norm(normals,axis=1)-1)));angle=float(np.degrees(np.arccos(np.clip(np.sum(normals*initial,axis=1),-1,1))).max())
                gaps=[];impulses=[];off=0
                for k,(left,right) in enumerate(groups):
                    gaps.append(float((newx[right]@normals[k]).min()-(newx[left]@normals[k]).max()))
                    ia=float(-mult[off:off+len(left)].sum());off+=len(left);ib=float(-mult[off:off+len(right)].sum());off+=len(right)
                    assert abs(ia-ib)<1e-7;impulses.append(ia)
                active=sum(i>1e-7 for i in impulses);simultaneous+=active>=2;total+=sum(impulses)
                mom=float(np.linalg.norm((mass[:,None]*newv).sum(axis=0)-momentum))
                assert unit<1e-8 and minj>.1 and min(gaps)>=gap-1e-10 and residual<1e-7 and comp<1e-7 and mom<1e-6,(sid,step,unit,minj,min(gaps),residual,comp,mom)
                writer.writerow([sid,step,n,len(cells),len(ga),len(gb),m,stats['iter_count'],minj,min(gaps),sum(impulses),active,mom,residual,comp,angle,unit]);np.savetxt(trace,newx,fmt='%.17g');x,v=newx,newv
            if sid==3:assert total<1e-6 and simultaneous==0,(sid,total,simultaneous)
            else:assert total>1e-4 and simultaneous>0,(sid,total,simultaneous)
            print(f'PASS scene={sid} cells={len(cells)} active-simultaneous={simultaneous} total-impulse={total:.9g}',flush=True)
    (out/'regions.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'environment.json').write_text(json.dumps({'casadi':ca.__version__,'numpy':np.__version__,'dt':dt,'gap':gap,'scope':'automatic convex region construction from tet meshes, pairwise cross-body optimized planes; no arbitrary self-contact response'},indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);run(p.parse_args().output)
