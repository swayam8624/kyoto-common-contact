"""Exact sufficient pairwise separation up to a common mesh simplex.
LP/SAT only propose witnesses; integer arithmetic certifies full linear paths.
Unresolved is not evidence of collision. No new contact response law."""
import numpy as np
from itertools import combinations
from scipy.optimize import linprog
from pass48_integer_region import scaled
from pass46_integer_orientation import batch_orientation, positive
from pass45_transaction import RejectedTrial
from pass42_region_certificate import certify

def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def det(a,b,c):return dot(a,cross(b,c))
def cubic(a,b,c,p,q,r):return (3*det(a,b,c),det(p,b,c)+det(a,q,c)+det(a,b,r),det(a,q,r)+det(p,b,r)+det(p,q,c),3*det(p,q,r))
def quadratic(a,b,p,q,aux):
    def rec(c,d):
        x,y,z=c
        if min(c)>0:return True
        if min(x,z)<=0 or not d:return False
        return rec((4*x,2*(x+y),x+2*y+z),d-1) and rec((x+2*y+z,2*(y+z),4*z),d-1)
    return rec((2*det(a,aux,b),det(p,aux,b)+det(a,aux,q),2*det(p,aux,q)),8)
def propose(mid,s,l,r):
    o=mid[s[0]];v=np.vstack((o-mid[l],mid[r]-o));scale=np.max(np.linalg.norm(v,axis=1))
    if not scale:return None
    v=v/scale;edge=mid[s[1]]-o if len(s)==2 else None
    eq=np.r_[edge/np.linalg.norm(edge),0.][None,:] if edge is not None and np.linalg.norm(edge)>0 else None
    if edge is not None and eq is None:return None
    sol=linprog([0,0,0,-1],A_ub=np.column_stack((-v,np.ones(len(v)))),b_ub=np.zeros(len(v)),A_eq=eq,b_eq=[0.] if eq is not None else None,bounds=[(-1,1)]*3+[(None,None)],method='highs')
    if not sol.success or sol.x[3]<=0:return None
    return np.cross(sol.x[:3],edge) if edge is not None else sol.x[:3]
EDGES=list(combinations(range(4),2));FACES=list(combinations(range(4),3))
def sat(a,b):
    def edges(x):return np.array([x[j]-x[i] for i,j in EDGES])
    def normals(x):return [np.cross(x[j]-x[i],x[k]-x[i]) for i,j,k in FACES]
    ea,eb=edges(a),edges(b);axes=np.array([*np.eye(3),*normals(a),*normals(b),*np.cross(ea[:,None,:],eb[None,:,:]).reshape(-1,3)])
    norm=np.linalg.norm(axes,axis=1);good=np.isfinite(norm)&(norm>0);axes=axes[good]/norm[good,None];axes=np.vstack((axes,-axes));gap=(b@axes.T).min(axis=0)-(a@axes.T).max(axis=0)
    return axes[int(np.argmax(gap))]
class SweepMeshGate:
    def __init__(self,cells):
        self.t=np.asarray(cells,dtype=int);self.pairs=[];self.cache={}
        if self.t.ndim!=2 or self.t.shape[1]!=4 or not len(self.t) or self.t.min()<0 or any(len(set(c))!=4 for c in self.t):raise ValueError('invalid tetra topology')
        for i,j in combinations(range(len(self.t)),2):
            a,b=set(self.t[i]),set(self.t[j]);s=sorted(a&b)
            if len(s)==4:raise ValueError('duplicate tetra')
            self.pairs.append((i,j,s,sorted(a-b),sorted(b-a)))
        self.incident={(i,j) for i,j,s,l,r in self.pairs if s}
        self.lookup={(i,j):(i,j,s,l,r) for i,j,s,l,r in self.pairs}
    def check(self,a,b):
        a,b=np.asarray(a,dtype=float),np.asarray(b,dtype=float)
        if not all(batch_orientation(a,b,self.t)):raise RejectedTrial('unresolved orientation')
        vals,_=scaled(np.concatenate((a,b)).ravel());pts=[tuple(vals[i:i+3]) for i in range(0,len(vals),3)];A,B=pts[:len(a)],pts[len(a):];mid=a/2+b/2
        lo=np.minimum(a[self.t].min(1),b[self.t].min(1));hi=np.maximum(a[self.t].max(1),b[self.t].max(1))
        # Inclusive sweep; all shared-simplex pairs must be checked regardless of AABB.
        kept=set(self.incident);active=[]
        for i in sorted(range(len(self.t)),key=lambda i:(lo[i,0],i)):
            active=[j for j in active if hi[j,0]>=lo[i,0]]
            for j in active:
                if np.all(hi[j,1:]>=lo[i,1:]) and np.all(hi[i,1:]>=lo[j,1:]):kept.add((min(i,j),max(i,j)))
            active.append(i)
        counts=[0]*4;culled=len(self.pairs)-len(kept);unresolved=[];new=0
        for key in sorted(kept):
            i,j,s,l,r=self.lookup[key];k=len(s)
            key=(i,j)
            def verify(q):
                if q is None:return False
                q=tuple(scaled(q)[0])
                if not any(q):return False
                if k==0:return all(min(dot(P[v],q) for v in r)>max(dot(P[v],q) for v in l) for P in (A,B))
                o=s[0]
                if k==1:return all(all(dot(sub(P[o],P[v]),q)>0 for v in l) and all(dot(sub(P[v],P[o]),q)>0 for v in r) for P in (A,B))
                e,p=sub(A[s[1]],A[o]),sub(B[s[1]],B[o]);return all(quadratic(e,sub(A[o],A[v]),p,sub(B[o],B[v]),q) for v in l) and all(quadratic(e,sub(A[v],A[o]),p,sub(B[v],B[o]),q) for v in r)
            if k==3:
                o,u,v=s;e,f=sub(A[u],A[o]),sub(A[v],A[o]);p,q=sub(B[u],B[o]),sub(B[v],B[o]);c=cubic(e,f,sub(A[l[0]],A[o]),p,q,sub(B[l[0]],B[o]));d=cubic(e,f,sub(A[r[0]],A[o]),p,q,sub(B[r[0]],B[o]));ok=(positive(c,8) and positive(tuple(-x for x in d),8)) or (positive(d,8) and positive(tuple(-x for x in c),8))
            else:
                hint=self.cache.get(key);ok=verify(hint)
                if not ok:
                    hint=sat(mid[l],mid[r]) if k==0 else propose(mid,s,l,r);new+=1;ok=verify(hint)
                    if ok:self.cache[key]=hint
            if not ok and k==0:
                ok=certify(a[l],b[l],a[r],b[r],0.,max_depth=10)[0]
            if ok:counts[k]+=1
            else:unresolved.append([i,j,k])
        return dict(total_pairs=len(self.pairs),aabb_culled=culled,strict_disjoint=counts[0],shared_vertex=counts[1],shared_edge=counts[2],shared_face=counts[3],unresolved=unresolved,proposals=new)
