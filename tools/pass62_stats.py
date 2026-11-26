"""Paired log-ratio analysis. Resample blocks for fixed-suite timing uncertainty;
source/seed clusters for sensitivity only (few, deliberately selected clusters).
No individual frame is treated as an independent observation.
"""
import csv, json, math, random, statistics, sys
from pathlib import Path
from collections import defaultdict

ARMS=('exhaustive','rebuilt_sap','rebuilt_bvh','retained_refit','retained_frontier','persistent_x','adaptive_axis','persistent_axis','cached_axis','spatial_first','fused','ipc_lbvh')

def mean(v): return statistics.mean(v)
def expmean(v): return math.exp(mean(v))
def quantile(v,p):
    v=sorted(v); i=(len(v)-1)*p; j=int(i); return v[j]+(v[min(j+1,len(v)-1)]-v[j])*(i-j)
def interval(v): return [math.exp(quantile(v,p)) for p in (.025,.975)]

def read(path,n,held):
    groups=defaultdict(list); frames={}; totals=defaultdict(int); scenes=defaultdict(set)
    with path.open() as f:
        for r in csv.DictReader(f):
            scene=(r['scene'],int(r['width']),r['regime']); leaf=int(r['leaf']); b=int(r['block']); s=int(r['slot']); a=r['arm']; ns=int(r['ns']); meta=tuple(int(r[k]) for k in ('candidates','vertices','edges','faces'))
            assert 0<=b<n and 0<=s<n and a==ARMS[(s+b)%n] and ns>0 and min(meta)>=0
            k=(*scene,leaf); assert frames.setdefault(k,meta)==meta
            groups[(*k,b)].append(s); totals[scene,b,a]+=ns; scenes[scene].add(leaf)
    assert len(frames)==(120 if held else 300) and len(scenes)==(20 if held else 17) and len(groups)==len(frames)*n
    assert all(sorted(g)==list(range(n)) for g in groups.values())
    assert all(v==set(range(len(v))) for v in scenes.values())
    if held:
        assert set(scenes)=={(f'heldout_{s}',1,m) for s in (61057,61069,61091,61121) for m in ('sparse','dense_masks','topology_shear','large_offset','subnormal')}
    return frames,sorted(scenes),totals

def cluster(scene,held):
    if held:return scene[0]
    # Replicas and reversal use the same source as physical2; zero/half skin share a trajectory.
    return scene[0] if scene[0].startswith('physical') else 'physical2'

def compare(scenes,totals,n,held,base,arm):
    logs=[[math.log(totals[s,b,base]/totals[s,b,arm]) for b in range(n)] for s in scenes]
    means=[mean(x) for x in logs]; by=defaultdict(list)
    for s,v in zip(scenes,means):by[cluster(s,held)].append(v)
    cluster_means={k:mean(v) for k,v in sorted(by.items())}; cv=list(cluster_means.values())
    # The same resampled block indices are used across configurations. This
    # describes repeat timing variation on this fixed suite, not new geometries.
    block_means=[mean([v[b] for v in logs]) for b in range(n)]
    rng=random.Random(62013); boot=[mean(rng.choices(block_means,k=n)) for _ in range(10000)]
    # Cluster bootstrap is a sensitivity calculation, not a population CI.
    cboot=[mean(rng.choices(cv,k=len(cv))) for _ in range(10000)]
    signs=[v for v in cv if v!=0]; k=sum(v>0 for v in signs); m=len(signs); p=min(1.,2*sum(math.comb(m,i) for i in range(min(k,m-k)+1))/2**m) if m else 1.
    return dict(baseline=base,arm=arm,configurations=len(scenes),clusters=len(cv),ratio=expmean(means),fixed_suite_block_bootstrap_95=interval(boot),config_wins=sum(v>0 for v in means),cluster_equal_weight_ratio=expmean(cv),cluster_resample_sensitivity_95=interval(cboot),cluster_sign_test_two_sided_unadjusted=p,cluster_wins=k,cluster_ratios={k:math.exp(v) for k,v in cluster_means.items()},leave_one_cluster_out=[min(expmean([x for j,x in enumerate(cv) if i!=j]) for i in range(len(cv))),max(expmean([x for j,x in enumerate(cv) if i!=j]) for i in range(len(cv)))],worst_config=(*scenes[means.index(min(means))],math.exp(min(means))),best_config=(*scenes[means.index(max(means))],math.exp(max(means))))

def run(root,out):
    datasets=[('archive11','native47.csv',11,False),('archive12','stock59.csv',12,False),('held_gcc','gcc/heldout.csv',11,True),('held_clang','clang/heldout.csv',11,True),('held_stock','stock/stock61.csv',12,True)]
    results={}; ref={}
    for name,path,n,held in datasets:
        frames,scenes,totals=read(root/path,n,held)
        assert frames==ref.setdefault(held,frames),name
        pairs=[('persistent_axis','fused'),('spatial_first','fused'),('exhaustive','fused'),('persistent_axis','cached_axis'),('cached_axis','spatial_first')]
        if n==12:pairs.insert(0,('ipc_lbvh','fused'))
        results[name]=[compare(scenes,totals,n,held,b,a) for b,a in pairs]
    out.write_text(json.dumps(results,indent=2)+'\n');return results
if __name__=='__main__':
    run(Path(sys.argv[1]),Path(sys.argv[2]))
