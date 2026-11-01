#!/usr/bin/env python3
"""Validate online collision-control trace and render measured time-series."""
import csv
import math
from pathlib import Path
import statistics
import sys
from xml.sax.saxutils import escape
if len(sys.argv)!=4:
    raise SystemExit("usage: summarize_online_trace.py INPUT.csv OUTPUT.svg REPORT.md")
source,image,report=map(Path,sys.argv[1:])
with source.open(newline="") as f:
    rows=list(csv.DictReader(f))
required=("frame","onlineStepMs","sapStepMs","onlineBroadMs","sapBroadMs",
          "onlineNarrowMs","onlineDecisionMs","onlinePatches","onlineSplits",
          "onlineMerges","onlineStrategySwitches","onlineHierarchy",
          "contactCorrections","interSheetCorrections","onlineCandidates",
          "sapCandidates","regionDirty","regionBuilds","regionReuses",
          "coarseSkip","observedAabbWork","observedContactHeat",
          "observedCoarseOverlapFraction","maxPositionError","coverageChecked")
if len(rows)<60:raise SystemExit("online trace must have >=60 frames")
for i,row in enumerate(rows):
    if any(k not in row for k in required):
        raise SystemExit("missing trace columns")
    if int(row["frame"])!=i+1:
        raise SystemExit(f"missing/duplicate trace frame {i+1}")
    for k in required:
        x=float(row[k])
        if not math.isfinite(x) or x<0:
            raise SystemExit(f"nonfinite/negative {k} at frame {i+1}")
    if float(row["maxPositionError"])>=1e-8:
        raise SystemExit("online changed physical trajectory")
    if int(row["onlinePatches"])==0:
        raise SystemExit("online partition disappeared")
if not any(int(r["onlineSplits"])>0 for r in rows):
    raise SystemExit("controller must perform an actual local region split")
if sum(int(r["coverageChecked"]) for r in rows)<10:
    raise SystemExit("not enough independent oracle audit frames")
warmup=12
measured=rows[warmup:]
average=lambda field:statistics.fmean(float(r[field]) for r in measured)
splits=sum(int(r["onlineSplits"]) for r in rows)
merges=sum(int(r["onlineMerges"]) for r in rows)
switches=sum(int(r["onlineStrategySwitches"]) for r in rows)
online=average("onlineStepMs");sap=average("sapStepMs")
speed=100*(sap-online)/sap if sap>0 else 0
w,h=1220,775
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">',
'<rect width="1220" height="775" fill="#f9fbfd"/>',
'<text x="30" y="46" font-family="Arial,sans-serif" font-weight="bold" font-size="27" fill="#172f46">Online Region Splitting / Merging — Real Multi-Cloth Dynamics</text>',
'<text x="30" y="74" font-family="Arial,sans-serif" font-size="14" fill="#526879">Three deforming sheets, two moving spheres, same precise contact solver and physical trajectory</text>',
'<text x="30" y="97" font-family="Arial,sans-serif" font-size="13" fill="#526879">Top: per-frame complete CPU step time. Bottom: live patch count and confirmed inter-sheet contact corrections.</text>']
left,right=76,1170
maxStep=max(max(float(r["sapStepMs"]),float(r["onlineStepMs"])) for r in rows)
maxPatch=max(float(r["onlinePatches"]) for r in rows)
maxContact=max(1.,max(float(r["interSheetCorrections"]) for r in rows))
def polyline(field,bottom,height,maxval,color,stroke=2):
    points=[]
    for i,r in enumerate(rows):
        x=left+(right-left)*i/max(1,len(rows)-1)
        y=bottom-height*float(r[field])/max(maxval,1e-9)
        points.append(f"{x:.1f},{y:.1f}")
    svg.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="{stroke}" stroke-linecap="round" stroke-linejoin="round"/>')
for y in (200,350,550,675):
    svg.append(f'<line x1="{left}" x2="{right}" y1="{y}" y2="{y}" stroke="#d9e0e7" stroke-width="1"/>')
svg.extend(['<text x="30" y="132" font-family="Arial,sans-serif" fill="#1b496d" font-size="16">Online full step — blue</text>',
            '<text x="345" y="132" font-family="Arial,sans-serif" fill="#b46c22" font-size="16">SAP full step — ochre</text>'])
polyline("onlineStepMs",370,220,maxStep,"#2563a2")
polyline("sapStepMs",370,220,maxStep,"#be7e32")
svg.extend(['<text x="30" y="463" font-family="Arial,sans-serif" fill="#26857a" font-size="16">Online region leaf count — teal</text>',
            '<text x="370" y="463" font-family="Arial,sans-serif" fill="#9b5684" font-size="16">Verified inter-sheet corrections — magenta</text>'])
