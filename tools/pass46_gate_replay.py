"""Paid gate validation replay on stored enabled physical states.
This measures _check, not integration or commit. Same hints and exact
region fallback in both arms; only the orientation implementation changes.
"""
import csv,json,math,time,sys
from pathlib import Path
from fractions import Fraction
import numpy as np
from pass45_transaction import Gate,RejectedTrial
from pass46_integer_orientation import batch_orientation
from pass42_region_certificate import axis_witness, rational_points, endpoint_gap, certify

class IntegerGate(Gate):
    def _check(self,x,y,hints):
        if not np.isfinite(y).all():raise RejectedTrial('nonfinite position')
        if not all(batch_orientation(x,y,self._cells)):raise RejectedTrial('unresolved continuous orientation')
        for k,(a,b) in enumerate(self._pairs):
            if hints is not None:
                q=tuple(Fraction(float(z)) for z in hints[k]);norm2=sum(z*z for z in q)
                gap=min(endpoint_gap(rational_points(x[a]),rational_points(x[b]),q),endpoint_gap(rational_points(y[a]),rational_points(y[b]),q))
                if norm2 and gap>0 and gap*gap>Fraction(self._margin)**2*norm2:continue
            if not certify(x[a],y[a],x[b],y[b],self._margin,max_depth=8)[0]:raise RejectedTrial('unresolved disjoint-region path')

def main(root,out):
    rows=[];stats=[]
    for sid,folder in [(0,'run'),(2,'comb_enabled'),(4,'comb_static')]:
        d=np.load(root/folder/f'scene{sid}.npz');s=d['states'];t=d['cells'];g=json.loads((root/folder/f'regions{sid}.json').read_text());old=Gate(s[0],np.zeros_like(s[0]),t,g);new=IntegerGate(s[0],np.zeros_like(s[0]),t,g)
        work=[(a,b,np.array([axis_witness(a[l],a[r]) for l,r in old._pairs])) for a,b in zip(s,s[1:])]
        for r in range(2):
            for name in (('fraction','integer','integer','fraction') if r==0 else ('integer','fraction','fraction','integer')):
                gate=old if name=='fraction' else new
                start=time.perf_counter_ns()
                for a,b,h in work:gate._check(a,b,h)
                rows.append(dict(scene=sid,block=r,arm=name,ns=time.perf_counter_ns()-start,steps=len(work)))
        ratios=[]
        for r in range(2):
            sub=[x for x in rows if x['scene']==sid and x['block']==r];ratios.append(sum(x['ns'] for x in sub if x['arm']=='fraction')/sum(x['ns'] for x in sub if x['arm']=='integer'))
        stats.append(dict(scene=sid,steps=len(work),pairs=len(old._pairs),speedup=math.sqrt(math.prod(ratios)),min_ratio=min(ratios),max_ratio=max(ratios)));print(stats[-1],flush=True)
    # Verify real transaction rejections and preservation of committed state.
    a=np.vstack((np.zeros(3),np.eye(3)));x=np.vstack((a,a+[3,0,0]));t=np.array([[0,1,2,3],[4,5,6,7]]);r=[dict(cells=[0],nodes=[0,1,2,3]),dict(cells=[1],nodes=[4,5,6,7])];v=np.zeros_like(x)
    from pass45_transaction import reject
    g=IntegerGate(x,v,t,r);tunnel=v.copy();tunnel[4:,0]=-6;inv=v.copy();inv[1,0]=-2
    reject(g,tunnel,1.,0,'region path');reject(g,inv,1.,0,'orientation');reject(g,np.full_like(v,np.nan),1.,0,'invalid trial');reject(g,v,0.,0,'invalid trial');reject(g,v,1.,1,'stale version');assert g.commit(v,.001,0).version==1;reject(g,v,.001,0,'stale version');assert g.commit(v,.001,1).time==2*Fraction(.001)
    with (out/'gate_timing.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
    report=dict(scenes=stats,steps=sum(s['steps'] for s in stats),geomean=math.exp(sum(math.log(s['speedup']) for s in stats)/len(stats)),rejected_controls=6,valid_commits=2,scope='gate _check replay only; excludes generation of shared hints and physical integration')
    (out/'gate_summary.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
