#!/usr/bin/env python3
"""Create honest, reproducible broad-phase comparison evidence (stdlib only).

The upstream reference is Bullet's real btDbvtBroadphase, not a homemade BVH
labeled 'published'. Static rebuild costs are measured on identical inputs.
Guarded reuse intentionally returns safe supersets; candidate inflation matters.
"""
import csv
import math
from pathlib import Path
import sys
from xml.sax.saxutils import escape

if len(sys.argv)!=4:
    raise SystemExit("usage: summarize_leaderboard.py INPUT.csv OUTPUT.svg OUTPUT.md")
csv_file, svg_file, md_file=map(Path,sys.argv[1:])
with csv_file.open(newline="") as fh:
    rows=list(csv.DictReader(fh))
expected={"scene","algorithm","primitives","samples","meanMs","medianMs",
          "p95Ms","meanCandidates","meanExtraCandidates","builds","reuses","misses"}
if not rows or not expected.issubset(rows[0]):
    raise SystemExit("Missing or incompatible genuine Bullet broad-phase CSV")
cases=["sparse256","clustered512","dense256","sheets2","sheets3","simulated_cloths3"]
algos=["exhaustive_aabb","sweep_prune","bullet_dbvt","bullet_dbvt_dynamic","guarded_sap","region_fixed","region_adaptive"]
by_key={}
for row in rows:
    key=(row["scene"],row["algorithm"])
    if key in by_key:raise SystemExit(f"duplicate benchmark result: {key}")
    if int(row["misses"])!=0:raise SystemExit(f"required contact candidate missed: {key}")
    for field in ["meanMs","medianMs","p95Ms","meanCandidates","meanExtraCandidates"]:
        if not math.isfinite(float(row[field])) or float(row[field])<0:
            raise SystemExit(f"invalid {field}: {key}")
    by_key[key]=row
if set(by_key)!={(s,a) for s in cases for a in algos}:
    raise SystemExit("Comparative matrix incomplete; never silently omit baselines")

for path in (svg_file,md_file):path.parent.mkdir(parents=True,exist_ok=True)
def num(v,d=4):return f"{float(v):.{d}f}"
def label(s):return s.replace("_"," ").title().replace("Dbvt","DBVT").replace("Sap","SAP")
# One result chart showing all algorithms, separately per scene. No fake
# comparison against GPU LBVH or Box2D's 2D dynamic tree.
W,H=1200,1380
palette={"exhaustive_aabb":"#b88b50","sweep_prune":"#4a82bb",
         "bullet_dbvt":"#9c79c6","bullet_dbvt_dynamic":"#497fba","guarded_sap":"#238a80","region_fixed":"#b06d98","region_adaptive":"#246b62"}
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
'<rect width="1200" height="1380" fill="#f9fbfd"/>',
'<text x="42" y="48" font-family="Arial,sans-serif" fill="#16283c" font-size="28" font-weight="bold">3D Broad-Phase: Identical Bounds, Independent Baselines</text>',
'<text x="42" y="76" font-family="Arial,sans-serif" fill="#536475" font-size="16">Actual Bullet DBVT (upstream library) · Exhaustive AABB · SAP · Guarded SAP</text>',
'<text x="42" y="100" font-family="Arial,sans-serif" fill="#536475" font-size="13">Mean CPU full-query time in ms (lower is better); results are for this machine and synthetic workloads</text>']
for si,scene in enumerate(cases):
    y=145+si*196
    svg.append(f'<text x="42" y="{y}" fill="#16283c" font-family="Arial,sans-serif" font-size="17" font-weight="bold">{escape(label(scene))}</text>')
    mx=max(float(by_key[scene,a]["meanMs"]) for a in algos)
    mx=max(mx,1e-10)
    for ai,a in enumerate(algos):
        yy=y+9+ai*23
        val=float(by_key[scene,a]["meanMs"])
        width=max(1,380*val/mx)
        x=265+ai//2*435
        yrow=yy+(0 if ai%2==0 else 22)
        # two columns: each method one row, stacked vertically to fit
    for ai,a in enumerate(algos):
        yy=y+16+ai*22
        val=float(by_key[scene,a]["meanMs"])
        width=max(1,630*val/mx)
        svg.append(f'<rect x="260" y="{yy-13}" width="{width:.2f}" height="16" rx="3" fill="{palette[a]}"/>')
        svg.append(f'<text x="42" y="{yy}" font-family="Arial,sans-serif" font-size="12" fill="#3b4b5f">{escape(label(a))}</text>')
        svg.append(f'<text x="910" y="{yy}" font-family="Arial,sans-serif" font-size="13" fill="#16283c">{val:.5f} ms</text>')