polyline("onlinePatches",690,170,maxPatch,"#228979")
polyline("interSheetCorrections",690,170,maxContact,"#9b5684")
for i,row in enumerate(rows):
    if int(row["onlineSplits"]) or int(row["onlineMerges"]):
        x=left+(right-left)*i/max(1,len(rows)-1)
        color="#1b9d67" if int(row["onlineSplits"]) else "#c36537"
        svg.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="165" y2="715" stroke="{color}" stroke-opacity=".55" stroke-dasharray="4,5"/>')
svg.append(f'<text x="30" y="747" font-family="Arial,sans-serif" font-size="14" fill="#536779">Online mean {online:.3f} ms, SAP mean {sap:.3f} ms ({speed:+.1f}% measured reduction); {splits} splits, {merges} merges, {switches} strategy changes.</text>')
svg.append('</svg>')
image.parent.mkdir(parents=True,exist_ok=True)
image.write_text("\n".join(svg),encoding="utf-8")
lines=[
"# Online broad-phase controller — verified physical time-series",
"",
"**Implemented:** at every physics step, collect actual per-region AABB checks, cached-pair revalidations, candidate counts and confirmed contact corrections. A bounded local controller periodically splits contact-heavy 12×12/6×6 mesh patches into four children (minimum 3×3), merges quiet four-child quartets, and can change flat-versus-two-tier region traversal according to observed region overlap and measured broad-phase timing. Repartitioning resets candidate certificates BEFORE subsequent queries.",
"",
"**Correctness boundary:** narrow-phase mathematics and XPBD-style solver are unchanged; frame-by-frame particle positions match SAP to <1e-8. Exhaustive AABB candidate oracles run on periodically audited frames. No continuous collision detection or production-level nonpenetration guarantee.",
"",
f"Total recorded frames: **{len(rows)}**. Measured average after first {warmup}: online {online:.4f} ms/step, SAP {sap:.4f} ms/step.",
f"Measured percentage reduction: **{speed:+.2f}%** (a negative value means the online controller was slower).",
f"Real geometry: **{splits} patch splits**, **{merges} patch merges**, **{switches} front-end strategy switches**.",
f"Audited frames: **{sum(int(r['coverageChecked']) for r in rows)}**.",
f"Mean online controller decision time: **{average('onlineDecisionMs'):.5f} ms**.",
f"Mean online broad-phase service: **{average('onlineBroadMs'):.5f} ms**.",
f"Mean SAP broad-phase service: **{average('sapBroadMs'):.5f} ms**.",
"",
"## Every observed controller decision",
"",
"| Frame | Action | Patches | Contact corrections | Region AABB work | Broad phase (ms) | Complete online step (ms) |",
"|---:|---|---:|---:|---:|---:|---:|"]
for row in rows:
    actions=[]
    for flag,name in (("onlineSplits","split"),("onlineMerges","merge"),
                      ("onlineStrategySwitches","strategy switch")):
        if int(row[flag]):actions.append(name+" ×"+str(int(row[flag])))
    if not actions:continue
    lines.append(f'| {row["frame"]} | {", ".join(actions)} | {row["onlinePatches"]} | {row["interSheetCorrections"]} | {float(row["observedAabbWork"]):.1f} | {float(row["onlineBroadMs"]):.4f} | {float(row["onlineStepMs"]):.4f} |')
if splits+merges+switches==0:
    lines.append("| — | No changes | — | — | — | — | — |")
lines.extend(["",
"## Experimental limitations and prior art",
"",
"The controller is genuinely online and changes the region partition, but its thresholds, EWMA coefficients, controller work model and hysteresis are engineering heuristics and are not proved optimal. Its strategy-switch measurements are not same-state paired A/B timings. These results are a single sequential CPU run, not randomized controlled trials or proof of generalization.",
"",
"Region splitting/merging, automatic broad-phase pruning, dynamic BVH maintenance, and contact-aware cloth culling have significant prior art: PhysX ABP/PABP, Bullet DBVT, solver-integrated deforming BVH updates (TVCG 2026), and curvature-based collision culling (Mathematics 2026). This implementation alone does not establish publishable novelty or superiority.",
""])
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text("\n".join(lines),encoding="utf-8")
print(f"Verified {len(rows)} online physics frames: {splits} splits, {merges} merges, {switches} mode switches, mean step online {online:.3f} ms vs SAP {sap:.3f} ms.")
