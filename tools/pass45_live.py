"""Restricted same-body implicit neo-Hookean contact: disjoint-node regions only."""
import csv,json,sys
from pathlib import Path
from itertools import combinations
from fractions import Fraction as Q
import numpy as np
import casadi as ca
from scipy.optimize import linprog
from pass45_transaction import Gate,reject
from pass42_physical_replay import mesh,regions
from pass42_region_certificate import axis_witness,certify,rational_points,endpoint_gap

def inside(p,t):
    p=tuple(Q(float(x)) for x in p); t=[tuple(Q(float(x)) for x in a) for a in t]
    sub=lambda a,b:tuple(x-y for x,y in zip(a,b))
    def det(a,b,c):return a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
    a,b,c=[sub(t[i],t[0]) for i in (1,2,3)];d=sub(p,t[0]);den=det(a,b,c)
    if not den:return False
    u,v,w=det(d,b,c)/den,det(a,d,c)/den,det(a,b,d)/den
    return min(u,v,w,1-u-v-w)>0

def witness(x,t,A,B):
    # Floating LP proposes an overlap point; exact barycentrics verify it.
    for i in A:
        a=x[t[i]]
        for j in B:
            b=x[t[j]]
            if np.any(a.max(0)<=b.min(0)) or np.any(b.max(0)<=a.min(0)):continue
            eq=np.zeros((5,9));eq[:3,:4]=a.T;eq[:3,4:8]=-b.T;eq[3,:4]=1;eq[4,4:8]=1
            ub=np.zeros((8,9));ub[:,:8]=-np.eye(8);ub[:,8]=1
            sol=linprog([0]*8+[-1],A_ub=ub,b_ub=np.zeros(8),A_eq=eq,b_eq=[0,0,0,1,1],bounds=[(0,1)]*9,method='highs')
            if sol.success and sol.x[8]>1e-7:
                p=sol.x[:4]@a
                if inside(p,a) and inside(p,b):return [int(i),int(j),p.tolist()]
    return None

def certify_hint(a0,a1,b0,b1,axis,margin=.0002):
    # A solver-provided axis is just a proposal, checked exactly at both ends.
    q=tuple(Q(float(x)) for x in axis);norm2=sum(x*x for x in q)
    g=min(endpoint_gap(rational_points(a0),rational_points(b0),q),endpoint_gap(rational_points(a1),rational_points(b1),q))
    if norm2 and g>0 and g*g>Q(float(margin))**2*norm2:return True,1,0
    return certify(a0,a1,b0,b1,margin,max_depth=8)

