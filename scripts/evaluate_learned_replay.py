#!/usr/bin/env python3
"""Report *measured* held-out switched-ML runtime against static baselines.

No invented extrapolations: aggregates paired full-frame elapsed time and exact
pair checks from actual ccp_material_replay CSVs with an ML artifact enabled.
Bootstrap confidence interval resamples independent trajectories (NOT frames).
A policy that loses to Global Guard must be reported as a loss.
"""
import argparse
import csv
import json
import math
import random
from collections import defaultdict,Counter
from pathlib import Path
from statistics import mean,median

STATIC=("sap_rebuild","sap_persistent_coherent","global_guarded_exact","selective_guard_exact",
        "region_fixed","region_adaptive","region_hierarchical",
        "online_cost_aware","bullet_dbvt_persistent")
LEARNED="ml_cost_aware"

def quantile(rows,fraction):
    ordered=sorted(rows)
    if not ordered:raise ValueError("cannot summarize an empty sample")
    return ordered[min(len(ordered)-1,max(0,round((len(ordered)-1)*fraction)))]

def evaluate(paths,warmup,bootstrap,seed):
    scenes=[]
    for path in paths:
        with Path(path).open(newline="",encoding="utf-8") as stream:
            records=list(csv.DictReader(stream))
        by_frame=defaultdict(dict)
        for row in records:
            frame=int(row["frame"])
            method=row["backend"]
            if method in by_frame[frame]:raise ValueError(f"duplicate policy: {path}, {method}/{frame}")
            by_frame[frame][method]=row
        frames=sorted(by_frame)
        if frames!=list(range(len(frames))) or len(frames)<=warmup+4:
            raise ValueError(f"incomplete trajectory or insufficient post-warmup frames: {path}")
        if any(LEARNED not in by_frame[f] for f in frames):
            raise ValueError(f"no deployed learned controller frames in {path}")
        common=set.intersection(*(set(by_frame[f]) for f in frames))
        if not set(("global_guarded_exact","online_cost_aware",LEARNED)).issubset(common):
            raise ValueError(f"required baseline missing: {path}")
        for frame in frames:
            rowset=by_frame[frame]
            oracle=rowset["sap_rebuild"]
            for key,row in rowset.items():
                if row["pairs"]!=oracle["pairs"] or row["pair_hash"]!=oracle["pair_hash"]:
                    raise ValueError(f"pair hash/count mismatch: {path} frame {frame} policy {key}")
                if int(row["misses"])!=0 or float(row["ms"])<0:
                    raise ValueError(f"invalid correctness or timing: {path} frame {frame}")
        samples={method:[float(by_frame[f][method]["ms"])
                          for f in frames if f>=warmup] for method in sorted(common)}
        medians={key:median(values) for key,values in samples.items()}
        means={key:mean(values) for key,values in samples.items()}
        selection=Counter(by_frame[f][LEARNED]["chosen_backend"]
                          for f in frames if f>=warmup)
        changes=sum(int(by_frame[f][LEARNED]["ml_switched"])
                    for f in frames if f>=warmup)
        scenes.append({
            "path":str(path),"n":int(records[0]["n"]),"material":records[0]["material"],
            "seed":records[0]["seed"],"frames":len(frames),
            "warmup_excluded":warmup,"measured_median_ms":medians,
            "measured_mean_ms":means,
            "measured_p95_ms":{key:quantile(values,.95) for key,values in samples.items()},
            "ml_vs_global_mean_speedup":means["global_guarded_exact"]/means[LEARNED],
            "ml_vs_original_online_mean_speedup":means["online_cost_aware"]/means[LEARNED],
            "ml_vs_best_static_posthoc_mean_speedup":
                min(value for key,value in means.items() if key in STATIC)/means[LEARNED],
            "ml_action_counts":dict(selection),"ml_switches":changes,
            "every_policy_pairs_verified":True})
    if not scenes:raise ValueError("no replay evidence")
    ratios=[s["ml_vs_global_mean_speedup"] for s in scenes]
    def geomean(values):return math.exp(mean([math.log(v) for v in values]))
    rng=random.Random(seed)
    draws=[]
    for _ in range(bootstrap):
        sampled=[ratios[rng.randrange(len(ratios))] for _ in ratios]
        draws.append(geomean(sampled))
    aggregate={
      "trajectories":len(scenes),
      "per_trajectory_paired_geometric_mean_ml_vs_global":geomean(ratios),
      "bootstrap_trajectory_95pct_ci":[quantile(draws,.025),quantile(draws,.975)],
      "ml_trajectories_faster_than_global":sum(r>1 for r in ratios),
      "ml_trajectories_slower_than_global":sum(r<1 for r in ratios),
      "cannot_claim_general_superiority":True,
      "warning":"Posthoc best static algorithm is a hindsight comparator, NOT deployable; "
                "model selection overhead is INCLUDED in learned runtime. "
                "Independent trajectories/seed splits and memory still need verification."
    }
    return scenes,aggregate

