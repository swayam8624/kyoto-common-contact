#!/usr/bin/env python3
"""Pass 15: conservative publication-gate summary of original paired IPC data.

Never fabricate a runtime. Parses only native C++ measurements. Does not
treat consecutive time steps as independent study replicates.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import platform
import statistics
from collections import defaultdict
from pathlib import Path

REQUIRED = (
    "repeat", "step", "physical_time_s", "source_face_impulse_updates",
    "reference_pairs", "official_ipc_pairs", "maintained_hierarchy_pairs",
    "official_extra", "official_missing", "official_conversion_ns",
    "official_build_ns", "official_query_ns", "maintained_paid_total_ns",
    "hierarchy_rebuilds", "hierarchy_refits", "first_method",
)


def read_data(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = set(REQUIRED) - set(reader.fieldnames or ())
        if missing:
            raise ValueError(f"missing original native fields: {sorted(missing)}")
        rows = list(reader)
    if not rows:
        raise ValueError("missing native paired benchmark data")
    seen = set()
    for i, row in enumerate(rows, 2):
        for k in REQUIRED:
            if k == "first_method":
                if row[k] not in ("ipc", "hierarchy"):
                    raise ValueError(f"invalid measured ordering at row {i}")
                continue
            value = float(row[k])
            if not math.isfinite(value) or value < 0:
                raise ValueError(f"bad {k} at row {i}")
        key = (int(row["repeat"]), int(row["step"]))
        if key in seen:
            raise ValueError(f"duplicate source snapshot/repeat {key}")
        seen.add(key)
        if int(row["official_missing"]) != 0:
            raise ValueError(f"official IPC misses reference pairs at {key}")
        if int(row["maintained_hierarchy_pairs"]) != int(row["reference_pairs"]):
            raise ValueError(f"maintained tree candidate parity failed at {key}")
        if (int(row["official_ipc_pairs"]) - int(row["reference_pairs"])
                != int(row["official_extra"])):
            raise ValueError(f"unexplained official conservative pairs at {key}")
        official = sum(int(row[f"official_{part}_ns"])
                       for part in ("conversion", "build", "query"))
        maintained = int(row["maintained_paid_total_ns"])
        if official <= 0 or maintained <= 0:
            raise ValueError(f"invalid paid native clock observation at {key}")
        row["official_paid_ns"] = official
        row["hierarchy_paid_ns"] = maintained
        row["ratio_official_over_hierarchy"] = official / maintained
    grouped = defaultdict(set)
    for row in rows:
        grouped[int(row["repeat"])].add(int(row["step"]))
    if len(grouped) < 2 or any(steps != next(iter(grouped.values()))
                                for steps in grouped.values()):
        raise ValueError("repeated native trials have nonmatching source states")
    return rows


def percentile_nearest(values, q):
    ordered = sorted(values)
    return ordered[math.ceil(q * len(ordered)) - 1]


def summarize(rows: list[dict]):
    paid_ipc = [r["official_paid_ns"] for r in rows]
    paid_atlas = [r["hierarchy_paid_ns"] for r in rows]
    ratios = [r["ratio_official_over_hierarchy"] for r in rows]
    warm = [r for r in rows if int(r["hierarchy_refits"]) > 0]
    cold = [r for r in rows if int(r["hierarchy_refits"]) == 0]
    contact = [r for r in rows if int(r["source_face_impulse_updates"]) > 0]
    stages = {
        "ipc_conversion_ns":sum(int(r["official_conversion_ns"]) for r in rows),
        "ipc_build_ns":sum(int(r["official_build_ns"]) for r in rows),
        "ipc_query_ns":sum(int(r["official_query_ns"]) for r in rows),
        "maintained_all_paid_ns":sum(paid_atlas)
    }
    return {
        "experiment": "Actual pinned official IPC Toolkit LBVH vs refittable feature hierarchy",
        "evidence_scope": "same solver-generated 3D FEM macro-step endpoint-linear snapshots; feature candidates only",
        "candidate_coverage": "each native row checks exhaustive reference, tree exact parity, and IPC superset",
        "samples":len(rows),
        "distinct_source_snapshots":len({int(r["step"]) for r in rows}),
        "repetitions":len({int(r["repeat"]) for r in rows}),
        "contact_bearing_rows":len(contact),
        "cold_hierarchy_rows":len(cold),
        "warm_hierarchy_rows":len(warm),
        "total_official_paid_ns":sum(paid_ipc),
        "total_maintained_paid_ns":sum(paid_atlas),
        "ratio_of_totals_official_over_maintained":sum(paid_ipc)/sum(paid_atlas),
        "geometric_mean_paired_ratio_official_over_maintained":statistics.geometric_mean(ratios),
        "median_paired_ratio":statistics.median(ratios),
        "minimum_paired_ratio":min(ratios),
        "maximum_paired_ratio":max(ratios),
        "official_p95_paid_ns":percentile_nearest(paid_ipc, .95),
        "maintained_p95_paid_ns":percentile_nearest(paid_atlas, .95),
        "official_extra_candidates_total":sum(int(r["official_extra"]) for r in rows),
        "stage_paid_breakdown":stages,
        "python_platform":platform.platform(),
        "warnings": [
            "This is NOT continuous narrowphase CCD parity, solver-step timing, or robust physical contact proof",
            "Original FEM macro-step endpoints do not reconstruct adaptive accepted physical leaf trajectories",
            "Snapshot correlation and only three repeats preclude independent uncertainty inference",
            "Three runs and alternating order provide preliminary descriptive timing, not calibrated A/A controls",
            "No measured speedup or winning regime may be claimed until CI passes on native source artifacts",
        ],
    }


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    rows=read_data(a.input)
    result=summarize(rows)
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/"pass15_paired_source_report.json").write_text(
        json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(result,indent=2))
        return
    x=[float(r["physical_time_s"]) for r in rows]
    fig,ax=plt.subplots(figsize=(10,5))
    ax.scatter(x,[r["official_paid_ns"]/1e6 for r in rows],
               alpha=.5,s=22,label="Official pinned IPC total paid (ms)")
    ax.scatter(x,[r["hierarchy_paid_ns"]/1e6 for r in rows],
               alpha=.5,s=22,label="Maintained hierarchy total paid (ms)")
    ax.set(xlabel="Actual source FEM physical time (s)",ylabel="Paid candidate generation (ms)",
           title="Feature candidate generation only: pinned IPC vs refittable hierarchy")
    ax.grid(alpha=.25);ax.legend();fig.tight_layout()
    fig.savefig(a.output/"15a_native_paid_time.png",dpi=200)
    plt.close(fig)

    fig,ax=plt.subplots(figsize=(10,5))
    normal=[r for r in rows if int(r["source_face_impulse_updates"])==0]
    contact=[r for r in rows if int(r["source_face_impulse_updates"])>0]
    for label,subset in (("non-contact FEM state",normal),("contact-bearing FEM state",contact)):
        if subset:
            ax.scatter([float(r["physical_time_s"]) for r in subset],
                       [r["ratio_official_over_hierarchy"] for r in subset],
                       s=20,alpha=.6,label=label)
    ax.axhline(1.0,color="gray",ls="--")
    ax.set(xlabel="Actual source FEM physical time (s)",
           ylabel="Official total paid / hierarchy total paid",
           title="Paired native feature-only ratio (unreplicated physical scene)")
    ax.grid(alpha=.25);ax.legend();fig.tight_layout()
    fig.savefig(a.output/"15b_paired_ratio_and_real_contact.png",dpi=200)
    plt.close(fig)

    fig,ax=plt.subplots(figsize=(10,5))
    for idx,field in enumerate(("official_conversion_ns","official_build_ns","official_query_ns")):
        ax.bar(idx,sum(int(r[field]) for r in rows)/1e6)
    ax.set_xticks(range(3),["Data conversion","LBVH build","Candidate query"])
    ax.set(ylabel="Sum across matched records (ms)",
           title="Official pinned LBVH: fully paid component costs")
    ax.grid(axis="y",alpha=.25);fig.tight_layout()
    fig.savefig(a.output/"15c_official_ipc_paid_components.png",dpi=200)
    plt.close(fig)
    print(json.dumps(result,indent=2))


if __name__=="__main__":
    main()