def run(out,steps=32,selected=None):
    out.mkdir(exist_ok=False);dt=.001;gap=.0008;records=[]
    with (out/'steps.csv').open('w',newline='') as f:
        wr=csv.writer(f);wr.writerow(['scene','enabled','step','min_j','gap','impulse','momentum_error','kkt','certified','unresolved','overlap'])
        for sid,rot,enabled,static in [(0,False,True,False),(1,False,False,False),(2,True,True,False),(3,True,False,False),(4,True,True,True)]:
            if selected is not None and sid != selected:continue
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
            w=ca.MX.sym('w',3,n);state=ca.MX.sym('state',3,n);vel=ca.MX.sym('vel',3,n);trial=state+dt*w;energy=0;js=[];mu=2500/(2*1.32);lam=2500*.32/(1.32*(1-2*.32))
            for i,c in enumerate(t):
                F=ca.horzcat(*[trial[:,int(j)]-trial[:,int(c[0])] for j in c[1:]])@ca.DM(inv[i]);J=ca.dot(F[:,0],ca.cross(F[:,1],F[:,2]));js.append(J);log=ca.log(J);energy+=vol[i]*(mu/2*(ca.sumsqr(F)-3)-mu*log+lam/2*log**2)
            obj=.5*ca.sum2(ca.sum1((w-vel)**2)*ca.DM(mass.reshape(1,-1)))+energy;z=ca.vec(w);p=ca.vertcat(ca.vec(state),ca.vec(vel));ineq=[];count=0
            if enabled:
                planes=ca.MX.sym('planes',m);axes=ca.MX.sym('axes',3,m);prior=ca.MX.sym('prior',3,m);z=ca.vertcat(z,planes,ca.vec(axes));p=ca.vertcat(p,ca.vec(prior));obj+=1e-7*ca.sumsqr(axes-prior)
                for k,(a,b) in enumerate(group):
                    proj=axes[:,k].T@trial/dt;ineq.extend([ca.vec(planes[k]-proj[:,a]-gap/(2*dt)),ca.vec(proj[:,b]-planes[k]-gap/(2*dt))])
                count=sum(len(a)+len(b) for a,b in group)
            cons=ca.vertcat(*ineq,*js,*([ca.sumsqr(axes[:,k]) for k in range(m)] if enabled else []));lower=np.r_[np.zeros(count),np.full(len(t),.1),np.ones(m if enabled else 0)];upper=np.r_[np.full(count+len(t),np.inf),np.ones(m if enabled else 0)]
            solver=ca.nlpsol('step','ipopt',{'x':z,'p':p,'f':obj,'g':cons},{'ipopt.print_level':0,'ipopt.sb':'yes','print_time':False,'ipopt.tol':1e-10,'ipopt.constr_viol_tol':1e-10,'ipopt.acceptable_tol':1e-9,'ipopt.max_iter':200,'ipopt.bound_relax_factor':0.});grad=ca.Function('grad',[z,p],[ca.gradient(obj,z),ca.jacobian(cons,z)])
            gate=Gate(x,v,t,g) if enabled else None
            states=[x.copy()];impulses=[];certs=0;unres=0;wits=[];minj=1.;maxerr=0.;maxkkt=0.;minclear=float('inf')
            for step in range(steps):
                par=np.r_[x.ravel(),v.ravel()];guess=v.ravel()
                if enabled:
                    par=np.r_[par,normals.ravel()];offset=[((x[a]@normals[k]).max()+(x[b]@normals[k]).min())/(2*dt) for k,(a,b) in enumerate(group)];guess=np.r_[guess,offset,normals.ravel()]
                sol=solver(x0=guess,p=par,lbg=lower,ubg=upper);assert solver.stats()['success'],(sid,step,solver.stats()['return_status'])
                zz=np.asarray(sol['x']).ravel();vn=zz[:3*n].reshape(n,3);y=x+dt*vn;val=np.asarray(sol['g']).ravel();mul=np.asarray(sol['lam_g']).ravel();gr,jac=grad(zz,par);kkt=float(np.max(np.abs(np.asarray(gr).ravel()+np.asarray(jac).T@mul)));j=float(min(val[count:count+len(t)]));err=float(np.linalg.norm((mass[:,None]*vn).sum(axis=0)-mom));imp=0.
                if enabled:
                    normals=zz[-3*m:].reshape(m,3);off=0
                    for a,b in group:
                        ia=-sum(mul[off:off+len(a)]);off+=len(a);ib=-sum(mul[off:off+len(b)]);off+=len(b);assert abs(ia-ib)<1e-7;imp+=ia
                clear=[];ok=0;bad=0;wit=None
                for k,(a,b) in enumerate(group):
                    axis=normals[k] if enabled else axis_witness(y[a],y[b]);clear.append(float((y[b]@axis).min()-(y[a]@axis).max()))
                    safe,_,_=certify_hint(x[a],y[a],x[b],y[b],axis);ok+=safe;bad+=not safe
                    if not safe and not enabled and wit is None:wit=witness(y,t,pairs[k][0]['cells'],pairs[k][1]['cells'])
                if enabled:assert bad==0 and min(clear)>=gap-1e-10,(sid,step,clear,bad)
                assert j>.1 and err<1e-6 and kkt<1e-7,(sid,step,j,err,kkt)
                if gate is not None:
                    assert gate.version==step
                    if step==0:
                        bad_v=vn.copy();bad_v[0,0]=np.nan
                        reject(gate,bad_v,dt,step,'invalid trial')
                        reject(gate,vn,dt,step+1,'stale version')
                    receipt=gate.commit(vn,dt,step,normals)
                    assert np.array_equal(gate.snapshot()[0],y) and receipt.version==step+1
                if wit is not None:wits.append([step,*wit])
                certs+=ok;unres+=bad;impulses.append(float(imp));minj=min(minj,j);maxerr=max(maxerr,err);maxkkt=max(maxkkt,kkt);minclear=min(minclear,min(clear));wr.writerow([sid,int(enabled),step,j,min(clear),imp,err,kkt,ok,bad,int(wit is not None)]);f.flush();states.append(y.copy());x,v=y,vn
            if enabled and not static:assert max(impulses)>1e-7,(sid,'no contact')
            if static:assert max(impulses)<1e-7
            if not enabled:assert wits,(sid,'no overlap')
            record=dict(scene=sid,rotated=rot,enabled=enabled,stationary=static,cells=len(t),nodes=n,regions=len(g),pairs=m,steps=steps,active_steps=sum(i>1e-7 for i in impulses),certified=certs,unresolved=unres,overlap_steps=len(wits),min_j=minj,min_gap=minclear,max_momentum_error=maxerr,max_kkt=maxkkt,peak_impulse=max(impulses));record['transaction_steps']=gate.version if gate is not None else 0;record['transaction_time']=str(gate.time) if gate is not None else '0';records.append(record);(out/f'regions{sid}.json').write_text(json.dumps(g));np.savez_compressed(out/f'scene{sid}.npz',states=states,cells=t);(out/f'witnesses{sid}.json').write_text(json.dumps(wits));print(record,flush=True)
    (out/'summary.json').write_text(json.dumps(records,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]),int(sys.argv[2]) if len(sys.argv)>2 else 32,int(sys.argv[3]) if len(sys.argv)>3 else None)
