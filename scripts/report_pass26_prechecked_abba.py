#!/usr/bin/env python3
"""Pass 26: audit ORIGINAL FEM four-arm child-overlap-prechecked-vs-incumbent receipts.

No synthetic physical input may be accepted as a research measurement.
Repeated observations from a single simulated trajectory are correlated;
uncertainty across 8 repeated executions is NOT solver generalization.
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
    "source_kind","scene","step","trial","order_code","cold_epoch",
    "source_tetrahedra","source_vertices","physical_time_s",
    "source_contact_impulses","accepted_physical_leaves",
    "oracle_candidates","dirty_vertices",
    "child_precheck_enabled_A1","child_precheck_enabled_A2",
    "A1_prechecked_paid_ns","A2_prechecked_paid_ns",
    "B1_incumbent_paid_ns","B2_incumbent_paid_ns",
    "A1_pairs","A2_pairs","B1_pairs","B2_pairs"
)
ORDERS=(
    "A1-B1-B2-A2","A2-B2-B1-A1",
    "B1-A1-A2-B2","B2-A2-A1-B1"
)
CLOCKS=("A1_prechecked_paid_ns","A2_prechecked_paid_ns",
        "B1_incumbent_paid_ns","B2_incumbent_paid_ns")


def load(path:Path)->list[dict]:
    with path.open(encoding="utf-8",newline="") as source:
        reader=csv.DictReader(source)
        if set(FIELDS)-set(reader.fieldnames or []):
            raise ValueError("missing paid native ABBA receipt fields")
        rows=list(reader)
    if not rows:
        raise ValueError("original FEM ABBA native receipt is empty")
    seen=set()
    scenes=defaultdict(set)
    cold=defaultdict(int)
    first_method=defaultdict(set)
    for row in rows:
        if row["source_kind"]!="original_physically_integrated_FEM":
            raise ValueError("nonphysical source label in experimental receipt")
        if row["order_code"] not in ORDERS:
            raise ValueError("unrecognized ABBA rotation")
        for field in FIELDS:
            if field in ("source_kind","order_code"):
                continue
            value=float(row[field])
            if not math.isfinite(value) or value<0:
                raise ValueError(f"invalid native data in {field}")
        for col in CLOCKS:
            if int(row[col])<=0:
                raise ValueError(f"nonpositive paid native clock {col}")
        if int(row["cold_epoch"]) not in (0,1):
            raise ValueError("unrecognized source epoch marker")
        for col in ("A1_pairs","A2_pairs","B1_pairs","B2_pairs"):
            if int(row[col])!=int(row["oracle_candidates"]):
                raise ValueError(f"candidate oracle mismatch for {col}")
        if int(row["dirty_vertices"])>int(row["source_vertices"]):
            raise ValueError("dirty feature fraction greater than 1")
        key=(int(row["scene"]),int(row["trial"]),int(row["step"]))
        if key in seen:raise ValueError("duplicate physical source frame/trial")
        seen.add(key)
        scene,trial,step=key
        scenes[(scene,trial)].add(step)
        cold[(scene,trial)]+=int(row["cold_epoch"])
        first_method[(scene,trial)].add(row["order_code"])
    trials={trial for _,trial in scenes}
    if len(trials)<4:raise ValueError("insufficient repeated ABBA trials")
    scene_ids={scene for scene,_ in scenes}
    if len(scene_ids)<2:raise ValueError("missing independent real FEM mesh variants")
    expected={(scene,trial) for scene in scene_ids for trial in trials}
    if set(scenes)!=expected:
        raise ValueError("unequal physical scenes across repeated trials")
    reference={scene:next(iter(scenes[(scene,trial)] for trial in sorted(trials)))
               for scene in scene_ids}
    for scene,trial in expected:
        if scenes[(scene,trial)]!=reference[scene]:
            raise ValueError("physical source frames changed between repeats")
        if cold[(scene,trial)]!=1:
            raise ValueError("each physical scene/trial needs exactly one cold frame")
        if len(first_method[(scene,trial)])<2:
            raise ValueError("ABBA algorithm ordering was not counterbalanced")
    return rows


def summary(rows:list[dict])->dict:
    warm=[r for r in rows if int(r["cold_epoch"])==0]
    if not warm:raise ValueError("no steady-state physical FEM source")
    def group_stats(sample):
        if not sample:return None
        a1=sum(int(r["A1_prechecked_paid_ns"]) for r in sample)
        a2=sum(int(r["A2_prechecked_paid_ns"]) for r in sample)
        b1=sum(int(r["B1_incumbent_paid_ns"]) for r in sample)
        b2=sum(int(r["B2_incumbent_paid_ns"]) for r in sample)
        return {
            "rows":len(sample),
            "original_unique_physical_steps":len({(r["scene"],r["step"]) for r in sample}),
            "optimized_two_instance_paid_ns":a1+a2,
            "incumbent_two_instance_paid_ns":b1+b2,
            "incumbent_over_optimized":(b1+b2)/(a1+a2),
            "optimized_A1_over_A2":a1/a2,
            "incumbent_B1_over_B2":b1/b2,
            "mean_dirty_vertex_fraction":statistics.mean(
                int(r["dirty_vertices"])/int(r["source_vertices"])
                for r in sample),
            "contact_bearing_rows":sum(int(r["source_contact_impulses"])>0
                                       for r in sample),
            "A_outer_rows":sum(r["order_code"].startswith("A") for r in sample),
            "B_outer_rows":sum(r["order_code"].startswith("B") for r in sample),
        }
    by_scene=defaultdict(list);by_trial=defaultdict(list)
    by_order=defaultdict(list)
    for r in warm:
        by_scene[int(r["scene"])].append(r)
        by_trial[int(r["trial"])].append(r)
        by_order["A_outer" if r["order_code"].startswith("A") else "B_outer"].append(r)
    scenes={str(k):group_stats(v) for k,v in sorted(by_scene.items())}
    trials={str(k):group_stats(v) for k,v in sorted(by_trial.items())}
    order={k:group_stats(v) for k,v in by_order.items()}
    ar=order["A_outer"]["incumbent_over_optimized"]
    br=order["B_outer"]["incumbent_over_optimized"]
    ratios=[v["incumbent_over_optimized"] for v in trials.values()]
    return {
        "experiment":"PASS26 isolated fully paid four-arm dense-motion regeneration A/B",
        "source":"original accepted physically integrated C++23 FEM positions",
        "body_contact_note":"physical FEM is against static finite triangle patch; 3 mesh/material variants",
        "algorithm_A":"selective BVH with conservative child-AABB prechecks before VF/EE recursion",
        "algorithm_B":"otherwise identical original selective BVH with per-call VF/EE overlap checks",
        "candidate_correctness":"A1/A2/B1/B2 all independently equal original exhaustive VF/EE oracle",
        "total_rows":len(rows),
        "warm_rows":len(warm),
        "cold_rows":len(rows)-len(warm),
        "warm":group_stats(warm),
        "cold":group_stats([r for r in rows if int(r["cold_epoch"])==1]),
        "per_scene":scenes,
        "per_trial":trials,
        "per_execution_order":order,
        "order_ratio_disagreement_factor":max(ar,br)/min(ar,br),
        "all_trials_favor_optimized":all(x>1 for x in ratios),
        "all_trials_favor_incumbent":all(x<1 for x in ratios),
        "median_trial_ratio":statistics.median(ratios),
        "child_prechecks_enabled":max(int(r["child_precheck_enabled_A1"]) for r in rows)>0,
        "limitations":[
            "Strong pinned IPC not included in THIS A/B implementation-comparison; use independent same-run IPC ABBA for the winning variant",
            "Physical FEM is generated once; correlated frames and repeated queries are not independent scientific data sets",
            "No whole-solver speedup, coupled deformable/deformable response, new CCD theorem or publication novelty claim",
            "Only descriptive timing evidence: official same-run cached-input IPC must be replicated after any algorithm promotion",
        ]
    }


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    rows=load(a.input)
    report=summary(rows)
    if not report["child_prechecks_enabled"]:
        raise ValueError("alleged optimization never executed on physical FEM")
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/"pass26_native_prechecked_abba_report.json").write_text(
        json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(report,indent=2));return
    s=sorted(report["per_scene"].items())
    labels=[f"FEM scene {i}" for i,_ in s]
    ratios=[v["incumbent_over_optimized"] for _,v in s]
    fig,ax=plt.subplots(figsize=(9,5))
    ax.barh(labels,ratios)
    ax.axvline(1,ls="--",color="gray")
    ax.set(xlabel="Original selective BVH / child-overlap-prechecked BVH paid wall time",
           title="Actual integrated 3D FEM: isolated 4-arm ABBA code ablation")
    ax.grid(axis="x",alpha=.25);fig.tight_layout()
    fig.savefig(a.output/"26a_real_FEM_child_overlap_ablation.png",dpi=190)
    plt.close(fig)
    fig,ax=plt.subplots(figsize=(8,4))
    for order_name,measure in sorted(report["per_execution_order"].items()):
        ax.bar(order_name,measure["incumbent_over_optimized"])
    ax.axhline(1,ls="--",color="gray")
    ax.set(ylabel="Incumbent selective / optimized selective paid time",
           title="Original FEM: isolation of ABBA method-order effects")
    ax.grid(axis="y",alpha=.25);fig.tight_layout()
    fig.savefig(a.output/"26b_ABBA_order_stability.png",dpi=190)
    plt.close(fig)
    print(json.dumps(report,indent=2))


if __name__=="__main__":main()
