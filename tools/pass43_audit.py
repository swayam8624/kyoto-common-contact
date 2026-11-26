"""Independent exact continuous orientation and exhaustive/SAP candidate parity.
The orientation gate does not prove global injectivity. SAP checks swept AABB
candidates, not true contact. Incident pairs are explicitly excluded.
"""
import csv,json,sys
from pathlib import Path
from fractions import Fraction as Q
from itertools import combinations
import numpy as np

def det(a,b,c):
    return a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
def columns(x):
    a=[tuple(Q(float(y)) for y in r) for r in x]
    return [tuple(i-j for i,j in zip(a[k],a[0])) for k in (1,2,3)]
def orientation(a,b,depth=10):
    # 3 times cubic Bernstein coefficients of det((1-t)A+tB).
    u,v,w=columns(a);p,q,r=columns(b)
    c=(3*det(u,v,w),det(p,v,w)+det(u,q,w)+det(u,v,r),det(u,q,r)+det(p,v,r)+det(p,q,w),3*det(p,q,r))
    def bound(c,d):
        if min(c)>0:return True
        if min(c[0],c[3])<=0 or d==0:return False
        a,b,e=[(c[i]+c[i+1])/2 for i in range(3)];f,g=(a+b)/2,(b+e)/2;h=(f+g)/2
        return bound((c[0],a,f,h),d-1) and bound((h,g,e,c[3]),d-1)
    return bound(c,depth)

def candidates(a,b,t,skin):
    lo=np.minimum(a[t].min(1),b[t].min(1))-skin;hi=np.maximum(a[t].max(1),b[t].max(1))+skin
    sets=[set(c) for c in t];eligible=[(i,j) for i,j in combinations(range(len(t)),2) if not sets[i]&sets[j]]
    def hit(i,j):return bool(np.all(lo[i]<=hi[j]) and np.all(lo[j]<=hi[i]))
    oracle={p for p in eligible if hit(*p)};active=[];sap=set()
    for i in sorted(range(len(t)),key=lambda i:(lo[i,0],i)):
        active=[j for j in active if hi[j,0]>=lo[i,0]]
        for j in active:
            if not sets[i]&sets[j] and hit(i,j):sap.add(tuple(sorted((i,j))))
        active.append(i)
    assert oracle==sap
    return len(eligible),len(sap)

def main(path):
    unit=np.vstack((np.zeros(3),np.eye(3)));other=unit.copy();other[1:]*=[-1,-1,1]
    assert orientation(unit,unit) and not orientation(unit,other) # positive endpoints; zero determinant midpath
    assert not orientation(unit,unit[[0,2,1,3]])
    results=[]
    with (path/'audit.csv').open('w',newline='') as f:
        w=csv.writer(f);w.writerow(['scene','step','exact_orientation_pass','orientation_unresolved','eligible_pairs','zero_skin_candidates','half_mm_skin_candidates'])
        for file in sorted(path.glob('scene*.npz')):
            sid=int(file.stem[5:]);d=np.load(file);t=d['cells'];s=d['states'];total=0;unresolved=0;counts=0;zero=0;skin=0
            for step,(a,b) in enumerate(zip(s,s[1:])):
                ok=sum(orientation(a[c],b[c]) for c in t);n,z=candidates(a,b,t,0.);_,k=candidates(a,b,t,.0005)
                total+=ok;unresolved+=len(t)-ok;counts+=n*2;zero+=z;skin+=k;w.writerow([sid,step,ok,len(t)-ok,n,z,k]);f.flush()
            r=dict(scene=sid,orientation_certified=total,orientation_unresolved=unresolved,exhaustive_pair_checks=counts,zero_skin_candidates=zero,half_mm_skin_candidates=skin);results.append(r);print(r,flush=True)
    (path/'audit_summary.json').write_text(json.dumps(results,indent=2)+'\n')
    assert all(r['orientation_unresolved']==0 for r in results)
if __name__=='__main__':main(Path(sys.argv[1]))
