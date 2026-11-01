#!/usr/bin/env python3
"""Build measured multi-cloth comparative charts from actual native C++ runs.

The values are NEVER hard-coded. Failure to finish all paired scene/policy
comparisons prevents publishing any result as if it were verified.
"""
import csv
import math
from pathlib import Path
import sys
from xml.sax.saxutils import escape
if len(sys.argv)!=4:raise SystemExit("usage: summarize_multicloth.py INPUT.csv OUTPUT.svg REPORT.md")
source,chart,report=map(Path,sys.argv[1:])
with source.open(newline="") as fh:rows=list(csv.DictReader(fh))
policies=["sap_rebuild","global_guarded","region_fixed","region_adaptive",
          "region_sweep_fixed","region_sweep_adaptive","online_contact_work"]
expected={(s,p) for s in (1,2,3) for p in policies}
data={}
measured_frames=set()
required=("sheets","policy","frames","meanBroadMs","meanNarrowMs",
          "meanStepMs","meanCandidates","meanInterContacts",
          "meanRegionBuilds","meanRegionReuses","meanRejected","meanPreciseTests",
          "maxPositionError","meanActiveRegionPairs","meanSkippedRegionPairs",
          "meanOnlinePatches","meanSplits","meanMerges","meanStrategySwitches",
          "meanOnlineDecisionMs")
for r in rows:
 if any(x not in r for x in required):raise SystemExit("Missing required C++ benchmark field")
 key=(int(r["sheets"]),r["policy"])
 if key in data:raise SystemExit("Duplicate row "+str(key))
 try:frame_count=int(r["frames"])
 except (TypeError,ValueError):raise SystemExit("Invalid measured frame count "+str(key))
 if str(frame_count)!=r["frames"] or frame_count<=0:
  raise SystemExit("Measured frame count must be a positive integer "+str(key))
 measured_frames.add(frame_count)
 for field in required:
  if field in ("sheets","policy"):continue
  v=float(r[field])
  if not math.isfinite(v) or v<0:raise SystemExit("Invalid "+field+" "+str(key))
 if float(r["maxPositionError"])>=1e-7:raise SystemExit("Physical solver divergence: "+str(key))
 data[key]=r
if set(data)!=expected:raise SystemExit("Missing policies or scenes: "+str(expected-set(data)))
if len(measured_frames)!=1:
 raise SystemExit("All scene/policy rows must use the same measured frame count")
for s in (2,3):
 for p in policies:
  if float(data[s,p]["meanInterContacts"])<=0:
   raise SystemExit(f"No actual cross-sheet contact measured: sheets={s} policy={p}")
W,H=1260,1180
colors={"sap_rebuild":"#b47c3d","global_guarded":"#667b9a",
        "region_fixed":"#8c77ba","region_adaptive":"#1c8e86",
        "region_sweep_fixed":"#468ac6","region_sweep_adaptive":"#236c65",
        "online_contact_work":"#dd6a37"}
names={"sap_rebuild":"SAP full rebuild","global_guarded":"Global guarded SAP",
       "region_fixed":"Regional fixed budget","region_adaptive":"Regional adaptive budget",
       "region_sweep_fixed":"Two-tier region sweep (fixed)",
       "region_sweep_adaptive":"Two-tier region sweep (adaptive)",
       "online_contact_work":"Online contact-work partitioner"}
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">',
'<rect width="1260" height="1180" fill="#f8fbfd"/>',
'<text x="35" y="44" fill="#132a3c" font-family="Arial,sans-serif" font-size="27" font-weight="bold">Actual continuously moving, physically interacting cloths</text>',
'<text x="35" y="70" fill="#516578" font-family="Arial,sans-serif" font-size="14">Same XPBD-style solver, same geometry and forces. Real inter-sheet contact correction. Lower is better.</text>',
'<text x="35" y="91" fill="#516578" font-family="Arial,sans-serif" font-size="13">CPU milliseconds, warm-up excluded · every recorded trajectory compared against full SAP</text>']
for si,sheets in enumerate((1,2,3)):
 y=127+si*326
 svg.append(f'<text x="35" y="{y}" fill="#1b3144" font-family="Arial,sans-serif" font-weight="bold" font-size="20">{sheets} cloth sheet{"s" if sheets>1 else ""} + 2 moving spheres</text>')
 mx=max(float(data[sheets,p]["meanStepMs"]) for p in policies)
 for pi,p in enumerate(policies):
  yy=y+29+pi*34
  val=float(data[sheets,p]["meanStepMs"])
  broad=float(data[sheets,p]["meanBroadMs"])
  narrow=float(data[sheets,p]["meanNarrowMs"])
  svg.append(f'<text x="35" y="{yy+13}" fill="#365063" font-family="Arial,sans-serif" font-size="14">{escape(names[p])}</text>')
  width=max(1,610*val/max(mx,1e-10))
  svg.append(f'<rect x="300" y="{yy}" width="{width:.1f}" height="23" rx="4" fill="{colors[p]}"/>')
  svg.append(f'<text x="945" y="{yy+17}" fill="#142c40" font-family="Arial,sans-serif" font-size="14">{val:.3f} ms</text>')
  svg.append(f'<text x="1060" y="{yy+17}" fill="#516578" font-family="Arial,sans-serif" font-size="12">BP {broad:.3f}</text>')
