"""Balanced fresh-solver timing, with identical physical schedule and state parity.
Includes gate construction and all trial solves, excludes common model setup.
Whole-mesh arm has stronger safety scope, not an equivalent optimization.
"""
import csv,json,math,time,sys,platform
from pathlib import Path
import numpy as np
from pass49_adaptive import Model,Adaptive,same
from pass42_physical_replay import regions
from pass45_transaction import Gate
from pass48_integer_region import IntegerRegionGate
from pass50_live import WholeGate

def run(out):
 out.mkdir(exist_ok=False);rows=[];summary=[];names=['fraction','integer','whole_mesh']
 for sid in (0,2):
  m=Model(sid);g=regions(m.initial.x,m.t);ref=None
  def arm(i):
   start=time.perf_counter_ns();base=(Gate if i==0 else IntegerRegionGate)(m.initial.x,m.initial.v,m.t,g)
   m.gate=WholeGate(base,m.t) if i==2 else base
   a=Adaptive(m)
   for _ in range(4):
    ok,att,err=a.advance(.004,max_depth=0);assert ok,(sid,i,err)
   ns=time.perf_counter_ns()-start
   return a,ns
  # Paid warmup per arm, then each arm occupies each execution slot once.
  for i in range(3):
   a,_=arm(i)
   if ref is None:ref=a.trace
   assert all(same(s,t) and r==q for (s,r),(t,q) in zip(a.trace,ref))
  for block in range(3):
   for slot in range(3):
    i=(block+slot)%3;a,ns=arm(i);assert len(a.trace)==len(ref) and all(same(s,t) and r==q for (s,r),(t,q) in zip(a.trace,ref));rows.append(dict(scene=sid,block=block,slot=slot,arm=names[i],ns=ns,steps=len(a.trace)));print(rows[-1],flush=True)
  ratios={}
  for target in names[1:]:
   vals=[]
   for block in range(3):
    r={x['arm']:x['ns'] for x in rows if x['scene']==sid and x['block']==block};vals.append(r['fraction']/r[target])
   ratios[target]=dict(geomean=math.exp(sum(map(math.log,vals))/len(vals)),min=min(vals),max=max(vals),ratios=vals)
  summary.append(dict(scene=sid,ratios=ratios));print(summary[-1],flush=True)
  with (out/'timing.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  (out/'summary.json').write_text(json.dumps(dict(scenes=summary,platform=platform.platform(),python=sys.version,scope='3 balanced blocks, 4 fixed 4ms steps per arm, paid gate setup and actual solves; excludes common model setup; whole mesh stronger contract'),indent=2)+'\n')
if __name__=='__main__':run(Path(sys.argv[1]))
