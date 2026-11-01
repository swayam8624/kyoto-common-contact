#!/usr/bin/env python3
"""Profile-guided static patch-size ablation, with untouched evaluation frames.

No fabricated timings; all rows require paired physical-state equality.
Select from the calibration period only, then report later-frame performance.
"""
import csv
import math
from pathlib import Path
import sys
from xml.sax.saxutils import escape

if len(sys.argv)!=4:
    raise SystemExit("usage: summarize_patch_sweep.py IN.csv OUT.svg REPORT.md")
source,chart,report=map(Path,sys.argv[1:])
with source.open(newline="") as stream:
    rows=list(csv.DictReader(stream))

fields=("patchSpan","frontier","phase","frames","meanStepMs","meanBroadMs",
        "meanNarrowMs","meanCandidates","meanSkippedPatchPairs",
        "meanRegionBuilds","meanRegionReuses","meanInterContacts",
        "maxPositionError")
variants=[(span,frontier) for frontier in ("all_region_pairs","two_tier")
          for span in (1,2,3,4,6,12)]
expected={(span,frontier,phase) for span,frontier in variants
          for phase in ("calibration","evaluation")}
expected|={(0,"sap_rebuild",phase) for phase in ("calibration","evaluation")}
data={}
for row in rows:
    if not all(field in row for field in fields):
        raise SystemExit("patch experiment missing required columns")
    key=(int(row["patchSpan"]),row["frontier"],row["phase"])
    if key in data:raise SystemExit(f"duplicate patch profile row {key}")
    for field in fields:
        if field in ("frontier","phase"):continue
        try:value=float(row[field])
        except (TypeError,ValueError):raise SystemExit(f"invalid {field} in {key}")
        if not math.isfinite(value) or value<0:
            raise SystemExit(f"invalid {field} in {key}")
    if float(row["maxPositionError"])>=1e-8:
        raise SystemExit(f"solver position divergence in {key}")
    data[key]=row
if set(data)!=expected:
    raise SystemExit(f"incomplete patch-size experiment {expected-set(data)}")

# Include the reference SAP as a fallback: profiling must never be forced
# to choose an expensive regional configuration in a sparse workload.
choices=variants+[(0,"sap_rebuild")]
selected=min(choices,key=lambda item:float(data[(*item,"calibration")]["meanStepMs"]))
winner=float(data[(*selected,"evaluation")]["meanStepMs"])
sap=float(data[0,"sap_rebuild","evaluation"]["meanStepMs"])
control=float(data[3,"all_region_pairs","evaluation"]["meanStepMs"])
improvement=100*(sap-winner)/sap if sap else 0
order=sorted(variants,key=lambda m:float(data[(*m,"evaluation")]["meanStepMs"]))
w,h=1250,845
max_ms=max([sap]+[float(data[(*m,"evaluation")]["meanStepMs"]) for m in variants])
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">',
'<rect width="1250" height="845" fill="#f9fbfd"/>',
'<text x="30" y="44" fill="#193146" font-family="Arial,sans-serif" font-size="26" font-weight="bold">Patch Granularity: Earlier Calibration, Later Evaluation</text>',
'<text x="30" y="72" fill="#53677b" font-family="Arial,sans-serif" font-size="15">Three genuinely interacting cloth sheets. Same forces and precise solver; candidate sets audited.</text>',
'<text x="30" y="99" fill="#53677b" font-family="Arial,sans-serif" font-size="14">Bar lengths: mean complete CPU-step time during held-out later frames (lower is better).</text>']
for i,(span,frontier) in enumerate(order):
    y=138+i*44
    t=float(data[span,frontier,"evaluation"]["meanStepMs"])
    color="#26867e" if frontier=="two_tier" else "#8d77b1"
    label=f"{frontier.replace('_',' ')} · {span}x{span} cells"
    svg.extend([
      f'<text x="30" y="{y+15}" fill="#33495d" font-family="Arial,sans-serif" font-size="14">{escape(label)}</text>',
      f'<rect x="330" y="{y}" width="{max(1,690*t/max_ms):.2f}" height="23" rx="3" fill="{color}"/>',
      f'<text x="1045" y="{y+17}" fill="#18334a" font-family="Arial,sans-serif" font-size="15">{t:.4f} ms</text>',
    ])
svg.extend([
f'<text x="30" y="711" fill="#193146" font-family="Arial,sans-serif" font-size="17">SAP evaluation reference: {sap:.4f} ms / step</text>',
f'<text x="30" y="740" fill="#193146" font-family="Arial,sans-serif" font-size="17">Calibration-selected: {escape(selected[1])}, span {selected[0]} · evaluation {winner:.4f} ms ({improvement:+.1f}% relative to SAP)</text>',
'<text x="30" y="786" fill="#53677b" font-family="Arial,sans-serif" font-size="13">SAP is an eligible fallback. A single timing run is noisy; this does not establish independent-scene generalization.</text>',
'<text x="30" y="813" fill="#53677b" font-family="Arial,sans-serif" font-size="13">No online region splitter/merger or publication-level novelty is claimed.</text>',
'</svg>'])
chart.parent.mkdir(parents=True,exist_ok=True)
chart.write_text("\n".join(svg),encoding="utf-8")

lines=[
"# Patch-size calibration and later-frame evaluation",
"",
"The C++ experiment profiles 12 variants: six mesh-region cell spans (1, 2, 3, 4, 6, 12), both the original flat patch-pair enumeration and the new hierarchical envelope sweep.",
"",
"Calibration and evaluation use DISJOINT later and earlier time windows of one deterministic three-cloth physics trajectory. The selected policy is chosen using calibration complete-step time ONLY. Evaluation is reported on later frames that do not take part in the selection.",
"",
"Every alternative must preserve all physical particle positions against full sweep-and-prune (maximum error below 1e-8); conservative broad-phase query coverage also has its own independent oracle tests.",
"",
"| Region span | Frontier | Time window | Full step (ms) | Broad phase (ms) | Precise response (ms) | Candidates | Coarse pairs skipped | Region builds | Region reuses |",
"|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
for span,frontier,phase in sorted(data):
    row=data[span,frontier,phase]
    n=lambda field:f'{float(row[field]):.3f}'
    lines.append(f'| {span} | {frontier} | {phase} | {n("meanStepMs")} | {n("meanBroadMs")} | {n("meanNarrowMs")} | {n("meanCandidates")} | {n("meanSkippedPatchPairs")} | {n("meanRegionBuilds")} | {n("meanRegionReuses")} |')
lines += [
"",
"## Training-time choice and untouched later-frame result",
"",
f"Calibration chose **{selected[1]}**" + (f", patch span **{selected[0]}** cells." if selected[1]!="sap_rebuild" else " as the lower-cost fallback."),
f"Later-frame evaluation: **{winner:.4f} ms**, SAP **{sap:.4f} ms**, original flat span-three **{control:.4f} ms**.",
f"Evaluation step-time reduction relative to SAP: **{improvement:+.2f}%** (negative = slower).",
"",
"## Limitations",
"",
"This is STATIC profile-guided granularity, not a learned online mesh partitioner. One deterministic scene and one timing run cannot establish generality; repeat with other trajectories and hardware before making speedup claims.",
"",
"AABB, region pruning, temporal caches, hierarchical spatial search and profile-guided tuning have significant prior art. This study tests their interaction and correctness but does not prove a previously unpublished algorithm.",
]
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text("\n".join(lines),encoding="utf-8")
print(f"Validated {len(data)} rows. Calibration choice {selected}; later evaluation {winner:.4f} ms vs SAP {sap:.4f} ms.")
