#!/usr/bin/env python3
"""Analyze native Pass 20 SYNTHETIC 3D feature motion-sparsity receipts.

This is a controlled geometry stress test, NOT original solver data.
No synthetic result may be presented as a FEM or contact-physics speedup.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

FIELDS=(
    "source_kind","seed","moving_triangles","total_triangles","trial","frame",
    "moving_vertex_paths","oracle_candidates","first_method",
    "full_A_paid_ns","full_B_paid_ns","selective_paid_ns",
    "selective_dirty_vertices","selective_unchanged_refits",
    "selective_partial_refits",
)

def read_native(path:Path)->list[dict]:
    with path.open("r",encoding="utf-8",newline="") as f:
        reader=csv.DictReader(f)
        if set(FIELDS)-set(reader.fieldnames or []):
            raise ValueError("native synthetic feature receipt columns missing")
        rows=list(reader)
    if not rows:raise ValueError("no native feature stress data")
    seen=set()
    for row in rows:
        if row["source_kind"]!="synthetic_not_physical_FEM":
            raise ValueError("source label changed; may misrepresent synthetic input")
        if row["first_method"] not in ("full_A","full_B","selective"):
            raise ValueError("invalid recorded native method ordering")
        key=(int(row["moving_triangles"]),int(row["trial"]),int(row["frame"]))
        if key in seen:raise ValueError(f"duplicate control frame {key}")
        seen.add(key)
        numeric=[k for k in FIELDS if k not in
                 ("source_kind","first_method")]
        if any(not math.isfinite(float(row[k])) or float(row[k])<0 for k in numeric):
            raise ValueError(f"nonfinite or negative native measurement at {key}")
        for stage in ("full_A_paid_ns","full_B_paid_ns","selective_paid_ns"):
            if int(row[stage])<=0:
                raise ValueError(f"nonpositive native paid timing {stage} at {key}")
        n=int(row["total_triangles"])
        moving=int(row["moving_triangles"])
        frame=int(row["frame"])
        expected_dirty=3*n if frame==0 else 3*moving
        if int(row["selective_dirty_vertices"])!=expected_dirty:
            raise ValueError(f"incorrect selective touched vertices at {key}: expected {expected_dirty}")
    return rows

def summarize(rows:list[dict])->dict:
    grouped=defaultdict(list)
    for r in rows:
        grouped[int(r["moving_triangles"])].append(r)
    cohorts={}
    for moved,group in sorted(grouped.items()):
        total=int(group[0]["total_triangles"])
        if any(int(row["total_triangles"])!=total for row in group):
            raise ValueError("mixed source feature topology in one cohort")
        full_a=sum(int(r["full_A_paid_ns"]) for r in group)
        full_b=sum(int(r["full_B_paid_ns"]) for r in group)
        selective=sum(int(r["selective_paid_ns"]) for r in group)
        reps=sorted(set(int(r["trial"]) for r in group))
        per_trial={}
        for trial in reps:
            rs=[r for r in group if int(r["trial"])==trial]
            na=sum(int(r["full_A_paid_ns"]) for r in rs)
            nb=sum(int(r["full_B_paid_ns"]) for r in rs)
            ns=sum(int(r["selective_paid_ns"]) for r in rs)
            per_trial[trial]={
                "frames":len(rs),
                "full_A_over_full_B":na/nb,
                "mean_full_over_selective":(.5*(na+nb))/ns,
                "first_selective_count":sum(r["first_method"]=="selective" for r in rs)
            }
        cohorts[moved]={
            "source_kind":"synthetic_features_NOT_physical_solver",
            "active_triangle_fraction":moved/total,
            "frames":len(group),
            "seeds":len(reps),
            "full_A_total_ns":full_a,
            "full_B_total_ns":full_b,
            "selective_total_ns":selective,
            "full_A_over_full_B":full_a/full_b,
            "mean_full_over_selective":(.5*(full_a+full_b))/selective,
            "per_trial":per_trial,
            "median_per_trial_mean_full_over_selective":
                statistics.median(v["mean_full_over_selective"] for v in per_trial.values()),
            "caveat":"feature-only controlled motion; synthetic 3D trajectories, no physically integrated FEM"
        }
    return {
        "experiment":"Pass20 controlled native feature-motion sparsity crossover",
        "source_truth":"synthetic 3D feature paths; NOT solver-produced trajectories",
        "candidate_oracle":"each frame independently exact feature-set compared with exhaustive VF/EE enumeration in C++",
        "moving_fraction_cohorts":cohorts,
        "scientific_limits":[
            "No real FEM physics, contact response, full solver timing or novelty established",
            "Three seeded synthetic feature selections are not independent physical solvers",
            "Per-frame timings are highly correlated and affected by branch order and CPU noise",
            "A/A full-refit controls expose timing instability; small improvements cannot be called significant",
            "Use original pinned IPC FEM studies for physical performance interpretation"
        ]
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    args=p.parse_args()
    rows=read_native(args.input)
    data=summarize(rows)
    args.output.mkdir(parents=True,exist_ok=True)
    out=args.output/"pass20_synthetic_motion_sparsity.json"
    out.write_text(json.dumps(data,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(data,indent=2))
        return
    cohorts=sorted(data["moving_fraction_cohorts"].values(),
                   key=lambda c:c["active_triangle_fraction"])
    x=[c["active_triangle_fraction"]*100 for c in cohorts]
    y=[c["mean_full_over_selective"] for c in cohorts]
    fig,ax=plt.subplots(figsize=(9,5))
    ax.plot(x,y,marker="o",lw=2,label="Paid full-refit average / selective")
    for c in cohorts:
        q=c["active_triangle_fraction"]*100
        for rep in c["per_trial"].values():
            ax.scatter([q],[rep["mean_full_over_selective"]],
                       alpha=.35,s=20)
    ax.axhline(1.0,ls="--",color="gray",label="Equal measured time")
    ax.set(xlabel="Synthetic moving triangle fraction (%)",
           ylabel="Mean two full-refit controls / selective paid time",
           title="Controlled 3D feature-only sparsity crossover — NOT FEM physics")
    ax.legend();ax.grid(alpha=.25);fig.tight_layout()
    fig.savefig(args.output/"20a_paid_synthetic_sparsity_crossover.png",dpi=180)
    plt.close(fig)
    print(json.dumps(data,indent=2))

if __name__=="__main__":main()
