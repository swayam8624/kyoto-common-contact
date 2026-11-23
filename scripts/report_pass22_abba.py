#!/usr/bin/env python3
"""Pass22B: audit ORIGINAL physical FEM isolated fully-paid ABBA duel.

Both candidate algorithms are instantiated twice independently, retain
their source state, and each is queried exactly once per physical frame.
Nothing here creates simulation data or substitutes for native results.
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
    "trial","step","scene_id","regime","source_vertices",
    "source_tetrahedra","physical_time_s","source_contact_updates",
    "oracle_candidates","cold_scene_frame","order_code",
    "selective_A1_wall_ns","selective_A2_wall_ns",
    "ipc_B1_wall_ns","ipc_B2_wall_ns",
    "selective_A1_pairs","selective_A2_pairs",
    "ipc_B1_pairs","ipc_B2_pairs","oracle_parity"
)
VALID_ORDERS={
    "A1-B1-B2-A2","A2-B2-B1-A1",
    "B1-A1-A2-B2","B2-A2-A1-B1"
}
VALID_REGIMES={"routine","fast_adaptive","uncoupled_multibody"}
TIMES=("selective_A1_wall_ns","selective_A2_wall_ns",
       "ipc_B1_wall_ns","ipc_B2_wall_ns")
PAIR_COUNTS=("selective_A1_pairs","selective_A2_pairs",
             "ipc_B1_pairs","ipc_B2_pairs")

def load(path:Path)->list[dict]:
    with path.open(encoding="utf-8",newline="") as f:
        reader=csv.DictReader(f)
        missing=set(FIELDS)-set(reader.fieldnames or ())
        if missing:raise ValueError(f"missing original ABBA CSV fields: {sorted(missing)}")
        rows=list(reader)
    if not rows:raise ValueError("missing native isolated ABBA evidence")
    seen=set()
    per_repeat=defaultdict(list)
    for row in rows:
        if row["regime"] not in VALID_REGIMES:
            raise ValueError("invalid source physical regime")
        if row["order_code"] not in VALID_ORDERS:
            raise ValueError("ABBA order is not a complete counterbalanced sequence")
        for k in FIELDS:
            if k in ("regime","order_code"):continue
            number=float(row[k])
            if not math.isfinite(number) or number<0:
                raise ValueError(f"nonfinite or negative field: {k}")
        frame=(int(row["trial"]),int(row["step"]))
        if frame in seen:raise ValueError(f"duplicate source interval {frame}")
        seen.add(frame)
        if int(row["oracle_parity"])!=1:
            raise ValueError(f"native candidate oracle failure: {frame}")
        if int(row["cold_scene_frame"]) not in (0,1):
            raise ValueError("invalid topology epoch/cold label")
        for k in PAIR_COUNTS:
            if int(row[k])!=int(row["oracle_candidates"]):
                raise ValueError(f"candidate mismatch {k} at {frame}")
        for k in TIMES:
            if int(row[k])<=0:
                raise ValueError(f"nonpositive fully paid clock {k} at {frame}")
        per_repeat[int(row["trial"])].append(row)
    if len(per_repeat)<2:
        raise ValueError("requires two or more native repeated ABBA trajectories")
    for trial,frames in per_repeat.items():
        scenes=defaultdict(list)
        for row in frames:scenes[int(row["scene_id"])].append(row)
        for scene,subset in scenes.items():
            cold=sum(int(row["cold_scene_frame"]) for row in subset)
            if cold!=1:
                raise ValueError(f"repeat {trial} scene {scene} has {cold} cold frames")
        if len({r["step"] for r in frames})!=len(frames):
            raise ValueError(f"same trial {trial} repeats physical source step")
    shapes={
        trial:{(int(row["step"]),int(row["scene_id"]),row["regime"])
               for row in frames}
        for trial,frames in per_repeat.items()
    }
    reference=next(iter(shapes.values()))
    if any(states!=reference for states in shapes.values()):
        raise ValueError("ABBA trial trajectories do not cover identical sources")
    return rows

def paid(rows:list[dict],prefix:str)->int:
    if prefix=="A":
        return sum(int(r["selective_A1_wall_ns"])+int(r["selective_A2_wall_ns"])
                   for r in rows)
    if prefix=="B":
        return sum(int(r["ipc_B1_wall_ns"])+int(r["ipc_B2_wall_ns"])
                   for r in rows)
    raise ValueError("invalid arm")

def subset_stats(rows:list[dict])->dict:
    a=paid(rows,"A")
    b=paid(rows,"B")
    a1=sum(int(r["selective_A1_wall_ns"]) for r in rows)
    a2=sum(int(r["selective_A2_wall_ns"]) for r in rows)
    b1=sum(int(r["ipc_B1_wall_ns"]) for r in rows)
    b2=sum(int(r["ipc_B2_wall_ns"]) for r in rows)
    return {
        "paired_rows":len(rows),
        "original_unique_steps":len({int(r["step"]) for r in rows}),
        "selective_two_instances_total_ns":a,
        "official_cached_ipc_two_instances_total_ns":b,
        "official_over_selective_ratio":b/a,
        "selective_A1_over_A2":a1/a2,
        "official_B1_over_B2":b1/b2,
        "a_outer_rows":sum(r["order_code"].startswith("A") for r in rows),
        "b_outer_rows":sum(r["order_code"].startswith("B") for r in rows),
        "contact_bearing_rows":sum(int(r["source_contact_updates"])>0 for r in rows),
    }

def summarize(rows:list[dict])->dict:
    warm=[r for r in rows if int(r["cold_scene_frame"])==0]
    cold=[r for r in rows if int(r["cold_scene_frame"])==1]
    by_scene=defaultdict(list)
    by_trial=defaultdict(list)
    by_order=defaultdict(list)
    for r in warm:
        by_scene[int(r["scene_id"])].append(r)
        by_trial[int(r["trial"])].append(r)
        by_order["A_outer" if r["order_code"].startswith("A") else "B_outer"].append(r)
    if not warm:
        raise ValueError("no warm steady-state original ABBA observations")
    scenes={}
    for scene,subset in sorted(by_scene.items()):
        one=subset[0]
        scenes[scene]={
            **subset_stats(subset),
            "physical_regime":one["regime"],
            "source_vertices":int(one["source_vertices"]),
            "source_tetrahedra":int(one["source_tetrahedra"]),
            "uncoupled_note":"No deformable/deformable physical response"
               if one["regime"]=="uncoupled_multibody" else None,
        }
    trial_stats={n:subset_stats(s) for n,s in sorted(by_trial.items())}
    order_stats={n:subset_stats(s) for n,s in sorted(by_order.items())}
    aorder=order_stats["A_outer"]["official_over_selective_ratio"]
    border=order_stats["B_outer"]["official_over_selective_ratio"]
    ratio_disagreement=max(aorder,border)/min(aorder,border)
    per_trial_ratios=[x["official_over_selective_ratio"]
                      for x in trial_stats.values()]
    return {
        "experiment":"Isolated fully-paid 4-arm ABBA on actual source FEM states",
        "source_scope":"two independent selective BVHs and two separately topology-cached official IPC Toolkit LBVH builds; each advanced exactly once per interval",
        "input":"original native physical 3D FEM states, geometry and contact measurements",
        "candidate_invariant":"each A1/A2/B1/B2 source set checked against exhaustive VF/EE oracle",
        "total_source_rows":len(rows),
        "cold_rows":len(cold),
        "warm_rows":len(warm),
        "warm_steady_state":subset_stats(warm),
        "cold_steady_state":subset_stats(cold),
        "per_source_scene":scenes,
        "per_trial":trial_stats,
        "per_order":order_stats,
        "order_ratio_disagreement_factor":ratio_disagreement,
        "median_per_trial_ipc_over_selective":statistics.median(per_trial_ratios),
        "all_repeats_favor_selective":all(v>1 for v in per_trial_ratios),
        "all_repeats_favor_ipc":all(v<1 for v in per_trial_ratios),
        "order_reliable_to_3_percent":ratio_disagreement<=1.03,
        "warnings":[
            "Warm/cold source epochs separated; cold time remains fully recorded",
            "Independent A1/A2 and B1/B2 instances prevent repeated same-frame refit from masquerading as warm speedup",
            "Still correlated physical source frames; four repeats are not independent solver/dataset replication",
            "No whole-solver, moving-moving IPC event timing, multithreaded or other published-BVH baseline measured",
            "A positive ratio is descriptive only. Small performance gaps without cross-hardware/order replication are inconclusive",
            "The multiobject source is two separately integrated FEM bodies, NOT coupled deformable-deformable response"
        ]
    }

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    original=load(args.input)
    report=summarize(original)
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/"pass22_isolated_abba_source_report.json").write_text(
        json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(report,indent=2))
        return
    scenes=sorted(report["per_source_scene"].items())
    names=[f"Scene {i}: {s['physical_regime']}" for i,s in scenes]
    ratios=[s["official_over_selective_ratio"] for _,s in scenes]
    fig,ax=plt.subplots(figsize=(10,5))
    ax.barh(names,ratios)
    ax.axvline(1,ls="--",color="gray")
    ax.set(xlabel="Pinned cached-input IPC / selective full paid wall time",
           title="ABBA actual FEM: 2 independent source-maintained instances per method")
    ax.grid(axis="x",alpha=.25)
    fig.tight_layout()
    fig.savefig(args.output/"22a_heldout_original_FEM_ABBA_by_scene.png",dpi=180)
    plt.close(fig)

    kinds=("A_outer","B_outer")
    fig,ax=plt.subplots(figsize=(8,4.5))
    ax.bar(kinds,[report["per_order"][k]["official_over_selective_ratio"]
                  for k in kinds])
    ax.axhline(1,ls="--",color="gray")
    ax.set(ylabel="Official IPC / selective paid wall time",
           title="Counterbalanced ABBA: size of benchmark order effect")
    ax.grid(axis="y",alpha=.25)
    fig.tight_layout()
    fig.savefig(args.output/"22b_ABBA_method_order_effect.png",dpi=180)
    plt.close(fig)
    print(json.dumps(report,indent=2))

if __name__=="__main__":
    main()
