#!/usr/bin/env python3
"""Source-verified all-policy scoring of NEW independently seeded Kyoto meshes.

Postprocessing only. Reads the held-out suite's original immutable
KYOTOM01 snapshot and full-pair CPU replay CSV SHA-256 receipts. Checks all
candidate sets against SAP on every single frame; does not re-export source
geometry, re-run a native timing benchmark, train an ML model or modify V4.
Outputs actual all-method ranking, by material/budget, and measured V4
Guard/SAP/trial/switch decision counts. No population CI is invented.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"benchmarks"))
from score_replay import score
sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import sha256, snapshot_metadata, check_ordered_replay


def geomean(ratios):
    vals=list(ratios)
    if not vals or any(not math.isfinite(x) or x<=0 for x in vals):
        raise ValueError("Invalid geometric-mean ratios")
    return math.exp(mean(math.log(x) for x in vals))


def action_stats(path):
    actions=[]
    with path.open(newline="",encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["backend"] == "guard_sap_bounded_trial":
                actions.append(row)
    actions.sort(key=lambda row:int(row["frame"]))
    if [int(r["frame"]) for r in actions] != list(range(len(actions))):
        raise ValueError("V4 decision observations have missing frames")
    chosen={r["chosen_backend"] for r in actions}
    if not chosen.issubset({"global_guarded_exact","sap_persistent_coherent"}):
        raise ValueError("Unknown frozen V4 decision backend")
    return {
        "frames":len(actions),
        "guard_frames":sum(r["chosen_backend"]=="global_guarded_exact" for r in actions),
        "sap_frames":sum(r["chosen_backend"]=="sap_persistent_coherent" for r in actions),
        "trial_frames":sum(int(r["hybrid_probed"]) for r in actions),
        "switches":sum(int(r["hybrid_switched"]) for r in actions)
    }


def verify_and_score(root):
    manifest=root/"HELDOUT_RESULTS.json"
    d=json.loads(manifest.read_text(encoding="utf-8"))
    if d.get("experiment") != "Kyoto independent geometry and topology plus coupled solver audit":
        raise ValueError("Not a held-out Kyoto independent topology manifest")
    if d.get("frozen_v4_unchanged") is not True or d.get(
            "historical_phase18_inputs_untouched") is not True:
        raise ValueError("Frozen controls not preserved in source manifest")
    results=[]
    all_names=None
    for item in d["material_replays"]:
        snapshot=Path(item["snapshot"])
        replay=Path(item["replay"])
        if not snapshot.is_file() or sha256(snapshot)!=item["snapshot_sha256"]:
            raise ValueError("Missing/modified source KYOTOM01: "+str(snapshot))
        if not replay.is_file() or sha256(replay)!=item["replay_sha256"]:
            raise ValueError("Missing/modified native full-pair replay: "+str(replay))
        hdr=snapshot_metadata(snapshot)
        if hdr["n"]!=item["actual_n"] or hdr["seed"]!=item["motion"]:
            raise ValueError("Source header differs from manifest")
        check_ordered_replay(replay,int(item["order_seed"]))
        with replay.open(newline="", encoding="utf-8") as f:
            rows=list(csv.DictReader(f))
        # The scorer audits EVERY method's full pair count/hash against
        # the same SAP reference on every frame, not merely sampled frames.
        default_warmup=8
        # Prefer a declared warmup from manifest, else require explicit
        # caller choice before analysis; main() overrides this parameter.
        results.append((item,hdr,replay,rows))
    if len(results)!=d["new_varied_material_scenes"]:
        raise ValueError("Missing original independent source scenes")
    return d,results,manifest


def analyze(root,warmup):
    d,raw,manifest=verify_and_score(root)
    scenes=[]
    cohort_methods=None
    for item,hdr,path,rows in raw:
        if not 0<=warmup<hdr["frames"]:
            raise ValueError("Invalid warmup for original held-out frame stream")
        scored=score(path,skip=warmup)
        ms={k:x["mean_ms"] for k,x in scored["methods"].items()}
        methods=set(ms)
        if cohort_methods is None:cohort_methods=methods
        if methods!=cohort_methods:
            raise ValueError("Different actual method cohorts across scenes")
        if methods!=set(item["methods_actually_executed"]):
            raise ValueError("Source-reported policy list differs from CSV")
        if not {"global_guarded_exact","guard_sap_bounded_trial","sap_rebuild",
                "online_cost_aware","sap_persistent_coherent"}.issubset(methods):
            raise ValueError("Original minimum algorithm controls absent")
        selected=action_stats(path)
        if selected["frames"]!=hdr["frames"] or selected["guard_frames"]+selected["sap_frames"]!=hdr["frames"]:
            raise ValueError("Actual on-policy V4 backend counts do not cover frames")
        ref=ms["global_guarded_exact"]
        scene={
            "material":item["material"],"regime":item["regime"],
            "requested_budget":item["budget"],"actual_n":hdr["n"],
            "motion_seed":item["motion"],"geometry_seed":item["geometry"],
            "topology_seed":item["topology"],
            "snapshot_sha256":item["snapshot_sha256"],
            "replay_sha256":item["replay_sha256"],
            "measured_frames":hdr["frames"]-warmup,
            "all_policy_means_ms":ms,
            "guard_over_method":{k:ref/x for k,x in ms.items()},
            "v4_decisions_all_frames":selected,
            "exact_current_sap_pair_hashes_verified":True
        }
        scenes.append(scene)
    bymethod=[]
    for name in cohort_methods:
        bymethod.append({
            "method":name,
            "guard_over_method_geomean":geomean(s["guard_over_method"][name] for s in scenes),
            "wins_over_guard":sum(s["guard_over_method"][name]>1 for s in scenes),
            "scene_count":len(scenes),
            "sum_scene_mean_speedup":(
                sum(s["all_policy_means_ms"]["global_guarded_exact"] for s in scenes) /
                sum(s["all_policy_means_ms"][name] for s in scenes)
            )
        })
    bymethod.sort(key=lambda x:x["guard_over_method_geomean"],reverse=True)
    group=defaultdict(list)
    for s in scenes:
        group[(s["material"],s["regime"],s["actual_n"])].append(s)
    conditions=[]
    for (mat,reg,n),subset in sorted(group.items()):
        choices={key:sum(s["v4_decisions_all_frames"][key] for s in subset)
                 for key in ("guard_frames","sap_frames","trial_frames","switches")}
        conditions.append({
            "material":mat,"regime":reg,"actual_n":n,
            "distinct_seeded_geometry_scenes":len(subset),
            "guard_over_v4_geomean":geomean(
                s["guard_over_method"]["guard_sap_bounded_trial"] for s in subset),
            "v4_decisions_all_frames":choices
        })
    return {
        "experiment":"Held-out Kyoto complete original-policy exact AABB source reanalysis",
        "source":str(manifest),"source_sha256":sha256(manifest),
        "original_policy_replay_csvs_reverified":len(scenes),
        "native_benchmarks_rerun_by_this_script":0,
        "new_scenes":len(scenes),"warmup_excluded":warmup,
        "all_method_scores":bymethod,
        "physical_scene_conditions":conditions,
        "on_policy_v4_total_decisions":{key:sum(s["v4_decisions_all_frames"][key] for s in scenes)
           for key in ("guard_frames","sap_frames","trial_frames","switches")},
        "individual_scenes":scenes,
        "limitations":[
            "AABB candidate generation only; not a timed coupled solver comparison",
            "All methods are executed in different randomized whole-trajectory order slots, not an unbiased interleaved ABBA experiment",
            "Every scene varies geometry and topology within an existing meshed material family; no arbitrary meshes or calibrated physical sources",
            "No model weights were executed unless explicitly present among original method names; do not add inferred ML baselines",
            "No confidence interval on unseen geometry or physics claimed",
            "Contact-resolution and solver-geometry parity are assessed in a separate coupled experiment",
            "Sum-of-scene-means speedup is not total wall time"
        ]
    }


def write(report,out):
    if out.exists():
        raise FileExistsError("Refusing to overwrite scored independent evidence "+str(out))
    out.mkdir(parents=True,exist_ok=False)
    (out/"HELDOUT_ALL_METHODS.json").write_text(
        json.dumps(report,indent=2)+"\n",encoding="utf-8")
    lines=[
        "# Kyoto new independently seeded geometry — full original-policy comparison",
        "",
        "No new simulations, native timings or model training. Immutable original",
        "source SHA-256 and framewise SAP count/hash parity verified.",
        "",
        "## Natural material fixture AABB replay only",
        "",
        "| Actually executed method | Guard/method geomean | Scene wins vs Guard | Sum of scene-means speedup |",
        "|---|---:|---:|---:|"
    ]
    for d in report["all_method_scores"]:
        lines.append("| "+d["method"]+" | "+
                     format(d["guard_over_method_geomean"],".4f")+"x | "+
                     str(d["wins_over_guard"])+"/"+str(d["scene_count"])+" | "+
                     format(d["sum_scene_mean_speedup"],".4f")+"x |")
    total=report["on_policy_v4_total_decisions"]
    lines += ["","## Frozen V4 measured on-policy choices, not inferred activation",
              "",
              "Guard frames: "+str(total["guard_frames"])+
              "; SAP frames: "+str(total["sap_frames"])+
              "; trial frames: "+str(total["trial_frames"])+
              "; switches: "+str(total["switches"])+".",
              "",
              "## Independently varied geometry/topology conditions","",
              "| Material | Motion | Actual N | Distinct initial meshes | Guard/V4 geomean | SAP action frames | Trials | Switches |",
              "|---|---|---:|---:|---:|---:|---:|---:|"]
    for d in report["physical_scene_conditions"]:
        a=d["v4_decisions_all_frames"]
        lines.append("| "+d["material"]+" | "+d["regime"]+
                     " | "+str(d["actual_n"])+
                     " | "+str(d["distinct_seeded_geometry_scenes"])+
                     " | "+format(d["guard_over_v4_geomean"],".4f")+"x | "+
                     str(a["sap_frames"])+" | "+str(a["trial_frames"])+" | "+
                     str(a["switches"])+" |")
    lines += ["","## Evidence boundaries",""]+["- "+x for x in report["limitations"]]
    (out/"HELDOUT_ALL_METHODS.md").write_text(
        "\n".join(lines)+"\n",encoding="utf-8")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--results",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--warmup",type=int,default=8)
    a=p.parse_args()
    report=analyze(a.results,a.warmup)
    write(report,a.output)
    print("PASS: "+str(report["original_policy_replay_csvs_reverified"])+
          " independently seeded full original-method CSVs reverified with SAP, no reruns")
    print(a.output/"HELDOUT_ALL_METHODS.md")


if __name__=="__main__":
    main()
