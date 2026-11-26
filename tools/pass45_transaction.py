"""Fail-closed commit of a single independent FEM trial, in declared scope.

All nodes follow the stored linear path. Check exact positive orientation
and separation of *all declared disjoint region pairs* before publishing
position, velocity, version, time and trace together. This is not a proof
of arbitrary same-body injectivity or a general contact response solver.
"""
from dataclasses import dataclass
from fractions import Fraction
from itertools import combinations
import numpy as np
from pass43_audit import orientation
from pass42_region_certificate import certify, rational_points, endpoint_gap

class RejectedTrial(RuntimeError):
    pass

@dataclass(frozen=True)
class Receipt:
    version:int
    time:Fraction
    cells:int
    pairs:int

class Gate:
    def __init__(self,x,v,cells,regions,margin=.0002):
        self._x=np.array(x,dtype=float,copy=True);self._v=np.array(v,dtype=float,copy=True)
        if self._x.ndim!=2 or self._x.shape[1]!=3 or self._v.shape!=self._x.shape or not np.isfinite(self._x).all() or not np.isfinite(self._v).all():raise ValueError('invalid initial state')
        self._cells=np.array(cells,dtype=int,copy=True)
        if self._cells.ndim!=2 or self._cells.shape[1]!=4 or len(self._cells)==0 or self._cells.min()<0 or self._cells.max()>=len(self._x):raise ValueError('invalid topology')
        seen=[];nodes=[]
        for r in regions:
            c=list(r['cells']);n=list(r['nodes'])
            if not c or min(c)<0 or max(c)>=len(self._cells) or len(set(n))!=len(n) or set(n)!=set(self._cells[c].ravel()):raise ValueError('invalid region')
            seen.extend(c);nodes.append(tuple(sorted(n)))
        if sorted(seen)!=list(range(len(self._cells))):raise ValueError('nonpartition')
        self._pairs=[(np.array(a),np.array(b)) for a,b in combinations(nodes,2) if not set(a)&set(b)]
        if not np.isfinite(margin) or margin<0:raise ValueError('invalid margin')
        self._margin=float(margin);self._version=0;self._time=Fraction(0);self._trace=[]
        self._check(self._x,self._x,None)
    @property
    def version(self):return self._version
    @property
    def time(self):return self._time
    def snapshot(self):return self._x.copy(),self._v.copy(),self._version,self._time,tuple(self._trace)
    def _check(self,x,y,hints):
        if not np.isfinite(y).all():raise RejectedTrial('nonfinite position')
        for c in self._cells:
            if not orientation(x[c],y[c]):raise RejectedTrial('unresolved continuous orientation')
        for k,(a,b) in enumerate(self._pairs):
            if hints is not None:
                q=tuple(Fraction(float(z)) for z in hints[k]);norm2=sum(z*z for z in q)
                gap=min(endpoint_gap(rational_points(x[a]),rational_points(x[b]),q),endpoint_gap(rational_points(y[a]),rational_points(y[b]),q))
                if norm2 and gap>0 and gap*gap>Fraction(self._margin)**2*norm2:continue
            if not certify(x[a],y[a],x[b],y[b],self._margin,max_depth=8)[0]:raise RejectedTrial('unresolved disjoint-region path')
    def commit(self,v,dt,version,hints=None):
        if version!=self._version:raise RejectedTrial('stale version')
        v=np.array(v,dtype=float,copy=True)
        if v.shape!=self._x.shape or not np.isfinite(v).all() or not np.isfinite(dt) or dt<=0:raise RejectedTrial('invalid trial')
        if hints is not None:
            hints=np.array(hints,dtype=float,copy=True)
            if hints.shape!=(len(self._pairs),3) or not np.isfinite(hints).all():raise RejectedTrial('invalid hints')
        y=self._x+float(dt)*v
        self._check(self._x,y,hints)
        # Nothing published until every check succeeded. A failed trial does
        # not alter arrays, version, clock, or the accepted receipt sequence.
        receipt=Receipt(self._version+1,self._time+Fraction(float(dt)),len(self._cells),len(self._pairs))
        self._x,self._v,self._version,self._time,self._trace=y,v,receipt.version,receipt.time,[*self._trace,receipt]
        return receipt

def unchanged(a,b):
    return np.array_equal(a[0],b[0]) and np.array_equal(a[1],b[1]) and a[2:]==b[2:]

def reject(g,v,dt,version,reason):
    before=g.snapshot()
    try:g.commit(v,dt,version)
    except RejectedTrial as e:assert reason in str(e),(reason,str(e))
    else:raise AssertionError('unsafe trial accepted')
    assert unchanged(before,g.snapshot())

if __name__=='__main__':
    # Rigid two-tet tunnelling with separated endpoints; exact orientation
    # stays positive, but disjoint-region continuous separation must fail.
    a=np.vstack((np.zeros(3),np.eye(3)));x=np.vstack((a,a+[3,0,0]));t=np.array([[0,1,2,3],[4,5,6,7]]);r=[dict(cells=[0],nodes=[0,1,2,3]),dict(cells=[1],nodes=[4,5,6,7])];v=np.zeros_like(x);g=Gate(x,v,t,r)
    tunnel=v.copy();tunnel[4:,0]=-6
    reject(g,tunnel,1.,0,'region path')
    inversion=v.copy();inversion[1,0]=-2
    reject(g,inversion,1.,0,'orientation')
    reject(g,np.full_like(v,np.nan),1.,0,'invalid trial')
    reject(g,v,0.,0,'invalid trial')
    reject(g,v,1.,1,'stale version')
    assert g.commit(v,.001,0).version==1
    reject(g,v,.001,0,'stale version')
    assert g.commit(v,.001,1).time==2*Fraction(.001)
    print('PASS six rejected transactions; two valid commits; no mutation or phantom clock advance')
