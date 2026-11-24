#!/usr/bin/env python3
"""Pass24: validate real FEM stage accounting from optional C++ wall clocks.

All native observations are source-backed and scientifically limited:
- Instrumentation changes the measured program's timing.
- The five stage clocks partition the same instrumented query.
- This is NOT an independently paired official IPC speed benchmark.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

STAGES=(
    "topology_signature_ns",
    "swept_feature_boxes_ns",
    "cold_build_or_warm_refit_ns",
    "vf_ee_traversal_ns",
    "candidate_normalize_ns",
)
FIELDS=("source_kind","trial","scene","regime","step","variant",
        "source_tetrahedra","source_vertices","source_impulses",
        "source_candidate_pairs","cold_epoch","dirty_vertices",
        *STAGES,"total_ns")


def load(path:Path)->list[dict]:
    with path.open(encoding="utf-8",newline="") as source:
        reader=csv.DictReader(source)
        if set(FIELDS)-set(reader.fieldnames or ()):
            raise ValueError("missing native stage-clock fields")
        rows=list(reader)
    if not rows:raise ValueError("no native FEM stage receipts")
    keys=set()
    groups=defaultdict(set)
    cold=defaultdict(int)
    frames=defaultdict(set)
    for row in rows:
        if row["source_kind"]!="original_solver_integrated_FEM":
            raise ValueError("unrecognized physical source provenance")
        if row["regime"] not in ("routine","fast_adaptive"):
            raise ValueError("unrecognized real physics regime")
        if row["variant"] not in ("full","selective"):
            raise ValueError("unrecognized hierarchy algorithm")
        for col in FIELDS:
            if col in ("source_kind","regime","variant"):continue
            value=float(row[col])
            if not math.isfinite(value) or value<0:
                raise ValueError(f"invalid native field {col}")
        observed=sum(int(row[k]) for k in STAGES)
        if int(row["total_ns"])<=0 or observed!=int(row["total_ns"]):
            raise ValueError("native stage clocks do not reconcile to full query")
        if int(row["cold_epoch"]) not in (0,1):
            raise ValueError("cold source marker invalid")
        if int(row["dirty_vertices"])>int(row["source_vertices"]):
            raise ValueError("dirty moving source exceeds original mesh")
        key=(int(row["trial"]),int(row["scene"]),int(row["step"]),row["variant"])
        if key in keys:raise ValueError(f"duplicate native source stage {key}")
        keys.add(key)
        rowkey=key[:3]
        groups[rowkey].add(key[3])
        scene_trial=(key[0],key[1],key[3])
        frames[scene_trial].add(key[2])
        cold[scene_trial]+=int(row["cold_epoch"])
    if any(v!={"full","selective"} for v in groups.values()):
        raise ValueError("each real source interval must have both methods")
    trials={k[0] for k in groups}
    scenes={k[1] for k in groups}
    if len(trials)<2 or len(scenes)<3:
        raise ValueError("insufficient repeated source and mesh coverage")
    for tr in trials:
        for scene in scenes:
            for variant in ("full","selective"):
                if cold[(tr,scene,variant)]!=1:
                    raise ValueError("missing/duplicated cold source stage epoch")
                if frames[(tr,scene,variant)]!=frames[(min(trials),scene,"full")]:
                    raise ValueError("unequal physical interval coverage")
    return rows


def split(data:list[dict])->dict:
    total=sum(int(row["total_ns"]) for row in data)
    stages={k:sum(int(row[k]) for row in data) for k in STAGES}
    return {
        "paid_stage_total_ns":total,
        "native_source_rows":len(data),
        "mean_dirty_fraction":statistics.mean(
            int(row["dirty_vertices"])/int(row["source_vertices"])
            for row in data),
        "candidate_pairs_mean":statistics.mean(
            int(row["source_candidate_pairs"]) for row in data),
        "stage_total_ns":stages,
        "stage_fraction":{k:v/total for k,v in stages.items()},
        "largest_stage":max(stages,key=stages.get)
    }


def summary(rows:list[dict])->dict:
    warm=[r for r in rows if int(r["cold_epoch"])==0]
    cold=[r for r in rows if int(r["cold_epoch"])==1]
    if not warm:raise ValueError("no warm native source")
    by_variant=defaultdict(list)
    by_scene=defaultdict(list)
    by_regime=defaultdict(list)
    by_trial=defaultdict(list)
    for row in warm:
        by_variant[row["variant"]].append(row)
        by_scene[(int(row["scene"]),row["variant"])].append(row)
        by_regime[(row["regime"],row["variant"])].append(row)
        by_trial[(int(row["trial"]),row["variant"])].append(row)
    return {
        "study":"original integrated 3D FEM — stage cost partition of maintained BVH",
        "candidate_contract":"each native original full and selective result exactly equals independent exhaustive VF/EE source",
        "input":"original physical FEM plus accepted 20 m/s CCD leaves",
        "warm_rows":len(warm),"cold_rows":len(cold),
        "by_variant":{k:split(v) for k,v in by_variant.items()},
        "by_scene":{f"{scene}:{variant}":split(v)
                    for (scene,variant),v in sorted(by_scene.items())},
        "by_regime":{f"{kind}:{variant}":split(v)
                     for (kind,variant),v in sorted(by_regime.items())},
        "by_trial":{f"{trial}:{variant}":split(v)
                    for (trial,variant),v in sorted(by_trial.items())},
        "critical_limits":[
            "Stage clocks are an opt-in INSTRUMENTED method. Readout overhead is not zero.",
            "No stage times may be compared directly to a separate uninstrumented cached-input IPC benchmark.",
            "FEM solver physics occurs outside stage timers; neither candidate nor solver speedup follows.",
            "Four source repeats and adjacent time frames are correlated; stage fractions are diagnostic.",
            "Disjoint stage sums are checked exactly on every original native C++ source row."
        ]
    }


def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument("--input",required=True,type=Path)
    a.add_argument("--output",required=True,type=Path)
    args=a.parse_args()
    rows=load(args.input)
    report=summary(rows)
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/"pass24_real_FEM_stage_profile.json").write_text(
        json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(report,indent=2))
        return
    fig,ax=plt.subplots(figsize=(10,5))
    for index,variant in enumerate(("full","selective")):
        offsets=0.
        measured=report["by_variant"][variant]["stage_fraction"]
        for stage in STAGES:
            share=measured[stage]
            ax.barh(index,share,left=offsets,label=stage if index==0 else "")
            offsets+=share
    ax.set_yticks([0,1],["Full maintained BVH","Selective maintained BVH"])
    ax.set(xlabel="Fraction of original FEM instrumented paid query",
           title="Pass24 stage attribution: topology, swept boxes, refit, VF/EE and sort")
    ax.legend(loc="center left",bbox_to_anchor=(1,0.5),fontsize=8)
    fig.tight_layout()
    fig.savefig(args.output/"24a_3D_FEM_paid_BVH_stage_shares.png",
                dpi=200,bbox_inches="tight")
    plt.close(fig)
    print(json.dumps(report,indent=2))


if __name__=="__main__":main()
