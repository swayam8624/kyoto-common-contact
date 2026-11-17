#!/usr/bin/env python3
"""Inspect real, per-frame Guard/SAP hybrid pilot results without retuning.

Reads the existing versioned ablation contract and raw replay CSVs. It never
reruns simulations, changes model weights or changes the recorded benchmarks.
Compares paired same-snapshot timings from separate policy invocations; those
snapshots are matched but policy cache histories are not identical.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean

POLICIES = (
    "global_guarded_exact", "ml_cost_aware", "ml_cost_aware_conservative",
    "sap_persistent_coherent", "guard_sap_measured_probe",
)


def geomean(values):
    items = list(values)
    return math.exp(mean(math.log(x) for x in items)) if items else None


def nearest_quantile(values, q):
    data = sorted(values)
    if not data:
        return None
    return data[min(len(data) - 1, max(0, round((len(data) - 1) * q)))]


def inspect_scene(path, material, regime, requested_n, seed, warmup, frames):
    by_backend = defaultdict(dict)
    with path.open(newline="", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        required = {"backend", "frame", "ms", "pairs", "pair_hash", "misses",
                    "chosen_backend", "hybrid_probed", "hybrid_switched"}
        if not required.issubset(set(reader.fieldnames or [])):
            raise ValueError(f"{path}: expected hybrid ABI V3 schema")
        for row in reader:
            method, frame = row["backend"], int(row["frame"])
            if frame in by_backend[method]:
                raise ValueError(f"{path}: duplicate {method} frame={frame}")
            by_backend[method][frame] = row
    absent = set(POLICIES) - set(by_backend)
    if absent:
        raise ValueError(f"{path}: missing backends {sorted(absent)}")
    for method in POLICIES:
        if sorted(by_backend[method]) != list(range(frames)):
            raise ValueError(f"{path}: incomplete method {method} frames")
    assert frames > warmup
    for frame in range(frames):
        ref = by_backend["global_guarded_exact"][frame]
        for method, mapping in by_backend.items():
            if frame not in mapping:
                raise ValueError(f"{path}: {method} missing frame {frame}")
            row = mapping[frame]
            if ((row["pairs"], row["pair_hash"]) != (ref["pairs"], ref["pair_hash"])
                or int(row["misses"]) != 0):
                raise ValueError(f"{path}: pair oracle mismatch {method} frame {frame}")
            elapsed = float(row["ms"])
            if not math.isfinite(elapsed) or elapsed < 0:
                raise ValueError(f"{path}: invalid timing {method} frame {frame}")
    means = {method: mean(float(row["ms"]) for frame, row in mapping.items()
                          if frame >= warmup)
             for method, mapping in by_backend.items()}
    p95 = {method: nearest_quantile([float(row["ms"]) for frame, row in mapping.items()
                                           if frame >= warmup], 0.95)
           for method, mapping in by_backend.items()}
    hybrid = by_backend["guard_sap_measured_probe"]
    action_counts = Counter()
    events = Counter()
    matched_penalty = defaultdict(list)
    probe_costs = []
    for frame in range(warmup, frames):
        row = hybrid[frame]
        action = row["chosen_backend"]
        if action not in ("global_guarded_exact", "sap_persistent_coherent"):
            raise ValueError(f"{path}: invalid hybrid action {action}")
        probe = int(row["hybrid_probed"])
        switch = int(row["hybrid_switched"])
        if probe not in (0, 1) or switch not in (0, 1):
            raise ValueError(f"{path}: invalid hybrid probe/switch bit")
        action_counts[action] += 1
        events["probes"] += probe
        events["switches"] += switch
        h = float(row["ms"])
        g = float(by_backend["global_guarded_exact"][frame]["ms"])
        sap = float(by_backend["sap_persistent_coherent"][frame]["ms"])
        matched_penalty["probe" if probe else "not_probed"].append(h - g)
        matched_penalty[action + ("_probed" if probe else "_not_probed")].append(h - g)
        if probe:
            probe_costs.append((h, g, sap))
    return {
        "material": material, "regime": regime, "requested_n": requested_n,
        "actual_n": int(by_backend["global_guarded_exact"][0]["n"]),
        "seed": seed, "frames": frames, "warmup": warmup,
        "mean_ms": means, "p95_ms": p95,
        "speedup_geo_components": {
            "ml_over_guard": means["global_guarded_exact"] / means["ml_cost_aware"],
            "conservative_over_guard": means["global_guarded_exact"] / means["ml_cost_aware_conservative"],
            "persistent_sap_over_guard": means["global_guarded_exact"] / means["sap_persistent_coherent"],
            "hybrid_over_guard": means["global_guarded_exact"] / means["guard_sap_measured_probe"],
            "hybrid_over_conservative": means["ml_cost_aware_conservative"] / means["guard_sap_measured_probe"],
        },
        "p95_guard_over_hybrid": p95["global_guarded_exact"] / p95["guard_sap_measured_probe"],
        "hybrid_actions": dict(action_counts), "hybrid_events": dict(events),
        "matched_same_frame_hybrid_minus_guard_ms": {
            group: {"count": len(values), "mean": mean(values),
                    "median": nearest_quantile(values, 0.5)}
            for group, values in matched_penalty.items()
        },
        "probe_frames_measured_ms": {
            "count": len(probe_costs),
            "hybrid": mean(x[0] for x in probe_costs) if probe_costs else None,
            "separately_run_guard": mean(x[1] for x in probe_costs) if probe_costs else None,
            "separately_run_sap": mean(x[2] for x in probe_costs) if probe_costs else None,
        },
    }


def summaries(scenes):
    grouped = defaultdict(list)
    for item in scenes:
        for kind, label in (("all", "all"), ("material", item["material"]),
                            ("regime", item["regime"]), ("requested_n", str(item["requested_n"]))):
            grouped[kind, label].append(item)
    result = {}
    for (kind, label), items in grouped.items():
        outcome = {}
        for policy, ratio_key in (("ml", "ml_over_guard"),
                                  ("conservative", "conservative_over_guard"),
                                  ("persistent_sap", "persistent_sap_over_guard"),
                                  ("hybrid", "hybrid_over_guard")):
            outcome[policy + "_geo_speedup_over_guard"] = geomean(
                x["speedup_geo_components"][ratio_key] for x in items)
        outcome["hybrid_aggregate_guard_time_over_hybrid"] = (
            sum(x["mean_ms"]["global_guarded_exact"] for x in items) /
            sum(x["mean_ms"]["guard_sap_measured_probe"] for x in items)
        )
        outcome["hybrid_p95_geo_speedup_over_guard"] = geomean(
            x["p95_guard_over_hybrid"] for x in items)
        outcome["hybrid_mean_scene_wins"] = sum(
            x["speedup_geo_components"]["hybrid_over_guard"] > 1 for x in items)
        outcome["hybrid_p95_scene_wins"] = sum(x["p95_guard_over_hybrid"] > 1 for x in items)
        outcome["scenes"] = len(items)
        outcome["probes"] = sum(x["hybrid_events"].get("probes", 0) for x in items)
        outcome["switches"] = sum(x["hybrid_events"].get("switches", 0) for x in items)
        outcome["hybrid_guard_frames"] = sum(x["hybrid_actions"].get("global_guarded_exact",0) for x in items)
        outcome["hybrid_sap_frames"] = sum(x["hybrid_actions"].get("sap_persistent_coherent",0) for x in items)
        result.setdefault(kind, {})[label] = outcome
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--run", required=True, type=Path,
                    help="Read-only input from completed balanced ablation")
    ap.add_argument("--output", required=True, type=Path,
                    help="New diagnostics JSON, not an existing experiment file")
    args = ap.parse_args()
    contract = json.loads((args.run / "ABLATION_CONTRACT.json").read_text(encoding="utf-8"))
    if contract.get("replayer_abi") != "KYOTO_REPLAY_ABLATION_V3":
        ap.error("Expected hybrid V3 replay ABI, refusing to analyze old experiment")
    if args.output.exists():
        ap.error(f"Refusing to overwrite an existing file: {args.output}")
    root = args.run / "replays"
    scenes = []
    for seed, material, regime, budget in contract["combos"]:
        filename = f"actual-{material}-{regime}-n{budget}-seed{seed}-evaluated.csv"
        path = root / filename
        if not path.is_file():
            ap.error(f"Missing complete replay CSV: {path}")
        scenes.append(inspect_scene(path,material,regime,budget,seed,
                                    contract["warmup"],contract["frames"]))
    result = {
        "methodology": "read-only aggregate of actual per-frame same-snapshot separate-policy replay",
        "warnings": [
            "Measurements from separately run backends have independent cache histories.",
            "Mean differences from paired frame timings are observational, not isolated causal overhead.",
            "This observed set must no longer be used as an untouched test set for parameter tuning.",
            "Balanced 108-cell matrix has one seed per material/regime/size cell, not four repeats per cell.",
        ],
        "contract": {"replayer_abi": contract["replayer_abi"],
                     "seeds": contract["seeds"], "sampling": contract.get("sampling"),
                     "scenes": len(scenes), "frames": contract["frames"],
                     "warmup": contract["warmup"]},
        "groups": summaries(scenes),
        "best_hybrid_scenes": sorted(scenes,key=lambda item: item["speedup_geo_components"]["hybrid_over_guard"],reverse=True)[:12],
        "worst_hybrid_scenes": sorted(scenes,key=lambda item: item["speedup_geo_components"]["hybrid_over_guard"])[:12],
        "all_scenes": scenes,
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print("[DIAGNOSE] wrote",args.output)
    for method in ("ml", "conservative", "persistent_sap", "hybrid"):
        print("[DIAGNOSE]",method,"geo speedup vs Guard:",
              f'{result["groups"]["all"]["all"][method + "_geo_speedup_over_guard"]:.4f}x')
    print("[DIAGNOSE] hybrid full-time aggregated ratio:",
          f'{result["groups"]["all"]["all"]["hybrid_aggregate_guard_time_over_hybrid"]:.4f}x')


if __name__ == "__main__":
    main()
