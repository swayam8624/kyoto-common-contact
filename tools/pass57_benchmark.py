"""Balanced fresh adaptive solves. Compare discrete decisions separately from numerical differences."""
import argparse,json,time,math
from pathlib import Path
from fractions import Fraction as Q
import numpy as np
from pass54_adaptive_discovery import AdaptiveDiscovery,geometry
from pass57_identity import IdentityModel
from pass56_parametric import ParametricModel

def sig(attempts):
    return [[(a['path'],a['dt'],a['status'],[(b['keys'],b['unresolved']) for b in a.get('attempts',[])]) for a in macro] for macro in attempts]

def run(out,kind,blocks,macros,reverse=False):
    out.mkdir(exist_ok=False);x,t,v=geometry(kind,kind in ('vertex','comb120'));rows=[];comparisons=[]
    for block in range(blocks):
        results={}
        for arm in ('fixed','parametric') if (block+int(reverse))%2==0 else ('parametric','fixed'):
            start=time.perf_counter_ns();m=(ParametricModel if arm=='fixed' else IdentityModel)(x,t,v,12);a=AdaptiveDiscovery(m,x,v);attempts=[]
            for k in range(macros):
                ok,r,err=a.advance(.004,max_move=.0032);assert ok,(kind,arm,k,err);attempts.append(r)
            ns=time.perf_counter_ns()-start;assert a.state.time==macros*Q(.004)
            states=np.array([x,*[s.x for s,r in a.trace]]);vel=np.array([v,*[s.v for s,r in a.trace]]);assert all(r['certified'] and not r['attempts'][-1]['unresolved'] for _,r in a.trace)
            row=dict(arm=arm,block=block,ns=ns,leaves=len(a.trace),pairs=sum(r['pair_checks'] for _,r in a.trace),**m.models.stats());rows.append(row);print(row,flush=True)
            results[arm]=(states,vel,sig(attempts),[r['dt'] for _,r in a.trace]);np.savez_compressed(out/f'{block}_{arm}.npz',states=states,velocities=vel,cells=t);(out/f'{block}_{arm}.json').write_text(json.dumps(attempts,indent=2));(out/'rows.json').write_text(json.dumps(rows,indent=2))
        s,w,k,d=results['fixed'];a,b,l,e=results['parametric'];same=d==e;decision=k==l
        comp=dict(block=block,same_schedule=same,decision_parity=decision,max_position_difference_m=float(np.max(np.abs(s-a))) if same else float(np.max(np.abs(s[-1]-a[-1]))),max_velocity_difference_m_s=float(np.max(np.abs(w-b))) if same else float(np.max(np.abs(w[-1]-b[-1]))));comparisons.append(comp);print(comp,flush=True)
        ratios=[next(r['ns'] for r in rows if r['block']==i and r['arm']=='fixed')/next(r['ns'] for r in rows if r['block']==i and r['arm']=='parametric') for i in range(block+1)]
        (out/'summary.json').write_text(json.dumps(dict(kind=kind,macros=macros,ratios=ratios,geomean=math.exp(sum(map(math.log,ratios))/len(ratios)),comparisons=comparisons,rows=rows),indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('kind');p.add_argument('--blocks',type=int,default=2);p.add_argument('--macros',type=int,default=5);p.add_argument('--reverse',action='store_true');a=p.parse_args();run(a.out,a.kind,a.blocks,a.macros,a.reverse)
