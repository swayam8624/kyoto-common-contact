#!/usr/bin/env python3
"""Compare independently VERIFIED compact Kyoto native traces across machines.

This is cross-platform SOURCE and ACTION checking, not a statistical
throughput equivalence or a new experiment. Input traces are produced
by export_kyoto_heldout_trace.py from immutable native files.

When source snapshots and pair fingerprints disagree, distinguish that
from a controller decision difference. If policy cohorts/slots differ,
never treat the timing delta as an unbiased comparison.
"""
from __future__ import annotations

import argparse
import gzip
import json
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean

SCHEMA = "kyoto-heldout-source-verified-trace-v1"


def load(path: Path) -> dict:
    with gzip.open(path, "rt", encoding="utf-8") as fd:
        d = json.load(fd)
    if d.get("schema") != SCHEMA or not d.get("scenes"):
        raise ValueError("Unsupported or empty Kyoto per-frame evidence trace")
    if len(d["scenes"]) != d.get("original_scene_count"):
        raise ValueError("Compact source trace's declared scene count is incorrect")
    return d


def scene_key(s: dict) -> tuple:
    return (s["material"], s["regime"], s["actual_n"],
            s["motion_seed"], s["geometry_seed"], s["topology_seed"])


def by_key(report: dict) -> dict[tuple, dict]:
    seen = {}
    for scene in report["scenes"]:
        key = scene_key(scene)
        if key in seen:
            raise ValueError("Source trace duplicates original scene key")
        if scene["frames"] != len(scene["observations"]) or scene["frames"] <= 0:
            raise ValueError("Source trace frame count is invalid")
        if [r["f"] for r in scene["observations"]] != list(range(scene["frames"])):
            raise ValueError("Source trace frame chronology is incomplete")
        seen[key] = scene
    return seen


def geomean(values: list[float]) -> float:
    if not values or any(x <= 0 or not math.isfinite(x) for x in values):
        raise ValueError("Invalid cross-platform positive query time ratios")
    return math.exp(mean(math.log(x) for x in values))


def compare(a: dict, b: dict, skip: int) -> dict:
    aa, bb = by_key(a), by_key(b)
    if aa.keys() != bb.keys():
        raise ValueError("Mac/Linux source cohort mismatch: same seeds/materials required")
    decisions = 0
    mismatches = []
    source_hash_matches = 0
    pair_hash_matches = 0
    pairs = 0
    matches = 0
    method_same = 0
    slot_same = 0
    guard_ratios_a, guard_ratios_b = [], []
    by_condition = defaultdict(lambda: {
        "scenes": 0, "action_disagreements": 0,
        "sap_a": 0, "sap_b": 0, "trial_a": 0, "trial_b": 0
    })
    for key in sorted(aa):
        left, right = aa[key], bb[key]
        if left["frames"] != right["frames"]:
            raise ValueError("Matched source has unequal temporal horizon")
        n = left["frames"]
        if skip < 0 or skip >= n:
            raise ValueError("Invalid common warmup")
        source_hash_matches += int(left["source_snapshot_sha256"] ==
                                   right["source_snapshot_sha256"])
        method_same += int(set(left["ordered_slots"]) ==
                           set(right["ordered_slots"]))
        slot_same += int(left["ordered_slots"] == right["ordered_slots"])
        c = by_condition[(left["material"], left["regime"], left["actual_n"])]
        c["scenes"] += 1
        ta, tb = [], []
        ga, gb = [], []
        for frame in range(n):
            x, y = left["observations"][frame], right["observations"][frame]
            pairs += 1
            pair_hash_matches += int(
                (x["pairs"], x["pair_hash"]) == (y["pairs"], y["pair_hash"]))
            action_a = (x["v4_backend"], int(x["v4_trial"]), int(x["v4_switch"]))
            action_b = (y["v4_backend"], int(y["v4_trial"]), int(y["v4_switch"]))
            matches += int(action_a == action_b)
            if action_a != action_b:
                c["action_disagreements"] += 1
                if len(mismatches) < 40:
                    mismatches.append({
                        "scene": list(key), "frame": frame,
                        "a": action_a, "b": action_b
                    })
            c["sap_a"] += int(x["v4_backend"] == "sap_persistent_coherent")
            c["sap_b"] += int(y["v4_backend"] == "sap_persistent_coherent")
            c["trial_a"] += int(x["v4_trial"])
            c["trial_b"] += int(y["v4_trial"])
            decisions += 1
            if frame >= skip:
                ga.append(float(x["guard_ms"]))
                gb.append(float(y["guard_ms"]))
                ta.append(float(x["v4_ms"]))
                tb.append(float(y["v4_ms"]))
        guard_ratios_a.append(mean(ga) / mean(ta))
        guard_ratios_b.append(mean(gb) / mean(tb))
    return {
        "schema": "kyoto-heldout-crossplatform-comparison-v1",
        "scene_count": len(aa), "frames_per_scene": sorted({
            x["frames"] for x in aa.values()}),
        "warmup": skip, "frame_pairs_compared": pairs,
        "original_snapshots_byte_identical": source_hash_matches == len(aa),
        "snapshot_sha_matches": source_hash_matches,
        "exact_pair_hash_matches": pair_hash_matches,
        "policy_method_cohort_matches": method_same,
        "policy_execution_slot_map_matches": slot_same,
        "v4_action_frame_matches": matches,
        "v4_action_frame_disagreements": decisions - matches,
        "first_action_disagreements": mismatches,
        "guard_over_v4_geomean_a": geomean(guard_ratios_a),
        "guard_over_v4_geomean_b": geomean(guard_ratios_b),
        "condition_action_summaries": [
            {"material": k[0], "regime": k[1], "actual_n": k[2], **v}
            for k, v in sorted(by_condition.items())
        ],
        "limitations": [
            "These compressed traces were source verified during export, not reverified here against the original native bytes",
            "Identical aggregate action counts can conceal per-frame disagreements",
            "Equal procedural scene keys do not establish exact native source bytes unless SHA-256 matches",
            "Even identical input snapshots do not remove CPU scheduling, compiler, cache, thermal and policy slot biases",
            "Timing figures are descriptive per-scene query ratio geomeans, not population confidence intervals or solver physics speedups",
            "Every comparison is original discrete AABB candidate generation, not CCD or independent contact penetration ground truth"
        ]
    }


