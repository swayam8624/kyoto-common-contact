#!/usr/bin/env python3
"""Post-run verified scoring for all Kyoto Phase 18 material policies.

Rereads existing CSVs and original DECISION_COVERAGE.json receipts.
No simulation, timing, ML training, or backend rerun. Strict framewise
candidate count/hash parity is checked via the frozen SAP replay scorer.
Natural motion and artificial translation are never pooled.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path
from statistics import median

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from score_replay import score
sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_kyoto_decision_coverage import check_ordered_replay

GUARD = "global_guarded_exact"
V4 = "guard_sap_bounded_trial"
SAP = "sap_persistent_coherent"


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def gm(values):
    values = list(values)
    if not values or any(not math.isfinite(x) or x <= 0 for x in values):
        raise ValueError("Invalid geometric-mean inputs")
    return math.exp(sum(math.log(x) for x in values) / len(values))


def validate_case(case, receipt, methods, warmup):
    if case["csv"] != receipt["csv"]:
        raise ValueError("CSV receipt mismatch")
    for pathkey in ("csv", "snapshot"):
        source = Path(case[pathkey])
        if not source.is_file() or sha(source) != case[pathkey + "_sha256"]:
            raise ValueError("Missing or modified immutable evidence: " + str(source))
    path = Path(case["csv"])
    scored = score(path, skip=warmup)
    if set(scored["methods"]) != methods:
        raise ValueError("Incomparable method cohorts: " + str(path))
    if check_ordered_replay(path, int(case["order_seed"])) != case["execution_order"]:
        raise ValueError("Recorded randomized method order disagrees: " + str(path))
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        needed = {"backend", "frame", "n", "seed", "chosen_backend",
                  "hybrid_probed", "hybrid_switched"}
        if not needed.issubset(reader.fieldnames or []):
            raise ValueError("Missing control-flow fields: " + str(path))
        actions = []
        for row in reader:
            if int(row["n"]) != int(case["actual_n"]) or int(row["seed"]) != int(case["seed"]):
                raise ValueError("Scene size/seed mismatch: " + str(path))
            if row["backend"] == V4:
                actions.append(row)
    actions.sort(key=lambda row: int(row["frame"]))
    frames = int(case["frames"])
    if [int(row["frame"]) for row in actions] != list(range(frames)):
        raise ValueError("Incomplete V4 frame sequence: " + str(path))
    counts = {
        "v4_guard_frames": sum(r["chosen_backend"] == GUARD for r in actions),
        "v4_sap_frames": sum(r["chosen_backend"] == SAP for r in actions),
        "trial_frames": sum(int(r["hybrid_probed"]) for r in actions),
        "switches": sum(int(r["hybrid_switched"]) for r in actions),
    }
    if counts["v4_guard_frames"] + counts["v4_sap_frames"] != frames:
        raise ValueError("Unknown V4 action")
    if any(r["hybrid_probed"] not in {"0", "1"} or
           r["hybrid_switched"] not in {"0", "1"} for r in actions):
        raise ValueError("Invalid V4 decision Boolean")
    if any(int(receipt[k]) != val for k, val in counts.items()):
        raise ValueError("Control-flow receipt changed: " + str(path))
    for k in ("type", "material", "regime", "actual_n", "seed", "order_seed"):
        if receipt[k] != case[k]:
            raise ValueError("Metadata disagree: " + k)
    times = {name: {
        "mean_ms": result["mean_ms"], "p95_ms": result["p95_ms"],
        "mean_pairs": result["mean_pairs"]
    } for name, result in scored["methods"].items()}
    ratio = times[GUARD]["mean_ms"] / times[V4]["mean_ms"]
    if not math.isclose(ratio, receipt["guard_over_v4"],
                        rel_tol=1e-8, abs_tol=1e-8):
        raise ValueError("Guard/V4 ratio disagrees with receipt")
    return {
        "source": str(path), "source_sha256": sha(path),
        "type": case["type"], "material": case["material"],
        "regime": case["regime"], "n": case["actual_n"],
        "seed": case["seed"], "frames": frames,
        "guard_over_v4": ratio, **counts, "methods": times
    }


def aggregate(rows, methods):
    if not rows:
        raise ValueError("Empty comparison cohort")
    reference = sum(r["methods"][GUARD]["mean_ms"] for r in rows)
    scores = {}
    for method in sorted(methods):
        ratios = [r["methods"][GUARD]["mean_ms"] /
                  r["methods"][method]["mean_ms"] for r in rows]
        scores[method] = {
            "guard_over_method_geomean": gm(ratios),
            "summed_scene_mean_speedup": reference /
                sum(r["methods"][method]["mean_ms"] for r in rows),
            "wins_vs_guard": sum(ratio > 1 for ratio in ratios) if method != GUARD else 0,
            "median_of_within_scene_p95_ms": median(
                r["methods"][method]["p95_ms"] for r in rows),
        }
    return {
        "trajectories": len(rows), "frames": sum(r["frames"] for r in rows),
        "v4_guard_frames": sum(r["v4_guard_frames"] for r in rows),
        "v4_sap_frames": sum(r["v4_sap_frames"] for r in rows),
        "trial_frames": sum(r["trial_frames"] for r in rows),
        "switches": sum(r["switches"] for r in rows),
        "trajectories_using_sap": sum(r["v4_sap_frames"] > 0 for r in rows),
        "v4_guard_relative_geomean": gm(r["guard_over_v4"] for r in rows),
        "v4_wins_vs_guard": sum(r["guard_over_v4"] > 1 for r in rows),
        "method_comparison": scores,
    }


def analyze(folder):
    manifest = folder / "DECISION_COVERAGE.json"
    d = json.loads(manifest.read_text(encoding="utf-8"))
    if d.get("experiment") != "Phase 18 controller-activation coverage, no threshold retuning":
        raise ValueError("Unsupported experiment type")
    if not d.get("natural_and_artificial_diagnostic_never_pooled"):
        raise ValueError("Natural and artificial source must remain separate")
    methods = set(d["tested_methods"])
    if not {"sap_rebuild", SAP, GUARD, V4, "online_cost_aware"}.issubset(methods):
        raise ValueError("Required original method absent")
    cases = d["data"]
    receipts = {r["csv"]: r for r in d["results"]}
    if len(cases) != len(d["results"]) or len(receipts) != len(cases) or \
       len({c["csv"] for c in cases}) != len(cases):
        raise ValueError("Missing or duplicate trajectory receipts")
    observations = [
        validate_case(c, receipts[c["csv"]], methods, int(d["warmup"]))
        for c in cases
    ]
    percohort = defaultdict(list)
    for row in observations:
        if row["type"] not in {"natural", "rigid_translation_diagnostic"}:
            raise ValueError("Unexpected cohort label")
        percohort[row["type"]].append(row)
    groups = {k: aggregate(rows, methods) for k, rows in sorted(percohort.items())}
    if set(groups) != set(d["groups"]):
        raise ValueError("Missing/unexpected aggregate cohort")
    for cohort, g in groups.items():
        for key in ("trajectories", "v4_guard_frames", "v4_sap_frames",
                    "trial_frames", "switches"):
            stored = "guard_frames" if key == "v4_guard_frames" else \
                     "sap_frames" if key == "v4_sap_frames" else key
            if g[key] != int(d["groups"][cohort][stored]):
                raise ValueError("Aggregate receipt mismatch: " + cohort + " " + key)
    conditions = defaultdict(list)
    for row in percohort.get("natural", []):
        conditions[(row["material"], row["regime"], row["n"])].append(row)
    stratified = []
    for (mat, regime, n), values in sorted(conditions.items()):
        stratified.append({
            "material": mat, "regime": regime, "n": n,
            "seed_values": sorted(x["seed"] for x in values),
            "guard_over_v4": gm(x["guard_over_v4"] for x in values),
            "sap_frames": sum(x["v4_sap_frames"] for x in values),
            "switches": sum(x["switches"] for x in values)
        })
    return {
        "source_manifest": str(manifest), "manifest_sha256": sha(manifest),
        "all_source_sha256_verified": True, "framewise_sap_parity_verified": True,
        "warmup_excluded": d["warmup"], "methods_actually_run": sorted(methods),
        "ml_weights_executed": d["ml_weights_executed"],
        "cohorts_never_pooled": True,
        "groups": groups, "natural_conditions": stratified,
        "individual_trajectories": observations,
        "limitations": [
            "Current discrete AABB candidate-list generation, not physical solver or CCD.",
            "Different material, regime and size conditions reuse only three seed values.",
            "Benchmarks ran once per randomized execution slot; CPU thermal/order confounding remains.",
            "Frame observations are correlated; no population confidence interval is asserted.",
            "Sum of scene means is not end-to-end wall time.",
            "p95 summary is median of individual trajectory p95, not pooled p95.",
            "Artificial global translation is a control-flow diagnostic and not physical material motion.",
            "Unexecuted trained ML weights cannot be ranked or declared a losing method."
        ]
    }


def save(report, output):
    if output.exists():
        raise FileExistsError("Refusing to overwrite " + str(output))
    output.mkdir(parents=True)
    (output / "VERIFIED_ALL_METHODS.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Kyoto Phase 18 — verified comparison of all actually executed algorithms", "",
        "No new simulation, timing, retraining or optimization: all results derive "
        "from immutable replay CSVs independently rechecked against SAP.", "",
        "Original decision JSON SHA-256: " + report["manifest_sha256"], "",
        "Warmup frames excluded per trajectory: " + str(report["warmup_excluded"]), ""
    ]
    for cohort, g in report["groups"].items():
        label = ("Natural stepped material motion" if cohort == "natural" else
                 "ARTIFICIAL rigid whole-scene translation — NOT PHYSICAL MOTION")
        lines.extend([
            "## " + label, "",
            str(g["trajectories"]) + " trajectories; " + str(g["frames"]) + " frames.",
            "V4 Guard frames: " + str(g["v4_guard_frames"]) +
            "; SAP frames: " + str(g["v4_sap_frames"]) +
            "; trials: " + str(g["trial_frames"]) +
            "; switches: " + str(g["switches"]) + ".", "",
            "V4 Guard-relative geomean: **" +
            format(g["v4_guard_relative_geomean"], ".4f") + "x**; V4 wins: " +
            str(g["v4_wins_vs_guard"]) + "/" + str(g["trajectories"]) + ".", "",
            "| Method | Guard/method geomean | Wins over Guard | Sum-of-scene-means speedup | Median within-scene p95 ms |",
            "|---|---:|---:|---:|---:|"
        ])
        for name, m in sorted(g["method_comparison"].items(),
                              key=lambda x: -x[1]["guard_over_method_geomean"]):
            lines.append("| " + name + " | " +
                         format(m["guard_over_method_geomean"], ".4f") + "x | " +
                         str(m["wins_vs_guard"]) + "/" + str(g["trajectories"]) + " | " +
                         format(m["summed_scene_mean_speedup"], ".4f") + "x | " +
                         format(m["median_of_within_scene_p95_ms"], ".5f") + " |")
        lines.append("")
    lines.extend([
        "## Natural conditions only (three seeds per condition in the default run)", "",
        "| Material | Motion | Actual N | Seed values | Guard/V4 geomean | SAP frames | Switches |",
        "|---|---|---:|---|---:|---:|---:|"
    ])
    for c in report["natural_conditions"]:
        lines.append("| " + c["material"] + " | " + c["regime"] + " | " +
                     str(c["n"]) + " | " +
                     ", ".join(map(str, c["seed_values"])) + " | " +
                     format(c["guard_over_v4"], ".4f") + "x | " +
                     str(c["sap_frames"]) + " | " + str(c["switches"]) + " |")
    lines.extend(["", "## Research limitations", ""])
    lines.extend("- " + x for x in report["limitations"])
    (output / "VERIFIED_ALL_METHODS.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--results", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    result = analyze(args.results)
    save(result, args.output)
    print("PASS: " + str(len(result["methods_actually_run"])) +
          " methods verified without rerunning the experiment")
    print(args.output / "VERIFIED_ALL_METHODS.md")


if __name__ == "__main__":
    main()
