#!/usr/bin/env python3
"""Trace the real measured cascade from broad phase to precise work to step.

No speculative per-contact time or solver speedup is invented. The "other"
bucket includes integration, XPBD constraints, bound creation, bookkeeping.
The final global closest-point scan is still present and separately shown.
"""
import csv
import math
from pathlib import Path
import statistics
import sys

if len(sys.argv)!=3:raise SystemExit("usage: summarize_pipeline.py BENCH.csv OUTPUT.svg")
src,dst=map(Path,sys.argv[1:])
with src.open(newline="") as fp: rows=list(csv.DictReader(fp))
fields=("policy","stepMs","broadMs","narrowMs","diagnosticMs","otherMs","preciseTests")
if not rows or not all(x in rows[0] for x in fields):
    raise SystemExit("Missing measured downstream metrics")
groups={"baseline_rebuild":[],"guarded_reuse":[]}
for row in rows:
    if row["policy"] in groups:groups[row["policy"]].append(row)
if not all(groups.values()):raise SystemExit("Missing paired policy")
stats={}
for key,data in groups.items():
    vals={}
    for field in fields[1:]:
        observations=[float(r[field]) for r in data]
        if not all(math.isfinite(v) and v>=0 for v in observations):
            raise SystemExit("Invalid recorded "+field)
        vals[field]=statistics.mean(observations)
    stats[key]=vals
W,H=1100,510
colors={"broadMs":"#427db9","narrowMs":"#b38a42","diagnosticMs":"#9477c4","otherMs":"#318b7b"}
labels={"broadMs":"Broad phase","narrowMs":"Precise candidate tests + response",
        "diagnosticMs":"Final global visualization diagnostics",
        "otherMs":"Other: constraints / integration / bounds"}
parts=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">',
'<rect width="1100" height="510" fill="#f9fbfd"/>',
'<text x="36" y="51" font-family="Arial,sans-serif" font-size="26" fill="#183044" font-weight="bold">Measured cascade: Candidate discovery to full CPU step</text>',
'<text x="36" y="79" font-family="Arial,sans-serif" font-size="15" fill="#526477">Paired XPBD-style cloth experiment; exact candidate-driven narrow phase; 1,350 steps per policy</text>']
maxTotal=max(v["stepMs"] for v in stats.values())
for index,(key,vals) in enumerate(stats.items()):
    y=136+index*148
    parts.append(f'<text x="36" y="{y}" fill="#1c2b3d" font-family="Arial,sans-serif" font-size="19">{key.replace("_"," ").title()}</text>')
    x=36.0
    barTop=y+16
    total=vals["stepMs"]
    for metric in ("broadMs","narrowMs","diagnosticMs","otherMs"):
        width=900.0*(vals[metric]/maxTotal)
        parts.append(f'<rect x="{x:.3f}" y="{barTop}" width="{max(0.0,width):.3f}" height="37" fill="{colors[metric]}"/>')
        x+=width
    parts.append(f'<text x="36" y="{barTop+62}" font-family="Arial,sans-serif" fill="#3d5064" font-size="14">'
                 f'Mean {total:.5f} ms · {vals["preciseTests"]:.1f} candidate-driven precise tests / step</text>')
for index,metric in enumerate(("broadMs","narrowMs","diagnosticMs","otherMs")):
    x=36+(index%2)*520;y=442+(index//2)*26
    parts.append(f'<rect x="{x}" y="{y-12}" width="15" height="15" fill="{colors[metric]}"/>')
    parts.append(f'<text x="{x+25}" y="{y}" font-family="Arial,sans-serif" fill="#445569" font-size="13">{labels[metric]}</text>')
parts.append('</svg>')
dst.parent.mkdir(parents=True,exist_ok=True)
dst.write_text("\n".join(parts),encoding="utf-8")
print("Generated real measured stage-composition chart:",dst)
for k,v in stats.items():print(k, "stage means",v)
