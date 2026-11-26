"""Multiple simultaneous contacts, stationary and exhausted-budget controls."""
import json,sys
from pathlib import Path
import numpy as np
from pass53_discovery import Model,fixture
from pass45_transaction import RejectedTrial
from pass50_edge_response import overlap

def run(out):
    out.mkdir(exist_ok=False);results=[]
    for rot in (False,True):
        for enabled in (True,False):
            a=np.vstack((np.zeros(3),np.eye(3)))*.02;x=np.vstack((a,a+[.035,0,0],a+[.07,0,0]));t=np.arange(12).reshape(3,4);v=np.zeros_like(x);v[:4,0]=1.2;v[8:,0]=-1.2
            if rot:
                R=np.array([[.48,-.6,-.64],[.36,.8,-.48],[.8,0,.6]]);x=x@R.T;v=v@R.T
            m=Model(x,t,v);rows=[];states=[x.copy()];rejected=0
            for k in range(40):
                y,vn,r=m.step(x,v,enabled);r['overlap']=None
                if not r['certified']:
                    for i,j,_ in r['attempts'][-1]['unresolved']:
                        p=overlap(y[t[i]],y[t[j]])
                        if p is not None:r['overlap']=[i,j,p];break
                if enabled and len(r['attempts'])>1:
                    before=x.copy(),v.copy()
                    try:m.step(x,v,True,budget=0)
                    except RejectedTrial:rejected+=1
                    else:raise AssertionError('exhausted budget accepted')
                    assert np.array_equal(x,before[0]) and np.array_equal(v,before[1])
                rows.append(r);states.append(y.copy());x,v=y,vn
            summary=dict(rotated=rot,enabled=enabled,steps=40,certified=sum(r['certified'] for r in rows),overlap_states=sum(r['overlap'] is not None for r in rows),simultaneous_steps=sum(len(r['attempts'][-1]['keys'])>=2 for r in rows),budget_rejections=rejected,active_steps=sum(r['impulse']>1e-7 for r in rows));print(summary,flush=True);results.append(summary)
            if enabled:assert summary['certified']==40 and summary['simultaneous_steps']>0 and rejected>0
            else:assert summary['overlap_states']>0
            name=f'triple_r{int(rot)}_e{int(enabled)}';np.savez_compressed(out/(name+'.npz'),states=states,cells=t);(out/(name+'.json')).write_text(json.dumps(rows,indent=2)+'\n')
    for kind in ('edge','vertex','disjoint'):
        x,t,v=fixture(kind,False);v[:]=0;m=Model(x,t,v);rows=[];initial=x.copy()
        for i in range(10):
            x,v,r=m.step(x,v,True);assert r['certified'] and len(r['attempts'])==1 and abs(r['impulse'])<1e-12;rows.append(r)
        assert np.max(np.abs(x-initial))<1e-8
        results.append(dict(stationary=kind,steps=10,certified=10,active_steps=0));(out/(kind+'_stationary.json')).write_text(json.dumps(rows,indent=2)+'\n')
    (out/'summary.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]))
