"""Exercise unmodified large physical timesteps, without fault injection or displacement caps."""
import json,sys
from fractions import Fraction as Q
from pathlib import Path
import numpy as np
from pass49_adaptive import Model,Adaptive,same,State

def run(out):
    out.mkdir(exist_ok=False);rows=[]
    for sid in (0,2):
        m=Model(sid)
        for dt in (.016,.032,.064):
            a=Adaptive(m);ok,attempts,error=a.advance(dt,max_depth=2);row={'scene':sid,'dt':dt,'completed':ok,'attempts':attempts,'error':error,'time':str(a.state.clock)};rows.append(row);print(sid,dt,ok,[x['result'] for x in attempts],flush=True)
            assert a.state.clock==(Q(dt) if ok else Q(0))
            if ok:
                b=Adaptive(m)
                for expected,r in a.trace:
                    done,_,err=b.advance(float(Q(r['dt'])),max_depth=0);assert done,err;assert same(expected,b.state)
                np.savez_compressed(out/f'scene{sid}_{int(dt*1000)}ms.npz',states=np.array([m.initial.x,*[s.x for s,_ in a.trace]]),velocities=np.array([m.initial.v,*[s.v for s,_ in a.trace]]),normals=np.array([m.initial.normals,*[s.normals for s,_ in a.trace]]),cells=m.t)
            if attempts[0]['result']!='accepted':
                control=Adaptive(m);s=control.state;before=State(s.x.copy(),s.v.copy(),s.normals.copy(),s.clock,s.version);trace=control.trace
                done,raw,err=control.advance(dt,max_depth=0);assert not done and raw[0]['result']==attempts[0]['result'];assert same(before,control.state) and control.trace is trace and not trace
                row['zero_budget_rollback']=True
            row['reference_parity']=ok
    (out/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]))
