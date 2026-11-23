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
    "repeat", "step", "regime", "scene_id",
    "source_vertices","source_edges","source_faces","source_tetrahedra",
    "physical_time_s", "source_face_impulse_updates",
    "reference_pairs", "official_ipc_pairs", "maintained_hierarchy_pairs",
    "official_extra", "official_missing", "official_conversion_ns",
    "official_build_ns", "official_query_ns", "maintained_paid_total_ns",
    "hierarchy_rebuilds", "hierarchy_refits", "first_method",
    "uncached_hierarchy_paid_ns", "uncached_hierarchy_pairs",
    "first_internal_hierarchy_method",
    "frontier_paid_total_ns","frontier_pairs","frontier_cut_size",
    "frontier_fallbacks","frontier_reused_node_pairs",
    "cached_ipc_conversion_ns","cached_ipc_build_ns",
    "cached_ipc_query_ns","cached_ipc_pairs",
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
            if k=="regime":
                if row[k] not in ("routine","fast_adaptive"):
                    raise ValueError(f"unrecognized source physics regime at row {i}")
                continue
            if k in ("first_method","first_internal_hierarchy_method"):
                allowed=("ipc","hierarchy") if k=="first_method" else ("cached","uncached","frontier")
                if row[k] not in allowed:
                    raise ValueError(f"invalid measured ordering at row {i}")
                continue
            value = float(row[k])
            if not math.isfinite(value) or value < 0:
                raise ValueError(f"bad {k} at row {i}")
        key = (int(row["repeat"]), int(row["step"]))
        if key in seen:
            raise ValueError(f"duplicate source snapshot/repeat {key}")
        seen.add(key)
        if int(row["cached_ipc_pairs"]) != int(row["official_ipc_pairs"]):
            raise ValueError(f"official IPC cached-input adapter changed candidates at {key}")
        if int(row["official_missing"]) != 0:
            raise ValueError(f"official IPC misses reference pairs at {key}")
        if int(row["frontier_pairs"]) != int(row["reference_pairs"]):
            raise ValueError(f"temporal frontier candidate parity failed at {key}")
        if int(row["uncached_hierarchy_pairs"]) != int(row["reference_pairs"]):
            raise ValueError(f"uncached hierarchy candidate parity failed at {key}")
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
        row["cached_ipc_paid_ns"]=sum(int(row[f"cached_ipc_{stage}_ns"])
            for stage in ("conversion","build","query"))
        if row["cached_ipc_paid_ns"]<=0:
            raise ValueError(f"invalid strong cached IPC timing at {key}")
        row["hierarchy_paid_ns"] = maintained
        row["frontier_paid_ns"]=int(row["frontier_paid_total_ns"])
        if row["frontier_paid_ns"]<=0:
            raise ValueError(f"invalid temporal frontier clock at {key}")
        row["uncached_paid_ns"] = int(row["uncached_hierarchy_paid_ns"])
        if row["uncached_paid_ns"]<=0:
            raise ValueError(f"invalid uncached clock observation at {key}")
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
    cached_ipc=[r["cached_ipc_paid_ns"] for r in rows]
    paid_atlas = [r["hierarchy_paid_ns"] for r in rows]
    ratios = [r["ratio_official_over_hierarchy"] for r in rows]
    old_totals=[r["uncached_paid_ns"] for r in rows]
    frontier_totals=[r["frontier_paid_ns"] for r in rows]
    per_repeat={}
    per_regime={}
    per_scene={}
    for sid in sorted({int(r["scene_id"]) for r in rows}):
        subset=[r for r in rows if int(r["scene_id"])==sid]
        keys=("source_vertices","source_edges","source_faces","source_tetrahedra")
        topology={k:sorted({int(r[k]) for r in subset}) for k in keys}
        if any(len(values)!=1 or values[0]<=0 for values in topology.values()):
            raise ValueError(f"inconsistent original FEM topology within scene {sid}")
        paidI=sum(r["official_paid_ns"] for r in subset)
        paidStrong=sum(r["cached_ipc_paid_ns"] for r in subset)
        paidC=sum(r["hierarchy_paid_ns"] for r in subset)
        paidF=sum(r["frontier_paid_ns"] for r in subset)
        per_scene[sid]={
            "physics_regime":subset[0]["regime"],
            "physical_intervals":len({int(r["step"]) for r in subset}),
            "repeated_paired_rows":len(subset),
            "topology":{k:values[0] for k,values in topology.items()},
            "contact_bearing_rows":sum(int(r["source_face_impulse_updates"])>0 for r in subset),
            "ipc_total_ns":paidI,
            "cached_input_ipc_total_ns":paidStrong,
            "cached_total_ns":paidC,
            "bounded_frontier_total_ns":paidF,
            "ipc_over_frontier":paidI/paidF,
            "cached_input_ipc_over_frontier":paidStrong/paidF,
            "cached_over_frontier":paidC/paidF,
        }
    for kind in ("routine","fast_adaptive"):
        subset=[r for r in rows if r["regime"]==kind]
        if not subset:
            raise ValueError(f"missing original {kind} solver evidence")
        per_regime[kind]={
            "rows":len(subset),
            "unique_source_intervals":len({int(r["step"]) for r in subset}),
            "ipc_total_ns":sum(r["official_paid_ns"] for r in subset),
            "cached_input_ipc_total_ns":sum(r["cached_ipc_paid_ns"] for r in subset),
            "cached_hierarchy_total_ns":sum(r["hierarchy_paid_ns"] for r in subset),
            "uncached_hierarchy_total_ns":sum(r["uncached_paid_ns"] for r in subset),
            "frontier_hierarchy_total_ns":sum(r["frontier_paid_ns"] for r in subset),
            "contact_bearing_rows":sum(
                int(r["source_face_impulse_updates"])>0 for r in subset)
        }
        per_regime[kind]["ipc_over_cached"]=(
            per_regime[kind]["ipc_total_ns"]/
            per_regime[kind]["cached_hierarchy_total_ns"])
        per_regime[kind]["cached_input_ipc_over_frontier"]=(
            per_regime[kind]["cached_input_ipc_total_ns"]/
            per_regime[kind]["frontier_hierarchy_total_ns"])
        per_regime[kind]["ipc_over_frontier"]=(
            per_regime[kind]["ipc_total_ns"]/
            per_regime[kind]["frontier_hierarchy_total_ns"])
        per_regime[kind]["uncached_over_cached"]=( 
            per_regime[kind]["uncached_hierarchy_total_ns"]/
            per_regime[kind]["cached_hierarchy_total_ns"])
    for rep in sorted({int(r["repeat"]) for r in rows}):
        batch=[r for r in rows if int(r["repeat"])==rep]
        per_repeat[rep]={
            "ipc_paid_ns":sum(r["official_paid_ns"] for r in batch),
            "cached_input_ipc_paid_ns":sum(r["cached_ipc_paid_ns"] for r in batch),
            "cached_paid_ns":sum(r["hierarchy_paid_ns"] for r in batch),
            "uncached_paid_ns":sum(r["uncached_paid_ns"] for r in batch),
            "frontier_paid_ns":sum(r["frontier_paid_ns"] for r in batch),
        }
        per_repeat[rep]["ipc_over_cached"]=(
            per_repeat[rep]["ipc_paid_ns"]/per_repeat[rep]["cached_paid_ns"])
        per_repeat[rep]["cached_input_ipc_over_frontier"]=(
            per_repeat[rep]["cached_input_ipc_paid_ns"]/
            per_repeat[rep]["frontier_paid_ns"])
        per_repeat[rep]["ipc_over_frontier"]=(
            per_repeat[rep]["ipc_paid_ns"]/per_repeat[rep]["frontier_paid_ns"])
        per_repeat[rep]["uncached_over_cached"]=( 
            per_repeat[rep]["uncached_paid_ns"]/per_repeat[rep]["cached_paid_ns"])
    warm = [r for r in rows if int(r["hierarchy_refits"]) > 0]
    cold = [r for r in rows if int(r["hierarchy_refits"]) == 0]
    contact = [r for r in rows if int(r["source_face_impulse_updates"]) > 0]
    stages = {
        "ipc_conversion_ns":sum(int(r["official_conversion_ns"]) for r in rows),
        "cached_ipc_conversion_ns":sum(int(r["cached_ipc_conversion_ns"]) for r in rows),
        "cached_ipc_build_ns":sum(int(r["cached_ipc_build_ns"]) for r in rows),
        "cached_ipc_query_ns":sum(int(r["cached_ipc_query_ns"]) for r in rows),
        "ipc_build_ns":sum(int(r["official_build_ns"]) for r in rows),
        "ipc_query_ns":sum(int(r["official_query_ns"]) for r in rows),
        "maintained_all_paid_ns":sum(paid_atlas),
        "uncached_hierarchy_all_paid_ns":sum(old_totals),
        "frontier_hierarchy_all_paid_ns":sum(frontier_totals)
    }
    return {
        "experiment": "Actual pinned official IPC Toolkit LBVH vs refittable feature hierarchy",
        "evidence_scope": "matched real FEM macro-step linearizations plus actual 20m/s adaptively accepted physical leaf intervals; feature candidates only",
        "candidate_coverage": "each native row checks exhaustive reference, tree exact parity, and IPC superset",
        "samples":len(rows),
        "per_repeat_totals":per_repeat,
        "per_source_physics_regime":per_regime,
        "per_source_scene":per_scene,
        "physical_scene_count":len(per_scene),
        "uncached_hierarchy_paid_total_ns":sum(old_totals),
        "frontier_hierarchy_paid_total_ns":sum(frontier_totals),
        "cached_input_official_ipc_paid_total_ns":sum(cached_ipc),
        "ratio_of_paid_totals_cached_input_ipc_over_frontier":
            sum(cached_ipc)/sum(frontier_totals),
        "ratio_of_paid_totals_original_ipc_over_cached_input_ipc":
            sum(paid_ipc)/sum(cached_ipc),
        "ratio_of_paid_totals_ipc_over_frontier":sum(paid_ipc)/sum(frontier_totals),
        "ratio_of_paid_totals_cached_over_frontier":sum(paid_atlas)/sum(frontier_totals),
        "total_source_frontier_budget_fallbacks":sum(
            int(r["frontier_fallbacks"]) for r in rows
            if int(r["step"])==max(
                int(t["step"]) for t in rows
                if t["repeat"]==r["repeat"])),
        "ratio_of_paid_totals_uncached_over_cached":sum(old_totals)/sum(paid_atlas),
        "median_per_repeat_uncached_over_cached":statistics.median(
            v["uncached_over_cached"] for v in per_repeat.values()),
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
            "Routine macro-step endpoints may approximate intermediate motion; fast-impact regime contains exact accepted physical-leaf start/end state pairs",
            "Snapshots and repeated timings are correlated; no independence or significance claim is made",
            "Repeated counterbalanced timings are descriptive; independent A/A controls and independent solver replication are still required",
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
    ax.scatter(x,[r["cached_ipc_paid_ns"]/1e6 for r in rows],
               alpha=.45,s=17,label="Pinned IPC with equally cached topology input (ms)")
    ax.scatter(x,[r["hierarchy_paid_ns"]/1e6 for r in rows],
               alpha=.5,s=22,label="Maintained cached hierarchy total paid (ms)")
    ax.scatter(x,[r["uncached_paid_ns"]/1e6 for r in rows],
               alpha=.4,s=10,label="Maintained uncached hierarchy control (ms)")
    ax.scatter(x,[r["frontier_paid_ns"]/1e6 for r in rows],
               alpha=.4,s=14,label="Retained node frontier total (ms)")
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
