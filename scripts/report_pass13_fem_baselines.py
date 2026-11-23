#!/usr/bin/env python3
"""Pass 13: source-only benchmark summary; never invent performance data.

This reports PAID rebuilt-SAP and exhaustive-reference build+query times
from the native C++ FEM CSV. It is not a maintained SCA/official IPC trial.
"""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    with a.input.open(encoding="utf-8",newline="") as stream:
        rows=list(csv.DictReader(stream))
    if len(rows)<2:
        raise ValueError("insufficient original native benchmark rows")
    required=("step","time_s","exact_pairs","sap_pairs",
              "reference_build_ns","reference_query_ns",
              "sap_build_ns","sap_query_ns","source_face_impulse_updates","persistent_total_ns",
              "persistent_warm_frames","persistent_invalidations",
              "hierarchy_total_ns","hierarchy_rebuilds","hierarchy_refits")
    for i,row in enumerate(rows):
        for key in required:
            if key not in row:
                raise ValueError(f"missing original column {key}")
            v=float(row[key])
            if not math.isfinite(v) or v<0:
                raise ValueError(f"bad original value {key} in row {i}")
        if int(row["exact_pairs"])!=int(row["sap_pairs"]):
            raise ValueError(f"candidate parity failure at row {i}")
    def duration(row,prefix):
        return int(row[f"{prefix}_build_ns"])+int(row[f"{prefix}_query_ns"])
    exact=[duration(r,"reference") for r in rows]
    sap=[duration(r,"sap") for r in rows]
    persistent=[int(r["persistent_total_ns"]) for r in rows]
    hierarchy=[int(r["hierarchy_total_ns"]) for r in rows]
    if min(exact)<=0 or min(sap)<=0 or min(persistent)<=0 or min(hierarchy)<=0:
        raise ValueError("nonpositive monotonic-clock timings")
    ratios=[x/y for x,y in zip(exact,sap)]
    summary={
        "scope":"original physically integrated FEM sampled intervals; independently rebuilt exhaustive versus SAP only",
        "snapshots":len(rows),
        "contact_snapshots":sum(int(r["source_face_impulse_updates"])>0 for r in rows),
        "source_candidate_pairs":sum(int(r["exact_pairs"]) for r in rows),
        "sap_candidate_pairs":sum(int(r["sap_pairs"]) for r in rows),
        "exhaustive_paid_total_ns":sum(exact),
        "sap_paid_total_ns":sum(sap),
        "persistent_order_paid_total_ns":sum(persistent),
        "maintained_hierarchy_paid_total_ns":sum(hierarchy),
        "ratio_of_paid_totals_rebuilt_sap_over_hierarchy":sum(sap)/sum(hierarchy),
        "hierarchy_rebuilds_during_source_snapshots":int(rows[-1]["hierarchy_rebuilds"]),
        "hierarchy_refits_during_source_snapshots":int(rows[-1]["hierarchy_refits"]),
        "ratio_of_paid_totals_rebuilt_sap_over_persistent_order":sum(sap)/sum(persistent),
        "persistent_warm_frames":int(rows[-1]["persistent_warm_frames"]),
        "persistent_invalidations_during_source_snapshots":int(rows[-1]["persistent_invalidations"]),
        "ratio_of_paid_totals_exhaustive_over_sap":sum(exact)/sum(sap),
        "median_paired_ratio_exhaustive_over_sap":statistics.median(ratios),
        "min_paired_ratio_exhaustive_over_sap":min(ratios),
        "max_paired_ratio_exhaustive_over_sap":max(ratios),
        "warning":"The maintained hierarchy is a first full-feature refit experiment, not a locally updated contact frontier. Not a fair SCA vs IPC result or full solver-step timing. One fixed-order pass; no counterbalancing, A/A, confidence interval, or independent replicate."
    }
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/"pass13_reference_summary.json").write_text(
        json.dumps(summary,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(summary,indent=2))
        return
    time=[float(r["time_s"]) for r in rows]
    fig,ax=plt.subplots(figsize=(10,5))
    ax.plot(time,[v/1e6 for v in exact],marker="o",
            label="Exhaustive build + query (ms)")
    ax.plot(time,[v/1e6 for v in sap],marker=".",
            label="Rebuilt SAP build + query (ms)")
    ax.plot(time,[v/1e6 for v in persistent],marker="x",
            label="Persistent-order SAP total (ms)")
    ax.plot(time,[v/1e6 for v in hierarchy],marker="s",
            label="Maintained hierarchy total (ms)")
    ax.set(xlabel="Physical FEM time (s)",ylabel="Measured paid time (ms)",
           title="Paid native feature costs; no official IPC or solver-integrated SCA")
    ax.grid(alpha=.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(a.output/"13a_baseline_costs.png",dpi=180)
    plt.close(fig)
    fig,ax=plt.subplots(figsize=(10,5))
    ax.scatter(time,ratios,c=["tab:orange" if int(r["source_face_impulse_updates"])>0
                              else "tab:blue" for r in rows])
    ax.axhline(1,color="gray",ls="--")
    ax.set(xlabel="Physical FEM time (s)",
           ylabel="Exhaustive / SAP paid-time ratio",
           title="Observed per-snapshot reference ratio; orange = actual contact")
    ax.grid(alpha=.25)
    fig.tight_layout()
    fig.savefig(a.output/"13b_paired_baseline_ratios.png",dpi=180)
    plt.close(fig)
    print(json.dumps(summary,indent=2))

if __name__=="__main__":
    main()
