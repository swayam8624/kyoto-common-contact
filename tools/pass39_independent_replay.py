#!/usr/bin/env python3
"""Independent NumPy/SciPy reference on native floor/free exported states.
Not an IPC dynamics solver. DOP853 is used for contact-free trajectories only.
"""
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
from scipy.integrate import solve_ivp
from scipy.spatial import cKDTree


def model(rest, cells, density=360., young=22000., poisson=.32):
    # Independent batched linear algebra and scatter assembly.
    dm = (rest[cells[:, 1:]] - rest[cells[:, :1]]).transpose(0, 2, 1)
    vol = np.linalg.det(dm) / 6
    if not np.all(vol > 0):
        raise ValueError('invalid rest orientation')
    inv = np.linalg.inv(dm)
    mass = np.zeros(len(rest))
    np.add.at(mass, cells.ravel(), np.repeat(vol*density/4, 4))
    mu = young/(2*(1+poisson)); lam = young*poisson/((1+poisson)*(1-2*poisson))
    def forces(x):
        ds = (x[cells[:, 1:]] - x[cells[:, :1]]).transpose(0, 2, 1)
        F = ds@inv
        J = np.linalg.det(F)
        if not np.all(J > 1e-6):
            raise ValueError('invalid trial deformation')
        log = np.log(J); invt = np.linalg.inv(F).transpose(0, 2, 1)
        P = mu*(F-invt) + (lam*log)[:, None, None]*invt
        W = mu/2*(np.sum(F*F, axis=(1, 2))-3)-mu*log+lam/2*log*log
        fs = (-vol[:, None, None]*(P@inv.transpose(0, 2, 1))).transpose(0, 2, 1)
        nodal = np.concatenate([-fs.sum(axis=1, keepdims=True), fs], axis=1)
        force = np.zeros_like(x); np.add.at(force, cells.ravel(), nodal.reshape(-1, 3))
        return force, np.dot(vol, np.maximum(W, 0)), J.min()
    return mass, forces


def independent_candidates(x, y, faces, edges):
    # Conservative spherical enclosure of vertex/edge/face AABBs and
    # SciPy KD-tree radius queries. Exact original binary64 box/filter
    # rules decide final candidates; parity against exported IDs is mandatory.
    lo = np.nextafter(np.minimum(x,y)-.0005, -np.inf)
    hi = np.nextafter(np.maximum(x,y)+.0005, np.inf)
    def boxes(ids):
        a,b = lo[ids].min(axis=1), hi[ids].max(axis=1)
        center = (a+b)/2
        radius = np.linalg.norm(np.maximum(center-a,b-center), axis=1)
        return a,b,center,radius
    vl,vh,vc,vr = boxes(np.arange(len(x))[:,None]); fl,fh,fc,fr = boxes(faces); el,eh,ec,er = boxes(edges)
    # Generous roundoff cushion for this measured finite metre-scale data.
    # Not a general certified KD-tree traversal claim.
    radius = np.nextafter(vr.max()+fr.max()+1e-12,np.inf)
    near = cKDTree(vc).query_ball_tree(cKDTree(fc),radius)
    out = set()
    for i, js in enumerate(near):
        for j in js:
            if i not in faces[j] and np.all(vl[i]<=fh[j]) and np.all(fl[j]<=vh[i]):
                out.add((0,i,j))
    for i,j in cKDTree(ec).query_pairs(np.nextafter(2*er.max()+1e-12,np.inf)):
        if not set(edges[i])&set(edges[j]) and np.all(el[i]<=eh[j]) and np.all(el[j]<=eh[i]):
            out.add((1,i,j))
    return out