svg.extend(['<text x="35" y="1140" fill="#516578" font-family="Arial,sans-serif" font-size="13">Regional certification is an experimental combination, not established publication novelty. No CCD or self-contact is claimed.</text>',
'<text x="35" y="1163" fill="#516578" font-family="Arial,sans-serif" font-size="13">Interpret alongside candidate counts, work rejected by revalidation, and the independent Bullet DBVT comparison.</text>',
'</svg>'])
chart.parent.mkdir(parents=True,exist_ok=True)
chart.write_text("\n".join(svg),encoding="utf-8")
lines=["# Multi-cloth research benchmark — generated from measured C++ data","",
"**Scope:** 1/2/3 continuously driven cloth sheets, two moving spheres, one custom XPBD-style solver, inter-sheet vertex–triangle and edge–edge discrete contact correction.",
"Every policy's particle states were compared against full SAP, with max allowed error below 1e-7.",
"These are not official IPC/OGC/CCFR simulation results and are not collision-free or CCD guarantees.","",
"| Sheets | Policy | Mean broad (ms) | Mean precise + response (ms) | Mean complete step (ms) | AABB candidates | Cross-sheet corrections | Rebuilt region blocks | Reused region blocks | Skipped region pairs | Guard pairs rejected | Online splits | Online merges | Controller cost (ms) | Max state error |",
"|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
for s in (1,2,3):
 for p in policies:
  r=data[s,p]
  f=lambda k,prec=3:f"{float(r[k]):.{prec}f}"
  lines.append(f'| {s} | {p} | {f("meanBroadMs")} | {f("meanNarrowMs")} | {f("meanStepMs")} | {f("meanCandidates",1)} | {f("meanInterContacts",1)} | {f("meanRegionBuilds",1)} | {f("meanRegionReuses",1)} | {f("meanSkippedRegionPairs",1)} | {f("meanRejected",1)} | {f("meanSplits",3)} | {f("meanMerges",3)} | {f("meanOnlineDecisionMs",4)} | {f("maxPositionError",9)} |')
lines += ["",
"## Interpretation and novelty boundary",
"",
"The *regional-certified* method divides stable cloth mesh topology into local patches, stores conservative envelope-pair candidate blocks for each patch pair, invalidates only blocks incident to escaping regions, and revalidates cached pairs against current exact padded AABBs. The *adaptive* variant estimates regional movement and observed guard false positives to adjust future budgets. This is a **proposed experimental combination**, not a proven historically novel broad-phase invention.",
"",
"Broad-phase query time may increase under dense contact because all current candidate pairs are checked and pair-list materialization has a cost. Negative results are valid results. A better policy must beat both SAP and existing dynamic BVH approaches on representative scenes without losing pair coverage.",
"",
"Compared with the earlier single static sphere, these runs include **genuine changing geometry and inter-sheet positional response**. However, our custom contact handling is discrete and approximate, has no cloth self-collision, and is not a robust scientific reference solver.",
"",
"## Independent published-library background",
"",
"- Bullet Physics dynamic AABB trees: https://github.com/bulletphysics/bullet3",
"- PhysX eABP, ePABP and region broad phases: https://github.com/NVIDIA-Omniverse/PhysX/blob/main/physx/include/PxBroadPhase.h",
"- Solver-Integrated Deformation-Aware BVH Updates, TVCG 2026, DOI 10.1109/TVCG.2026.3729180",
"- Efficient Self-Collision Culling for Real-Time Cloth Simulation Using Discrete Curvature Analysis, Mathematics 2026, DOI 10.3390/math14091504",
"",
"The literature substantially limits novelty claims for local selective updates, temporal reuse, and adaptivity. The prototype is intended to test a cost/selectivity hypothesis with conservative coverage, not to establish a publishable new method by assertion.",
""]
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text("\n".join(lines),encoding="utf-8")
print(f"Verified {len(rows)} scene/policy combinations and generated chart + report")


