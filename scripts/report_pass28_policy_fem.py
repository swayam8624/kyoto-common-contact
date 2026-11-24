#!/usr/bin/env python3
"""Strict native FEM three-arm receipt audit; no synthetic timings."""
import csv,sys,json,statistics
from pathlib import Path
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
with open(sys.argv[1],newline="") as f: rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(f)]
expected={(r,s,t,q) for r in range(5) for s in range(3) for t in range(12,241,12) for q in range(6)}
actual={(r["regime"],r["scene"],r["step"],r["trial"]) for r in rows}
if len(rows)!=1800 or actual!=expected:raise ValueError("missing or duplicate native rows")
for r in rows:
    i=r["scene"]*20+r["step"]//12-1
    if r["order"]!=(i+r["trial"])%6:raise ValueError("counterbalance mismatch")
    expected_cold=r["step"]==12 or (r["regime"]==4 and r["step"]//36!=(r["step"]-12)//36)
    if bool(r["cold"])!=expected_cold:raise ValueError("cold epoch mismatch")
    if min(r[k] for k in ("base_ns","union_ns","relation_ns","union_bytes","relation_bytes"))<=0:raise ValueError("invalid paid time or policy bytes")
    if any(v<0 for v in r.values()):raise ValueError("negative receipt")
    if r["regime"]==2 and r["oracle_pairs"]!=0:raise ValueError("nonreciprocal policy permitted pair")
def stats(a):
    b=sum(r["base_ns"] for r in a);u=sum(r["union_ns"] for r in a);v=sum(r["relation_ns"] for r in a)
    return {"rows":len(a),"baseline_over_union":b/u,"baseline_over_relation":b/v,"union_over_relation":u/v,
            "union_peak_bytes":max(r["union_bytes"] for r in a),"relation_peak_bytes":max(r["relation_bytes"] for r in a),
            "union_prunes":sum(r["union_prunes"] for r in a),"relation_prunes":sum(r["relation_prunes"] for r in a)}
groups={}
for regime in range(5):
    for cold in (0,1):
        a=[r for r in rows if r["regime"]==regime and r["cold"]==cold]
        groups[f"{regime}:{cold}"]={**stats(a),"trials":{str(t):stats([r for r in a if r["trial"]==t]) for t in range(6)},
                                   "scenes":{str(s):stats([r for r in a if r["scene"]==s]) for s in range(3)}}
(out/"pass28_summary.json").write_text(json.dumps(groups,indent=2)+"\n")
names=["All allowed","Same group","Directed cycle","Neighbour groups","Changing wide"]
for cold in (0,1):
    fig,ax=plt.subplots(figsize=(10,5),layout="constrained")
    for method,offset in [("union",-.18),("relation",.18)]:
        ax.bar([i+offset for i in range(5)],[groups[f"{i}:{cold}"][f"baseline_over_{method}"] for i in range(5)],.36,label=method)
    ax.axhline(1,linestyle="--");ax.set_yscale("log");ax.set_xticks(range(5),names,rotation=10)
    ax.set(ylabel="Baseline / method paid time (log scale)",title=("Cold rebuilds" if cold else "Warm refits")+" | physically integrated FEM geometry, assigned policies")
    ax.legend();fig.savefig(out/f"pass28_{'cold' if cold else 'warm'}.png",dpi=190);plt.close(fig)
fig,ax=plt.subplots(figsize=(9,5),layout="constrained")
for method,offset in [("union",-.18),("relation",.18)]:
    ax.bar([i+offset for i in range(5)],[groups[f"{i}:0"][f"{method}_peak_bytes"]/1024 for i in range(5)],.36,label=method)
ax.set_xticks(range(5),names,rotation=10);ax.set(ylabel="Peak summary allocation (KiB)",title="Incremental policy storage only");ax.legend();fig.savefig(out/"pass28_policy_memory.png",dpi=190)
