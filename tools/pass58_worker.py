"""One fresh process per measured run. RSS is OS high-water, not cache entry count."""
import argparse,json,time,resource,sys
from pathlib import Path
from fractions import Fraction as Q
import numpy as np
from pass54_adaptive_discovery import AdaptiveDiscovery,geometry
from pass56_parametric import ParametricModel
from pass58_sparse import SparseModel

def run(out,kind,arm,macros):
    out.mkdir(exist_ok=False);x,t,v=geometry(kind,kind=='comb120');start=time.perf_counter_ns();m=(ParametricModel if arm=='base' else SparseModel)(x,t,v,12);a=AdaptiveDiscovery(m,x,v);attempts=[]
    for i in range(macros):
        ok,r,err=a.advance(.004,max_move=.0032);assert ok,(kind,arm,i,err);attempts.append(r);print(kind,arm,i,flush=True)
    elapsed=time.perf_counter_ns()-start;rss=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    # Linux reports KiB, Darwin bytes. Includes interpreter and native solver allocations.
    rss_bytes=rss if sys.platform=='darwin' else rss*1024
    assert a.state.time==macros*Q(.004) and all(r['certified'] and not r['attempts'][-1]['unresolved'] for _,r in a.trace)
    row=dict(kind=kind,arm=arm,macros=macros,cells=len(t),ns=elapsed,peak_rss_bytes=rss_bytes,leaves=len(a.trace),pairs=sum(r['pair_checks'] for _,r in a.trace),max_kkt=max(r['kkt'] for _,r in a.trace),**m.models.stats())
    np.savez_compressed(out/'trajectory.npz',states=[x,*[s.x for s,_ in a.trace]],velocities=[v,*[s.v for s,_ in a.trace]],cells=t);(out/'attempts.json').write_text(json.dumps(attempts,indent=2));(out/'summary.json').write_text(json.dumps(row,indent=2));print(row,flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('kind');p.add_argument('arm',choices=['base','sparse']);p.add_argument('--macros',type=int,default=4);a=p.parse_args();run(a.out,a.kind,a.arm,a.macros)
