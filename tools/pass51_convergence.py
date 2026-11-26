"""Timestep refinement against finest numerical solution, not analytic error.
Same physical model, exact final time, all accepted leaves certified.
"""
import json,sys
from pathlib import Path
from fractions import Fraction
import numpy as np
from pass49_adaptive import Model,Adaptive

def run(out):
 out.mkdir(exist_ok=False);rows=[]
 for sid in (0,2):
  model=Model(sid);series=[]
  for n in (4,8,16,32):
   dt=.016/n;a=Adaptive(model)
   for k in range(n):
    ok,att,err=a.advance(dt,max_depth=0);assert ok,(sid,n,k,err)
   assert a.state.clock==Fraction(.016)
   states=np.array([model.initial.x,*[s.x for s,_ in a.trace]]);vel=np.array([model.initial.v,*[s.v for s,_ in a.trace]])
   np.savez_compressed(out/f'scene{sid}_n{n}.npz',states=states,velocities=vel,cells=model.t)
   row=dict(scene=sid,steps=n,dt=dt,contact_steps=sum(r['impulse']>1e-7 for _,r in a.trace),min_j=min(r['min_j'] for _,r in a.trace),min_gap=min(r['clearance'] for _,r in a.trace));series.append((row,states,vel));print(row,flush=True)
  _,ref,rv=series[-1]
  for row,s,v in series:
   stride=(len(ref)-1)//row['steps'];d=s-ref[::stride];dv=v-rv[::stride];row.update(final_rms_position_m=float(np.sqrt(np.mean(d[-1]**2))),trajectory_rms_position_m=float(np.sqrt(np.mean(d**2))),final_rms_velocity_m_s=float(np.sqrt(np.mean(dv[-1]**2))));rows.append(row)
  (out/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]))
