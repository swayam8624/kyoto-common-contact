#!/usr/bin/env python3
"""Plot measured policy-only counts; no implied geometry or timing speedup."""
import csv
import sys
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
names=["One-bit mask","Two-draw mask","Random low mask","Dense multi-bit","Wide category"]
with open(sys.argv[1],newline="") as f:
    rows=list(csv.DictReader(f))
if len(rows)!=5 or [int(r["regime"]) for r in rows]!=list(range(5)):
    raise ValueError("missing study regimes")
for r in rows:
    n=int(r["products"]); allowed=int(r["oracle_compatible"])
    u=int(r["union_reject"]); p=int(r["relation_reject"])
    if n!=50000 or int(r["unsafe_reject"])!=0 or not 0<=u<=p<=n-allowed:
        raise ValueError("invalid or unsafe policy data")
    if int(r["regime"])<4 and p!=n-allowed:
        raise ValueError("low-bit relation is not exact")
x=np.arange(5); w=.34
u=[100*int(r["union_reject"])/int(r["products"]) for r in rows]
p=[100*int(r["relation_reject"])/int(r["products"]) for r in rows]
fig,ax=plt.subplots(figsize=(10,5.6),layout="constrained")
ax.bar(x-w/2,u,w,label="Union of category and mask bits")
ax.bar(x+w/2,p,w,label="Relational certificate")
ax.set_xticks(x,names,rotation=12,ha="right")
ax.set_ylim(0,105);ax.set_ylabel("Node products rejected (%)")
ax.set_title("Policy-only pruning: 250,000 randomized products")
ax.legend();ax.grid(axis="y",alpha=.22)
fig.savefig(Path(sys.argv[2]),dpi=200)
