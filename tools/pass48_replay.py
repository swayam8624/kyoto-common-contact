"""Matched rational versus integer witness and safety-check replay."""
import csv,json,math,sys,time
from fractions import Fraction as Q
from itertools import product
from pathlib import Path
import numpy as np
from pass42_region_certificate import rational_points,endpoint_gap,axis_witness
from pass46_gate_replay import IntegerGate
from pass48_integer_region import IntegerRegionGate,witness

def ref(a0,a1,b0,b1,axis,margin):
    q=tuple(Q(float(x)) for x in axis);n=sum(x*x for x in q)
    g=min(endpoint_gap(rational_points(a0),rational_points(b0),q),endpoint_gap(rational_points(a1),rational_points(b1),q))
    return bool(n and g>0 and g*g>Q(float(margin))**2*n)

def controls():
    rng=np.random.default_rng(4801);u=np.array(list(product([0.,1.],repeat=3)));cases=[]
    for i in range(600):
        a=rng.normal(size=(8,3));b=a+rng.normal(size=(8,3));c=rng.normal(size=(5,3))+rng.uniform(-4,4,3);d=c+rng.normal(size=(5,3));q=rng.normal(size=3);s=2.**int(rng.choice([-500,0,500]));cases.append((a*s,b*s,c*s,d*s,q,abs(rng.normal())*s))
    for s in [np.nextafter(0.,1.),2.**-500,1.,2.**500,2.**900]:
        for m in [0.,s,np.nextafter(s,0.)]:
            for q in [np.array([1.,0.,0.]),np.array([2.**900,0.,0.]),np.array([np.nextafter(0.,1.),0.,0.]),np.zeros(3)]:cases.append((u*s,u*s,(u+[2,0,0])*s,(u+[2,0,0])*s,q,m))
    for q in [np.array([1.,0.,0.]),np.array([-1.,0.,0.])]:cases.append((u,u,u+[2,0,0],u+[-2,0,0],q,0.))
    yes=0
    for c in cases:
        a=ref(*c);b=witness(*c);assert a==b;yes+=b
    return dict(total=len(cases),accepted=yes,rejected_or_unknown=len(cases)-yes,mismatch=0)

def main(root,out):
    out.mkdir(exist_ok=False);rows=[];ctrl=controls();print(ctrl,flush=True);results=[];all_w=[]
    for sid,folder in [(0,'run'),(2,'comb_enabled'),(4,'comb_static')]:
        d=np.load(root/folder/f'scene{sid}.npz');s=d['states'];t=d['cells'];g=json.loads((root/folder/f'regions{sid}.json').read_text());gates=[IntegerGate(s[0],np.zeros_like(s[0]),t,g),IntegerRegionGate(s[0],np.zeros_like(s[0]),t,g)];pairs=gates[0]._pairs
        work=[(a,b,np.array([axis_witness(a[l],a[r]) for l,r in pairs])) for a,b in zip(s,s[1:])]
        w=[(a[l],b[l],a[r],b[r],h[k],.0002) for a,b,h in work for k,(l,r) in enumerate(pairs)];all_w.extend(w)
        assert all(ref(*c)==witness(*c) for c in w)
        for scope in ['witness','gate']:
            ratios=[]
            for block,order in enumerate([(0,1,1,0),(1,0,0,1)]):
                times=[0,0]
                for arm in order:
                    start=time.perf_counter_ns()
                    if scope=='witness':
                        fun=[ref,witness][arm];answer=[fun(*c) for c in w]
                    else:
                        for a,b,h in work:gates[arm]._check(a,b,h)
                    elapsed=time.perf_counter_ns()-start;times[arm]+=elapsed;rows.append(dict(scene=sid,scope=scope,block=block,arm=arm,ns=elapsed,checks=len(w) if scope=='witness' else len(work)))
                ratios.append(times[0]/times[1])
            results.append(dict(scene=sid,scope=scope,speedup=math.sqrt(math.prod(ratios)),checks=len(w) if scope=='witness' else len(work)))
        print(results[-2:],flush=True)
    with (out/'timing.csv').open('w',newline='') as f:wr=csv.DictWriter(f,fieldnames=rows[0]);wr.writeheader();wr.writerows(rows)
    summary=dict(controls=ctrl,physical_witnesses=len(all_w),physical_witnesses_accepted=sum(witness(*c) for c in all_w),results=results,geomean={scope:math.exp(sum(math.log(x['speedup']) for x in results if x['scope']==scope)/3) for scope in ['witness','gate']})
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(summary,flush=True)
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