def run(source, output):
    state = pd.read_csv(source/'states.csv', float_precision='round_trip')
    steps = pd.read_csv(source/'steps.csv', float_precision='round_trip')
    topo = pd.read_csv(source/'topology.csv')
    pairs = pd.read_csv(source/'candidates.csv')
    rows=[]
    for (size,regime), group in state.groupby(['size','regime'], sort=True):
        frames = [g.sort_values('node') for _,g in group.groupby('step')]
        st = steps[(steps['size']==size)&(steps.regime==regime)].sort_values('step')
        assert len(frames)==121 and len(st)==120
        rest=frames[0][['rx','ry','rz']].to_numpy(); cells=topo[(topo['size']==size)&(topo.kind=='tet')][['i','j','k','l']].to_numpy()
        faces=topo[(topo['size']==size)&(topo.kind=='face')][['i','j','k']].to_numpy()
        edges=np.array(sorted({tuple(sorted((int(t[i]),int(t[(i+1)%3])))) for t in faces for i in range(3)}))
        mass, force_fn=model(rest,cells)
        xyz=np.array([g[['x','y','z']].to_numpy() for g in frames]); vel=np.array([g[['vx','vy','vz']].to_numpy() for g in frames]);
        ref_force=np.array([g[['fx','fy','fz']].to_numpy() for g in frames]); imp=np.array([g.impulse.to_numpy() for g in frames]);
        np.testing.assert_allclose(frames[0].mass,mass,rtol=1e-12,atol=1e-12)
        force_err=step_err=velocity_err=imp_err=energy_err=0.; candidate_count=0
        for k in range(121):
            force, energy, J=force_fn(xyz[k]); force_err=max(force_err,float(np.max(np.abs(force-ref_force[k]))))
            np.testing.assert_allclose(force,ref_force[k],rtol=1e-9,atol=1e-9)
            if k>0:
                energy_err=max(energy_err,abs(energy-st.iloc[k-1].elastic)); np.testing.assert_allclose(J,st.iloc[k-1].min_j,atol=1e-12,rtol=1e-12)
            if k==120: break
            dt=float(st.iloc[k]['dt']); gravity=9.81 if regime=='impact' else 0.
            v=vel[k]+dt*(force/mass[:,None]+[0,0,-gravity]); x=xyz[k]+dt*v
            coll=(x[:,2]<0)&(v[:,2]<0); j=np.where(coll,-1.25*mass*v[:,2],0.)
            x[:,2]=np.maximum(x[:,2],0.)
            tang=np.linalg.norm(v[:,:2],axis=1)
            scale=np.ones(len(v)); nz=coll&(tang>0); scale[nz]=np.maximum(0,1-.2*j[nz]/(mass[nz]*tang[nz]))
            v[:,:2]*=scale[:,None]; v[coll,2]*=-.25
            step_err=max(step_err,float(np.max(np.abs(x-xyz[k+1])))); velocity_err=max(velocity_err,float(np.max(np.abs(v-vel[k+1]))));imp_err=max(imp_err,float(np.max(np.abs(j-imp[k+1]))))
            np.testing.assert_allclose(x,xyz[k+1],atol=1e-10,rtol=1e-10);np.testing.assert_allclose(v,vel[k+1],atol=1e-10,rtol=1e-10);np.testing.assert_allclose(j,imp[k+1],atol=1e-10,rtol=1e-10)
            if (k+1)%10==0:
                p=pairs[(pairs['size']==size)&(pairs.regime==regime)&(pairs.step==k+1)]
                truth=set(map(tuple,p[['kind','first','second']].to_numpy()))
                actual=independent_candidates(xyz[k],xyz[k+1],faces,edges)
                if actual!=truth: raise ValueError(f'candidate mismatch {size} {regime} {k}: {len(truth-actual)} missing {len(actual-truth)} extra')
                candidate_count+=len(actual)
        ode_error=np.nan; ode_vel_error=np.nan; ode_nfev=0
        if regime=='free':
            # Independent high-order adaptive solver, no floor or collisions.
            shape=xyz[0].shape; n=xyz[0].size
            def rhs(_,y):
                force,_,_=force_fn(y[:n].reshape(shape))
                return np.r_[y[n:],(force/mass[:,None]).ravel()]
            ts=np.array([g.time.iloc[0] for g in frames]); initial=np.r_[xyz[0].ravel(),vel[0].ravel()]
            result=solve_ivp(rhs,(0,ts[-1]),initial,t_eval=ts,method='DOP853',rtol=1e-10,atol=1e-12)
            if not result.success or result.y.shape[1]!=121: raise ValueError('DOP853 failed')
            ode_error=float(np.max(np.abs(result.y[:n].T.reshape(xyz.shape)-xyz)))
            ode_vel_error=float(np.max(np.abs(result.y[n:].T.reshape(vel.shape)-vel)));ode_nfev=result.nfev
        rows.append(dict(size=size,regime=regime,steps=120,nodes=len(rest),tets=len(cells),candidate_ids=candidate_count,max_force_error_N=force_err,max_step_position_error_m=step_err,max_step_velocity_error_mps=velocity_err,max_impulse_error_Ns=imp_err,max_energy_error_J=energy_err,dop853_position_error_m=ode_error,dop853_velocity_error_mps=ode_vel_error,dop853_nfev=ode_nfev))
        print(rows[-1],flush=True)
    pd.DataFrame(rows).to_csv(output,index=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();run(a.source,a.output)
