"""Counterbalanced orientation replay, including coordinate conversion."""
import csv,json,math,time,platform,sys
from pathlib import Path
import numpy as np
from pass43_audit import orientation
from pass46_integer_orientation import batch_orientation

def baseline(a,b,t,depth=10):return tuple(orientation(a[c],b[c],depth) for c in t)
def controls():
    rng=np.random.default_rng(462026);t=np.array([[0,1,2,3]]);u=np.vstack((np.zeros(3),np.eye(3)));count=true=0
    cases=[(u,u),(u,u[[0,2,1,3]]),(u,u*np.array([-1,-1,1]))]
    for i in range(600):
        a=rng.normal(size=(4,3));b=a+rng.normal(size=(4,3))*rng.choice([.001,.1,1,3]);scale=2.**int(rng.choice([-500,-100,0,100,500]));cases.append((a*scale,b*scale))
    for th in (1.8,2.1,2.4,2.7,3.,3.13):
        c,s=math.cos(th),math.sin(th);cases.append((u,u@np.array([[c,-s,0],[s,c,0],[0,0,1.]]).T))
    tiny=np.nextafter(0.,1.);cases.extend([(u*tiny,u*tiny),(u*2.**900,u*2.**900),(u,u*np.array([tiny,1,1]))])
    for a,b in cases:
        for depth in (0,1,5,10):
            ref=baseline(a,b,t,depth);opt=batch_orientation(a,b,t,depth);assert ref==opt;count+=1;true+=opt[0]
    return dict(controls=count,positive=true,negative=count-true)

def main(root,out):
    out.mkdir(exist_ok=False);ctrl=controls();print(ctrl,flush=True);rows=[];summary=[]
    for p in sorted(root.glob('scene*.npz')):
        d=np.load(p);states=d['states'];t=d['cells'];work=list(zip(states,states[1:]));ref=[baseline(a,b,t) for a,b in work];assert all(all(r) for r in ref)
        for r in range(2):
            for name in (('fraction','integer','integer','fraction') if r==0 else ('integer','fraction','fraction','integer')):
                f=baseline if name=='fraction' else batch_orientation
                start=time.perf_counter_ns();res=[f(a,b,t) for a,b in work];elapsed=time.perf_counter_ns()-start;assert res==ref
                rows.append(dict(scene=p.stem,block=r,arm=name,ns=elapsed,checks=len(work)*len(t)))
        ratios=[]
        for r in range(2):
            sub=[x for x in rows if x['scene']==p.stem and x['block']==r];ratios.append(sum(x['ns'] for x in sub if x['arm']=='fraction')/sum(x['ns'] for x in sub if x['arm']=='integer'))
        stat=dict(scene=p.stem,cells=len(t),steps=len(work),checks=len(work)*len(t),speedup=math.sqrt(math.prod(ratios)),min_ratio=min(ratios),max_ratio=max(ratios));summary.append(stat);print(stat,flush=True)
    with (out/'timing.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
    report=dict(controls=ctrl,scenes=summary,unique_physical_checks=sum(s['checks'] for s in summary),timed_checks=sum(x['checks'] for x in rows),geomean=math.exp(sum(math.log(s['speedup']) for s in summary)/len(summary)),platform=platform.platform(),python=sys.version,numpy=np.__version__,scope='orientation stage only; no whole solver timing')
    (out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
