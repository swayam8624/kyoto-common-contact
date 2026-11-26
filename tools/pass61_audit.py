"""Fail closed on missing held-out frames, unbalanced timing slots or count drift."""
import argparse,csv,json
from collections import defaultdict
from pathlib import Path
from pass59_analyze import ARMS
def run(path,n):
    groups=defaultdict(list);frames={}
    with path.open() as f:
        for r in csv.DictReader(f):
            k=(r['scene'],r['regime'],int(r['leaf']));b=int(r['block']);s=int(r['slot']);a=r['arm']
            assert 0<=b<n and 0<=s<n and a==ARMS[(b+s)%n] and int(r['ns'])>0
            meta=tuple(int(r[i]) for i in ('candidates','vertices','edges','faces'))
            assert min(meta)>=0 and frames.setdefault(k,meta)==meta
            groups[(*k,b)].append((s,a))
    expected={(f'heldout_{seed}',mode,leaf) for seed in (61057,61069,61091,61121) for mode in ('sparse','dense_masks','topology_shear','large_offset','subnormal') for leaf in range(6)}
    assert set(frames)==expected and len(groups)==120*n
    for g in groups.values():assert len(g)==n and {s for s,a in g}==set(range(n))
    return dict(frames=len(frames),arms=n,groups=len(groups),rows=len(groups)*n,max_candidates=max(v[0] for v in frames.values()),zero_candidate_frames=sum(v[0]==0 for v in frames.values()),count_parity=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('csv',type=Path);p.add_argument('--arms',type=int,choices=(11,12),required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();r=run(a.csv,a.arms);a.out.write_text(json.dumps(r,indent=2)+'\n');print(r)
