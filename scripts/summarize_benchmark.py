#!/usr/bin/env python3
"""Summarize measured, oracle-separated paired CPU runs using stdlib only.

No fabricated numbers. Do not infer cross-solver performance from this fixture.
"""
import csv
import math
import pathlib
import statistics
import sys
import xml.sax.saxutils as xml

if len(sys.argv) != 4:
    raise SystemExit("usage: summarize_benchmark.py INPUT.csv OUTPUT.svg OUTPUT.md")
src, svg_path, md_path = map(pathlib.Path, sys.argv[1:])
with src.open(newline="") as stream:
    rows = list(csv.DictReader(stream))
required = {"trial", "frame", "policy", "stepMs", "broadMs", "narrowMs", "oracleMs",
            "diagnosticMs", "preciseTests", "oracleMisses"}
if not rows or not required.issubset(rows[0]):
    raise SystemExit("CSV lacks required columns or rows")
groups = {}
paired = {}
seen = set()
for row in rows:
    if row["oracleMisses"] != "NA":
        raise SystemExit("Timed run incorrectly claims exhaustive validation")
    label = row["policy"]
    if label not in {"baseline_rebuild", "guarded_reuse"}:
        raise SystemExit("Unexpected benchmark policy: " + label)
    try:
        trial, frame = int(row["trial"]), int(row["frame"])
    except (TypeError, ValueError):
        raise SystemExit("Invalid trial/frame identity")
    if str(trial) != row["trial"] or str(frame) != row["frame"]:
        raise SystemExit("Trial/frame identities must be integers")
    key = (trial, frame, label)
    if key in seen:
        raise SystemExit("Duplicate timed sample: " + str(key))
    seen.add(key)
    paired.setdefault((trial, frame), set()).add(label)
    groups.setdefault(label, []).append(row)
if set(groups) != {"baseline_rebuild", "guarded_reuse"}:
    raise SystemExit("Both paired policies are required")
expected = {(trial, frame) for trial in range(5) for frame in range(30, 300)}
if set(paired) != expected or any(policies != {"baseline_rebuild", "guarded_reuse"}
                                   for policies in paired.values()):
    raise SystemExit("Expected 5 paired trials × frames 30..299 for both policies")

def measures(records, name):
    try:
        values = [float(r[name]) for r in records]
    except (TypeError, ValueError):
        raise SystemExit(f"Invalid metric {name}")
    if not all(math.isfinite(v) and v >= 0 for v in values):
        raise SystemExit(f"Invalid metric {name}")
    return values

def p95(values):
    ordered = sorted(values)
    return ordered[math.ceil(0.95*len(ordered))-1]

def fmt(x):
    return f"{x:.4f}"

stats = {}
for name,records in groups.items():
    step = measures(records, "stepMs")
    stats[name] = dict(
        n=len(records), mean=statistics.mean(step), median=statistics.median(step),
        p95=p95(step), maximum=max(step),
        broad=statistics.mean(measures(records, "broadMs")),
        narrow=statistics.mean(measures(records, "narrowMs")),
        diagnostics=statistics.mean(measures(records, "diagnosticMs")),
        precise=statistics.mean(measures(records, "preciseTests")),
        oracle=statistics.mean(measures(records, "oracleMs")))
b,g = stats["baseline_rebuild"],stats["guarded_reuse"]
ratio=b["mean"]/g["mean"] if g["mean"] else float("inf")
maximum=max(b["mean"],g["mean"],1e-6)
width,height = 930,480
parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
'<rect width="930" height="480" fill="#fafbfc"/>',
'<text x="40" y="53" font-family="Arial,sans-serif" font-size="27" font-weight="bold" fill="#1b293a">Measured candidate-driven cloth pipeline</text>',
'<text x="40" y="85" font-family="Arial,sans-serif" font-size="15" fill="#536476">Release build · 5 paired trials · oracle preflight outside timing · display diagnostics included</text>']
for i,(name,stat,color) in enumerate([
    ("baseline_rebuild",b,"#b87916"),("guarded_reuse",g,"#18877f")]):
    y=143+i*135
    bar=610*stat["mean"]/maximum
    parts.extend([
        f'<text x="40" y="{y}" font-family="Arial,sans-serif" font-size="20" fill="#1b293a">{xml.escape(name.replace("_"," "))}</text>',
        f'<rect x="40" y="{y+18}" width="{bar:.2f}" height="48" rx="7" fill="{color}"/>',
        f'<text x="670" y="{y+51}" font-family="Arial,sans-serif" font-size="23" font-weight="bold" fill="#1b293a">{fmt(stat["mean"])} ms</text>',
        f'<text x="41" y="{y+94}" font-family="Arial,sans-serif" font-size="14" fill="#536476">p95 {fmt(stat["p95"])} ms · mean broad {fmt(stat["broad"])} ms · mean precise {fmt(stat["narrow"])} ms</text>'])
parts.extend(['<text x="40" y="447" font-family="Arial,sans-serif" font-size="15" fill="#536476">Single synthetic scene only. GPU rendering excluded. Full final-frame diagnostic scan remains timed.</text>','</svg>'])
svg_path.parent.mkdir(parents=True,exist_ok=True)
svg_path.write_text("\n".join(parts),encoding="utf-8")
md_path.parent.mkdir(parents=True,exist_ok=True)
md=f"""# Kyoto meeting: post-fix benchmark results

**Generated from real CSV:** `{src}`

The independent all-triangle correctness oracle ran in a separate, untimed
preflight. The timed simulations use **candidate-driven narrow phase**, with
the exhaustive per-iteration oracle disabled. The final all-triangle scan for
the live visualization/minimum gap remains enabled, measured separately.

| Measurement | Rebuild every iteration | Guarded reuse |
|---|---:|---:|
| Measured steps | {b['n']} | {g['n']} |
| Mean step (ms) | {fmt(b['mean'])} | {fmt(g['mean'])} |
| Median step (ms) | {fmt(b['median'])} | {fmt(g['median'])} |
| p95 step (ms) | {fmt(b['p95'])} | {fmt(g['p95'])} |
| Max step (ms) | {fmt(b['maximum'])} | {fmt(g['maximum'])} |
| Mean broad (ms) | {fmt(b['broad'])} | {fmt(g['broad'])} |
| Mean precise path (ms) | {fmt(b['narrow'])} | {fmt(g['narrow'])} |
| Mean display diagnostics (ms) | {fmt(b['diagnostics'])} | {fmt(g['diagnostics'])} |
| Mean precise tests per step | {fmt(b['precise'])} | {fmt(g['precise'])} |

Observed mean step-time ratio (rebuild / guarded): **{ratio:.3f}×**.

**Interpretation:** limited to this fixed scene, collision type, timestep,
query margin, CPU and implementation. This is not IPC/OGC/CCFR speedup evidence.
The diagnostic scan must be removed or separately modeled before calling these
numbers a fully optimized production pipeline. A disabled oracle does not mean
zero unchecked misses; the dedicated oracle preflight and automated tests
provide the tested fixture's correctness evidence.

Generated figure: `{svg_path}`
"""
md_path.write_text(md,encoding="utf-8")
print(f"Calculated {b['n']} and {g['n']} measured frames. Mean ratio {ratio:.3f}x")
