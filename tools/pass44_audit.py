"""Exact orientation and conservative swept AABB candidate-set audit on archived physical paths."""
import csv,json,sys
from pathlib import Path
import numpy as np
from pass43_audit import orientation

def audit(path):
    records=[]
    with (path/'audit.csv').open('w',newline='') as f:
        w=csv.writer(f);w.writerow(['scene','step','orientation_certified','orientation_unresolved','eligible_pair_checks','zero_skin','half_mm_skin'])
        for file in sorted(path.glob('scene*.npz')):
            sid=int(file.stem[5:]);d=np.load(file);t=d['cells'];s=d['states'];n=len(t)
            incident=np.any(t[:,None,:,None]==t[None,:,None,:],axis=(2,3));eligible=np.triu(~incident,1);count=int(eligible.sum());tot=np.zeros(5,dtype=np.int64)
            for step,(a,b) in enumerate(zip(s,s[1:])):
                ok=sum(orientation(a[c],b[c]) for c in t);candidates=[]
                for skin in (0.,.0005):
                    # Round outward even after padding to avoid a missed near-boundary pair.
                    lo=np.nextafter(np.minimum(a[t].min(1),b[t].min(1))-skin,-np.inf);hi=np.nextafter(np.maximum(a[t].max(1),b[t].max(1))+skin,np.inf)
                    oracle=set(zip(*np.where(eligible & np.all(lo[:,None]<=hi[None,:],2)&np.all(lo[None,:]<=hi[:,None],2))))
                    active=[];sap=set()
                    for i in sorted(range(n),key=lambda i:(lo[i,0],i)):
                        active=[j for j in active if hi[j,0]>=lo[i,0]]
                        for j in active:
                            if not incident[i,j] and np.all(lo[i]<=hi[j]) and np.all(lo[j]<=hi[i]):sap.add(tuple(sorted((i,j))))
                        active.append(i)
                    assert oracle==sap,(sid,step,skin)
                    candidates.append(len(sap))
                row=[ok,n-ok,2*count,*candidates];tot+=row;w.writerow([sid,step,*row])
            r=dict(scene=sid,steps=len(s)-1,cells=n,orientation_certified=int(tot[0]),orientation_unresolved=int(tot[1]),eligible_pair_checks=int(tot[2]),zero_skin_candidates=int(tot[3]),half_mm_skin_candidates=int(tot[4]));records.append(r);print(r,flush=True)
    (path/'audit_summary.json').write_text(json.dumps(records,indent=2)+'\n');assert all(r['orientation_unresolved']==0 for r in records)
if __name__=='__main__':audit(Path(sys.argv[1]))