svg.append('<text x="42" y="1359" font-family="Arial,sans-serif" font-size="13" fill="#536475">Guarded reuse may emit conservative extra pairs. This chart measures broad phase only; NO simulated cloth-cloth response.</text>')
svg.append('</svg>')
svg_file.write_text("\n".join(svg),encoding="utf-8")
lines=["# Verified 3D broad-phase comparison","",
       "**Important:** this is a comparison against the **actual upstream Bullet Physics btDbvtBroadphase**.",
       "It is a serious, published-family 3D BVH baseline, **not** a claim to have beaten the world's best broad phase.",
       "All approaches receive the same sampled 3D bounds and bilateral category/mask filtering.",
       "The exhaustive AABB result is the correctness oracle. Every tested method must include all its required pairs.",
       "Bullet and guarded SAP may produce conservative extra pairs; that affects follow-up narrow-phase workload.",
       "Generated scene sheets are moving geometric AABB fixtures—not physically simulated coupled cloth.",
       "Each algorithm includes its own construction and candidate-list materialization in the measured duration.",
       "Oracle generation and correctness checking occur outside the timed window.","",
       "| Scenario | Primitives | Method | Mean broad (ms) | p95 (ms) | Mean candidates | Mean extra candidates | Builds | Cache reuses | Bullet updates | Misses |",
       "|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
for scenario in cases:
    for algorithm in algos:
        r=by_key[scenario,algorithm]
        lines.append(f'| {scenario} | {r["primitives"]} | {algorithm} | {num(r["meanMs"],6)} | {num(r["p95Ms"],6)} | {num(r["meanCandidates"],1)} | {num(r["meanExtraCandidates"],1)} | {r["builds"]} | {r["reuses"]} | {r["updates"]} | {r["misses"]} |')
lines.extend(["",
"## Interpreting the results",
"",
"The independent benchmark tests **spatial candidate discovery**, not contact impulses, precise triangle geometry, CCD or solver convergence.",
"The `simulated_cloths3` case records 32 actual configurations produced by the same C++ multi-cloth XPBD-style contact-response simulation. This is **not** synthetic sinusoidal geometry, and the method timings exclude the cost of producing those snapshots.",
"The guarded policy's candidate count can be larger than freshly regenerated pairs. Whether its saved builds improve end-to-end contact time depends on the *extra precise tests* a caller must run.",
"To avoid a weakened external baseline, we compare both **freshly reconstructed** Bullet DBVT and **one persistent Bullet DBVT updated on every geometry change**. Those are different computational regimes; persistent Bullet reuses its spatial tree but does not reuse a frozen pair list.",
"Those extra tests have NOT been modeled as invented time savings. The existing live cloth benchmark separately measures real candidate-only precise work, whole-step time, and validation.",
"",
"## Literature and implementation",
"",
"- Bullet Physics real DBVT baseline: https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/BroadphaseCollision/btDbvtBroadphase.h",
"- Erin Catto's Box2D dynamic tree design documentation (related family, 2D, not executed here): https://box2d.org/documentation/group__tree.html",
"- Tero Karras, *Maximizing Parallelism in the Construction of BVHs, Octrees, and k-d Trees* (HPG 2012). GPU LBVH is **not** evaluated here: https://research.nvidia.com/publication/2012-06_maximizing-parallelism-construction-bvhs-octrees-and-k-d-trees",
"- Cohen et al., *I-COLLIDE* (1995), historical sweep-and-prune foundation: https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/cohen95.pdf",
"",
"## Novelty boundary",
"",
"Neither SAP, DBVT, fat AABBs, nor temporal pair caches are novel by themselves.",
"The research hypothesis to test is a conservative, **method-aware, query-contract-based** reuse interface with measured full-pipeline benefits across distinct solver geometries and semantics.",
"That cross-simulator hypothesis is **not yet demonstrated**. Current novelty is an implementation/investigation direction, not a verified publication-level novel algorithm.",
""])
md_file.write_text("\n".join(lines),encoding="utf-8")
print(f"Verified {len(rows)} complete benchmark rows, 0 misses. Created {svg_file} and {md_file}")