def render(scenes,summary):
    output=[
       "# Held-out real CPU learned broad-phase: measured closed-loop results",
       "",
       "**Measured**, not inferred from proxy neural-network loss. All methods "
       "return matching current-AABB candidate pair sets on every frame.",
       "",
       "| Scene | AABBs | ML mean ms | Global mean ms | Original online mean ms | "
       "ML / Global speedup | ML switches |",
       "|---|---:|---:|---:|---:|---:|---:|"
    ]
    for row in scenes:
        name=Path(row["path"]).stem
        m=row["measured_mean_ms"]
        output.append(f'| {name} | {row["n"]} | {m[LEARNED]:.5f} | '
                      f'{m["global_guarded_exact"]:.5f} | '
                      f'{m["online_cost_aware"]:.5f} | '
                      f'{row["ml_vs_global_mean_speedup"]:.3f}x | '
                      f'{row["ml_switches"]} |')
    interval=summary["bootstrap_trajectory_95pct_ci"]
    output.extend([
        "",
        "## Across independent trajectories",
        "",
        f'- Per-trajectory geometric-mean speedup against Global Guard: '
        f'{summary["per_trajectory_paired_geometric_mean_ml_vs_global"]:.3f}x.',
        f'- 95% trajectory-bootstrap interval: [{interval[0]:.3f}, {interval[1]:.3f}]x.',
        f'- Scenes faster: {summary["ml_trajectories_faster_than_global"]}; '
        f'slower: {summary["ml_trajectories_slower_than_global"]}.',
        "",
        "**Important:** small Ns or a wide interval cannot establish "
        "statistical significance. All timings are isolated CPU broad-phase "
        "queries including learned inference, feature extraction and cache "
        "switch penalties; these are not full coupled simulation step times. "
        "No claim about GPU learned execution is made.",
        "",
        "Detailed per-scene P95, chosen backends, all algorithm timings and "
        "original metadata are preserved in the accompanying JSON."
    ])
    return "\n".join(output)+"\n"

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files",nargs="+",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--warmup",type=int,default=8)
    parser.add_argument("--bootstrap",type=int,default=2000)
    parser.add_argument("--seed",type=int,default=145)
    args=parser.parse_args()
    if args.warmup<0 or args.bootstrap<100:
        parser.error("invalid warmup or confidence interval configuration")
    scenes,summary=evaluate(args.files,args.warmup,args.bootstrap,args.seed)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if args.output.exists():parser.error("refusing to overwrite existing heldout report")
    report=render(scenes,summary)
    args.output.write_text(report,encoding="utf-8")
    args.output.with_suffix(".json").write_text(
        json.dumps({"aggregate":summary,"scenes":scenes},indent=2)+"\n",
        encoding="utf-8")
    print(report)

if __name__=="__main__":
    main()
