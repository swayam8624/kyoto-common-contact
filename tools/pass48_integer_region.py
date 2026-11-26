"""Exact dyadic witness for binary64 linear paths. For coordinates X/2**kx,
axis W/2**kw, and margin M/2**km, the minimum integer endpoint gap G
must be positive and G**2 * 2**(2*km) > M**2 * ||W||**2 * 2**(2*kx).
Failure is unknown; retain the original recursive exact fallback.
"""
import numpy as np
from pass42_region_certificate import certify
from pass45_transaction import Gate,RejectedTrial
from pass46_integer_orientation import batch_orientation

def scaled(values):
    ratios=[float(x).as_integer_ratio() for x in values]
    k=max((d.bit_length()-1 for _,d in ratios),default=0)
    return [n<<(k-(d.bit_length()-1)) for n,d in ratios],k

def witness(a0,a1,b0,b1,axis,margin):
    arr=[np.asarray(x,dtype=float) for x in (a0,a1,b0,b1)]
    q=np.asarray(axis,dtype=float)
    if any(x.ndim!=2 or x.shape[1]!=3 or not len(x) or not np.isfinite(x).all() for x in arr) or arr[0].shape!=arr[1].shape or arr[2].shape!=arr[3].shape:raise ValueError('invalid geometry')
    if q.shape!=(3,) or not np.isfinite(q).all() or not np.isfinite(margin) or margin<0:raise ValueError('invalid witness')
    xyz,k=scaled(np.concatenate(arr).ravel());w,_=scaled(q)
    norm=sum(x*x for x in w)
    if not norm:return False
    offsets=np.cumsum([0,*[len(x) for x in arr]])
    dots=[sum(x*y for x,y in zip(xyz[i:i+3],w)) for i in range(0,len(xyz),3)]
    p=[dots[offsets[i]:offsets[i+1]] for i in range(4)]
    gap=min(min(p[2])-max(p[0]),min(p[3])-max(p[1]))
    if gap<=0:return False
    m,d=float(margin).as_integer_ratio();km=d.bit_length()-1
    return (gap*gap << (2*km)) > (m*m*norm << (2*k))

class IntegerRegionGate(Gate):
    def _check(self,x,y,hints):
        if not np.isfinite(y).all():raise RejectedTrial('nonfinite position')
        if not all(batch_orientation(x,y,self._cells)):raise RejectedTrial('unresolved continuous orientation')
        for k,(a,b) in enumerate(self._pairs):
            if hints is not None and witness(x[a],y[a],x[b],y[b],hints[k],self._margin):continue
            if not certify(x[a],y[a],x[b],y[b],self._margin,max_depth=8)[0]:raise RejectedTrial('unresolved disjoint-region path')
