"""Fresh FEM with whole-mesh gate; matched original schedule and topology controls."""
import json,sys,time
from pathlib import Path
import numpy as np
from pass49_adaptive import Model,Adaptive,same
from pass45_transaction import RejectedTrial
from pass50_mesh_gate import MeshGate

def oriented(x,ids):
    ids=list(ids)
    if np.linalg.det((x[ids[1:]]-x[ids[0]]).T)<0:ids[2],ids[3]=ids[3],ids[2]
    return ids

def controls():
    x=np.vstack((np.zeros(3),np.eye(3)));rows=[]
    for shared in (0,1,2,3):
        # Two positive-volume tetrahedra: shared simplex allowed, excess overlap forbidden.
        a=list(range(4)); b=list(range(shared))+list(range(4,8-shared)); extra=x[shared:]*.3+.07
        y=np.vstack((x,extra));t=np.array([a,oriented(y,b)]);g=MeshGate(t);r=g.check(y,y);assert r['unresolved']==[[0,1,shared]],r;rows.append(dict(shared=shared,overlap_rejected=True))
        if shared==0:
            y=np.vstack((x,x+[3,0,0]));t=np.array([a,oriented(y,b)]);g=MeshGate(t);assert not g.check(y,y)['unresolved'];z=y.copy();z[4:]-=[6,0,0];assert g.check(y,z)['unresolved'];rows[-1]['tunneling_rejected']=True
        else:
            # Reflect the non-shared vertices across a plane through the common simplex.
            extra=x[shared:].copy()
            if shared==1:extra=-extra
            if shared==2:extra[:,1:]*=-1
            if shared==3:extra[:,2]*=-1
            y=np.vstack((x,extra));t=np.array([a,oriented(y,b)]);g=MeshGate(t);assert not g.check(y,y)['unresolved'];rows[-1]['legitimate_incidence_accepted']=True
    return rows

class WholeGate:
    def __init__(self,old,t):self.old=old;self.mesh=MeshGate(t);self.rows=[]
    def _check(self,x,y,n):
        self.old._check(x,y,n);r=self.mesh.check(x,y);self.rows.append(r)
        if r['unresolved']:raise RejectedTrial('unresolved whole-mesh pair')

def run(out):
    out.mkdir(exist_ok=False);c=controls();print(c,flush=True);summary=[]
    for sid in (0,2):
        m=Model(sid);a=Adaptive(m)
        for _ in range(4):assert a.advance(.004,max_move=.0032)[0]
        baseline=list(a.trace);gate=WholeGate(m.gate,m.t);m.gate=gate;b=Adaptive(m)
        for expected,r in baseline:
            from fractions import Fraction
            ok,att,err=b.advance(float(Fraction(r['dt'])),max_depth=0);assert ok,(sid,err,att);assert same(expected,b.state)
        result=dict(scene=sid,leaves=len(b.trace),bitwise_parity=True,checks=gate.rows);summary.append(result);print(sid,len(b.trace),sum(x['total_pairs'] for x in gate.rows),flush=True)
        np.savez_compressed(out/f'scene{sid}.npz',states=np.array([m.initial.x,*[s.x for s,_ in b.trace]]),cells=m.t)
    (out/'summary.json').write_text(json.dumps(dict(controls=c,scenes=summary),indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]))
