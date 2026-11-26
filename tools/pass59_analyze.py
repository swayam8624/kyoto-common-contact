"""Strict paid broad-phase replay receipt audit and equal-configuration timing."""
import argparse,csv,json,math,statistics
from collections import defaultdict
from pathlib import Path

ARMS=('exhaustive','rebuilt_sap','rebuilt_bvh','retained_refit','retained_frontier','persistent_x','adaptive_axis','persistent_axis','cached_axis','spatial_first','fused','ipc_lbvh')

def gm(values):return math.exp(statistics.mean(math.log(x) for x in values))

def read(path,n):
    groups=defaultdict(list);frames={};scenes=defaultdict(set);counts=defaultdict(int);totals=defaultdict(int)
    with path.open(newline='') as f:
        for r in csv.DictReader(f):
            scene=(r['scene'],int(r['width']),r['regime']);frame=(*scene,int(r['leaf']));block=int(r['block']);slot=int(r['slot']);arm=r['arm'];ns=int(r['ns']);meta=tuple(int(r[k]) for k in ('candidates','vertices','edges','faces'))
            assert arm in ARMS[:n] and 0<=block<n and 0<=slot<n and ns>0 and min(meta)>=0
            assert arm==ARMS[(slot+block)%n],(frame,block,slot,arm)
            assert frames.setdefault(frame,meta)==meta,(frame,'candidate/geometry count drift')
            groups[(*frame,block)].append((slot,arm));scenes[scene].add(frame[-1]);totals[(scene,block,arm)]+=ns;counts[(scene,block,arm)]+=1
    assert len(scenes)==17 and len(frames)==300 and len(groups)==300*n
    for key,g in groups.items():assert len(g)==n and {a for _,a in g}==set(ARMS[:n]) and {s for s,_ in g}==set(range(n)),key
    for scene,leaves in scenes.items():
        assert leaves==set(range(len(leaves)))
        for block in range(n):
            for arm in ARMS[:n]:assert counts[(scene,block,arm)]==len(leaves)
    return frames,scenes,totals

def run(path,n,out,reference=None):
    frames,scenes,totals=read(path,n)
    if reference is not None:assert frames==read(reference,11)[0],'cross-campaign candidate/geometry count mismatch'
    details=[]
    for scene in sorted(scenes):
        ratios={arm:gm([totals[(scene,b,'exhaustive')]/totals[(scene,b,arm)] for b in range(n)]) for arm in ARMS[:n]}
        speed={arm:gm([totals[(scene,b,'persistent_axis')]/totals[(scene,b,arm)] for b in range(n)]) for arm in ARMS[:n]}
        winner=max(ratios,key=ratios.get)
        details.append(dict(scene=scene[0],width=scene[1],regime=scene[2],frames=len(scenes[scene]),speed_vs_exhaustive=ratios,speed_vs_persistent_axis=speed,winner=winner))
    aggregate={arm:dict(geomean_vs_exhaustive=gm([r['speed_vs_exhaustive'][arm] for r in details]),geomean_vs_persistent_axis=gm([r['speed_vs_persistent_axis'][arm] for r in details]),wins=sum(r['winner']==arm for r in details),wins_vs_persistent=sum(r['speed_vs_persistent_axis'][arm]>1 for r in details)) for arm in ARMS[:n]}
    result=dict(arms=n,configurations=len(scenes),distinct_frames=len(frames),matched_groups=len(frames)*n,rows=len(frames)*n*n,max_candidates=max(v[0] for v in frames.values()),max_features=max(sum(v[1:]) for v in frames.values()),cross_campaign_counts_match=reference is not None,aggregate=aggregate,scenes=details,scope='Within-campaign exact vector parity enforced by C++ driver; this CSV audit validates counts and balanced slots, not vectors. Timing includes paid query, not physical integration. Ratios are geometric means of matched block sums, equal weight per configuration.')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='scenes'},indent=2))
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('csv',type=Path);p.add_argument('--arms',type=int,choices=[11,12],required=True);p.add_argument('--reference',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args();run(a.csv,a.arms,a.out,a.reference)
