#!/usr/bin/env python3
"""Validate and summarize actual Kyoto material benchmark CSVs."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import median

POLICIES = {
    "sap_rebuild", "global_guarded_exact", "selective_guard_exact",
    "region_fixed", "region_adaptive", "region_hierarchical", "online_cost_aware",
}

def percentile(values, p):
    values = sorted(values)
    return values[max(0, min(len(values)-1, int((len(values)-1)*p+.5)))]

def report(csvfile):
    with Path(csvfile).open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise RuntimeError(f"No rows in {csvfile}")
    by_group = defaultdict(list)
    all_frames = defaultdict(set)
    for r in rows:
        material = r["material"]
        policy = r["policy"]
        if policy not in POLICIES:
            raise RuntimeError(f"Unexpected method: {policy}")
        key = (material, r.get("regime","baseline"), int(r["requested_elements"]), int(r["seed"]))
        by_group[key,policy].append(r)
        all_frames[key].add(int(r["frame"]))
        if int(r["oracle_checked"]) and (int(r["misses"]) or int(r["extras"])):
            raise RuntimeError(f"Incorrect audited candidates in {csvfile}: {r}")
        if float(r["query_ms"]) < 0:
            raise RuntimeError("Negative elapsed time")
    output = [
        f"# Material broad-phase evidence: {Path(csvfile).name}",
        "",
        "Fixtures are approximate mass-spring simulations. Timings measure only CPU "
        "broad-phase query time, not world stepping, precise contact response, "
        "rendering or GPU execution.",
        "",
        "First four frames excluded from timing summary where possible. "
        "Values are descriptive, not confidence intervals.",
        "",
        "| Material | Regime | Budget | Actual AABBs | Policy | Median ms | P95 ms | "
        "Speedup vs Global Guard | Audits | Builds | Local repairs | Parallel frames |",
        "|---|---|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for key in sorted(all_frames):
        material,regime,budget,seed = key
        expected = all_frames[key]
        observed = {p for k,p in by_group if k==key}
        if observed != POLICIES:
            raise RuntimeError(f"Incomplete methods for {material}: {sorted(POLICIES-observed)}")
        def timings(policy):
            records = by_group[key,policy]
            frames = [int(r["frame"]) for r in records]
            if sorted(frames) != sorted(expected):
                raise RuntimeError(f"Missing or repeated {material}/{policy} frames")
            cut = 4 if len(expected)>8 else 0
            return [float(r["query_ms"]) for r in records if int(r["frame"])>=cut]
        baseline = median(timings("global_guarded_exact"))
        for policy in sorted(POLICIES):
            records = by_group[key,policy]
            durations = timings(policy)
            audits = sum(int(r["oracle_checked"]) for r in records)
            if audits==0:
                raise RuntimeError(f"Missing audits in {material}/{policy}")
            actual = int(records[0]["primitives"])
            builds = sum(int(r["region_builds"])+int(r["guard_builds"]) for r in records)
            repairs = sum(int(r["selective_repairs"]) for r in records)
            parallel = sum(int(r.get("parallel_workers",1))>1 for r in records)
            speedup = baseline/median(durations) if median(durations)>0 else float("inf")
            output.append(
                f"| {material} | {regime} | {budget} | {actual} | {policy} | "
                f"{median(durations):.5f} | {percentile(durations,.95):.5f} | "
                f"{speedup:.2f}x | {audits}/{len(records)} | {builds} | {repairs} | {parallel} |"
            )
    output += [
        "",
        "## Interpretation limits",
        "",
        "Global Guard and Selective Guard filter cached pairs against current "
        "padded bounds. Audited frames must match current AABB pairs. "
        "Above 512 primitives, the audit reference is SAP, not the exhaustive "
        "quadratic oracle. The separate generic online material controller "
        "is not the original cloth-specific OnlinePartitioner.",
        "",
        "A win on synthetic material fixtures cannot establish physical accuracy, "
        "novelty or general superiority. CPU broad-phase query time and full "
        "simulation step time must never be ranked together. GPU is separate.",
    ]
    return "\n".join(output)+"\n"

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("inputs", nargs="+",type=Path)
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    document="\n\n---\n\n".join(report(path) for path in args.inputs)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(document,encoding="utf-8")
        print(f"Wrote: {args.output}",flush=True)
    print(document)

if __name__=="__main__":
    main()