def write(result: dict, directory: Path) -> None:
    if directory.exists():
        raise FileExistsError("Refusing to overwrite original cross-platform comparison")
    directory.mkdir(parents=True, exist_ok=False)
    (directory / "CROSSPLATFORM_TRACE_COMPARISON.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Kyoto native V4 exact per-frame platform trace comparison",
        "",
        "This report compares source-verified native original policy traces;",
        "it executes no new native benchmarks and does not modify either source.",
        "",
        f"Matched original scenes: {result['scene_count']}; "
        f"source frames: {result['frame_pairs_compared']}; warmup: {result['warmup']}.",
        "",
        f"Byte-identical source snapshot SHA-256 receipts: "
        f"{result['snapshot_sha_matches']}/{result['scene_count']}.",
        f"Matching same-frame SAP candidate count/hash pairs: "
        f"{result['exact_pair_hash_matches']}/{result['frame_pairs_compared']}.",
        f"Matching exact V4 action/trial/switch decisions: "
        f"{result['v4_action_frame_matches']}/{result['frame_pairs_compared']}.",
        f"Matching per-scene method cohorts: "
        f"{result['policy_method_cohort_matches']}/{result['scene_count']}.",
        f"Matching execution slot maps: "
        f"{result['policy_execution_slot_map_matches']}/{result['scene_count']}.",
        "",
        "Guard/V4 per-scene query-time geomean for trace A: "
        f"{result['guard_over_v4_geomean_a']:.4f}x; "
        "for trace B: "
        f"{result['guard_over_v4_geomean_b']:.4f}x.",
        "",
        "## Independent source-condition action comparisons", "",
        "| Material | Regime | Actual N | Scenes | Mac/trace A SAP | Linux/trace B SAP | Trials A/B | Action disagreements |",
        "|---|---|---:|---:|---:|---:|---:|---:|"
    ]
    for v in result["condition_action_summaries"]:
        lines.append(
            f"| {v['material']} | {v['regime']} | {v['actual_n']} | "
            f"{v['scenes']} | {v['sap_a']} | {v['sap_b']} | "
            f"{v['trial_a']}/{v['trial_b']} | {v['action_disagreements']} |"
        )
    lines += ["", "## Boundaries", ""] + [
        "- " + x for x in result["limitations"]
    ]
    (directory / "CROSSPLATFORM_TRACE_COMPARISON.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--a", required=True, type=Path,
                        help="First native trace, for example Apple Silicon")
    parser.add_argument("--b", required=True, type=Path,
                        help="Second trace, for example Ubuntu GitHub CI")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--warmup", type=int, default=8)
    args = parser.parse_args()
    result = compare(load(args.a), load(args.b), args.warmup)
    write(result, args.output)
    print("PASS: compared original frozen selector action sequences "
          "frame by frame WITHOUT native benchmark reruns")
    print("Source SHA matches", result["snapshot_sha_matches"], "of",
          result["scene_count"], "V4 action/frame matches",
          result["v4_action_frame_matches"], "of",
          result["frame_pairs_compared"])
    print(args.output / "CROSSPLATFORM_TRACE_COMPARISON.md")


if __name__ == "__main__":
    main()
