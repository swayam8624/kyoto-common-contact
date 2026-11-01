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
fields=("trial","frame","policy","stepMs","broadMs","narrowMs","diagnosticMs","otherMs","preciseTests")
numeric_fields=fields[3:]
if not rows or not all(x in rows[0] for x in fields):
    raise SystemExit("Missing measured downstream metrics")
groups={"baseline_rebuild":[],"guarded_reuse":[]}
paired={}
seen=set()
for row in rows:
    if row["policy"] not in groups:raise SystemExit("Unexpected benchmark policy")
    try:trial,frame=int(row["trial"]),int(row["frame"])
    except (TypeError,ValueError):raise SystemExit("Invalid trial/frame identity")
    if str(trial)!=row["trial"] or str(frame)!=row["frame"]:
        raise SystemExit("Trial/frame identities must be integers")
    key=(trial,frame,row["policy"])
    if key in seen:raise SystemExit("Duplicate timed sample "+str(key))
    seen.add(key)
    paired.setdefault((trial,frame),set()).add(row["policy"])
    groups[row["policy"]].append(row)
if not all(groups.values()):raise SystemExit("Missing paired policy")
expected={(trial,frame) for trial in range(5) for frame in range(30,300)}
if set(paired)!=expected or any(policies!={"baseline_rebuild","guarded_reuse"}
                                 for policies in paired.values()):
    raise SystemExit("Expected 5 paired trials × frames 30..299 for both policies")
stats={}
for key,data in groups.items():
    vals={}
    for field in numeric_fields:
        try:observations=[float(r[field]) for r in data]
        except (TypeError,ValueError):raise SystemExit("Invalid recorded "+field)
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
